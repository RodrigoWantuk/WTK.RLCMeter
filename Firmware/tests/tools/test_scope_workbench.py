"""Actual headless Qt offline import/render/export; no device activity permitted."""
import json
import os
from pathlib import Path
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch

os.environ.setdefault("QT_QPA_PLATFORM","offscreen")
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"tools"))
import scope_synthetic
import scope_waveform as scope
import pc_bringup
try:
    from PySide6.QtWidgets import QApplication
    from PySide6.QtCore import QObject, Slot, QThread, QTimer, Qt
    from PySide6.QtTest import QTest
    import scope_workbench as ui
    import pc_osl_workbench as desktop
    QT=True
except ImportError:
    QT=False


@unittest.skipUnless(QT,"install pinned workbench requirements for real Qt tests")
class ScopeGuiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): cls.app=QApplication.instance() or QApplication([])

    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name); self.tab=ui.ScopeTab(); self.tab.resize(1200,850); self.tab.show()
        self.addCleanup(self.close)
        self.path,self.config,self.preset,self.params=scope_synthetic.generate(self.root/"input")
        self.configure()

    def pump(self,predicate,timeout=20):
        deadline=time.monotonic()+timeout
        while not predicate():
            self.app.processEvents(); time.sleep(.001)
            if time.monotonic()>deadline: self.fail("GUI worker timeout")
        self.app.processEvents()

    def close(self):
        self.tab.shutdown(); self.pump(lambda:not self.tab.thread.isRunning()); self.tab.close(); self.tab.deleteLater()

    def configure(self):
        self.tab.path.setText(str(self.path)); self.tab.preset.setCurrentText(self.preset)
        self.tab.instrument.setText("SYNTHETIC"); self.tab.operator.setText("TEST ONLY"); self.tab.date.setText("SYNTHETIC")
        self.tab.simultaneous.setChecked(True)
        for f,c in zip(self.tab.channels,self.config.channels):
            f["net"].setText(c.net); f["probe"].setText(str(c.probe_attenuation)); f["coupling"].setCurrentText(c.coupling)
            f["bandwidth"].setText(str(c.bandwidth_hz))

    def imported(self):
        QTest.mouseClick(self.tab.import_button,Qt.LeftButton); self.pump(lambda:not self.tab.busy)
        self.assertIsNotNone(self.tab.report,self.tab.status.text())

    def test_startup_no_evidence_or_export(self):
        self.assertIsNone(self.tab.report); self.assertFalse(self.tab.export_button.isEnabled())
        self.assertTrue(self.tab.synthetic.isChecked())
        self.assertIn("Never float",scope.SAFETY)

    def test_form_mapping_numeric_plot(self):
        self.imported(); self.assertEqual(self.tab.report["samples"],2000)
        self.assertEqual(self.tab.report["evidence_state"],"SYNTHETIC")
        self.assertEqual(len(self.tab.plot.traces),3)
        self.assertTrue(self.tab.export_button.isEnabled())
        self.assertIn("VEXC_DIFF",self.tab.metrics.toPlainText())
        self.assertAlmostEqual(self.tab.report["analysis"]["fundamental"]["fit"]["peak_v"],.5,places=9)

    def test_zoom_and_full_trace_reuses_original_samples(self):
        self.imported(); self.tab.start.setText("0.002"); self.tab.end.setText("0.004")
        QTest.mouseClick(self.tab.zoom_button,Qt.LeftButton); self.pump(lambda:not self.tab.busy)
        for trace in self.tab.plot.traces:
            self.assertGreaterEqual(min(p[0] for p in trace["points"]),.002)
            self.assertLessEqual(max(p[0] for p in trace["points"]),.004)
        QTest.mouseClick(self.tab.full_button,Qt.LeftButton); self.pump(lambda:not self.tab.busy)
        self.assertAlmostEqual(self.tab.plot.traces[0]["points"][0][0],0)

    def test_rejected_import_clears_stale_evidence(self):
        self.imported(); self.tab.profile.setCurrentText("hantek_dso2c10")
        self.tab.import_file(); self.pump(lambda:not self.tab.busy)
        self.assertIsNone(self.tab.report); self.assertFalse(self.tab.export_button.isEnabled()); self.assertFalse(self.tab.plot.traces)
        self.assertIn("UNSUPPORTED",self.tab.status.text())

    def test_missing_probe_does_not_default(self):
        self.tab.channels[0]["probe"].clear(); self.tab.import_file()
        self.assertFalse(self.tab.busy); self.assertIsNone(self.tab.report); self.assertIn("REJECTED",self.tab.status.text())

    def test_select_one_channel_basic(self):
        self.tab.channels[1]["enabled"].setChecked(False); self.tab.preset.setCurrentText("basic")
        self.imported(); self.assertEqual(len(self.tab.report["channels"]),1)

    def test_settings_form_roundtrip_preserves_unknown_and_probe(self):
        self.tab.apply_settings(self.config,self.preset,self.params)
        self.assertEqual(self.tab.config().channels,self.config.channels)
        self.assertTrue(self.tab.config().metadata["synthetic"])
        self.assertEqual(self.tab.parameters(),self.params)
        self.assertEqual(self.tab.config().metadata["trigger_acquisition"],"UNKNOWN")

    def test_settings_file_load_without_acquisition(self):
        with patch.object(ui.QFileDialog,"getOpenFileName",return_value=(str(self.path.with_name("config.json")),"")):
            self.tab.load_settings()
        self.assertFalse(self.tab.busy); self.assertIsNone(self.tab.report)
        self.assertEqual(self.tab.path.text(),str(self.path))
        self.assertEqual(self.tab.config().channels,self.config.channels)

    def test_settings_save_validates_without_default_scale(self):
        output=self.root/"saved.json"
        self.tab.channels[0]["probe"].clear()
        with patch.object(ui.QFileDialog,"getSaveFileName",return_value=(str(output),"")): self.tab.save_settings()
        self.assertFalse(output.exists()); self.assertIn("REJECTED",self.tab.status.text())

    def test_no_simultaneity_differential_rejected(self):
        self.tab.simultaneous.setChecked(False); self.tab.import_file(); self.pump(lambda:not self.tab.busy)
        self.assertIn("simultaneous",self.tab.status.text()); self.assertFalse(self.tab.export_button.isEnabled())

    def test_duplicate_operations_no_queue(self):
        entered=threading.Event(); release=threading.Event(); original=scope.import_csv
        def held(*args,**kwargs): entered.set(); release.wait(5); return original(*args,**kwargs)
        with patch.object(scope,"import_csv",held):
            self.tab.import_file(); self.pump(entered.is_set)
            self.assertFalse(self.tab.submit("import")); self.assertFalse(self.tab.import_button.isEnabled())
            self.assertFalse(self.tab.export_button.isEnabled()); release.set(); self.pump(lambda:not self.tab.busy)

    def test_responsive_timer_and_gui_thread_delivery(self):
        entered=threading.Event(); release=threading.Event(); original=scope.import_csv; delivered=[]
        def held(*args,**kwargs): entered.set(); release.wait(5); return original(*args,**kwargs)
        timer=QTimer(); timer.setInterval(2); ticks=[]; timer.timeout.connect(lambda:ticks.append(1)); timer.start()
        class Receiver(QObject):
            @Slot(object)
            def receive(self,result): delivered.append(QThread.currentThread())
        receiver=Receiver()
        self.tab.worker.done.connect(receiver.receive,Qt.QueuedConnection)
        with patch.object(scope,"import_csv",held):
            self.tab.import_file(); self.pump(lambda:entered.is_set() and len(ticks)>5)
            release.set(); self.pump(lambda:not self.tab.busy and bool(delivered))
        timer.stop(); self.assertEqual(delivered[0],self.app.thread())

    def test_shutdown_cancels_and_drains_worker(self):
        entered=threading.Event(); release=threading.Event(); original=scope.import_csv
        def held(*args,**kwargs): entered.set(); release.wait(5); return original(*args,**kwargs)
        with patch.object(scope,"import_csv",held):
            self.tab.import_file(); self.pump(entered.is_set); self.tab.shutdown(); release.set()
            self.pump(lambda:not self.tab.thread.isRunning())
        self.assertTrue(self.tab.canceled.is_set())

    def test_export_sidecar_does_not_mutate_b01(self):
        self.imported(); session=self.root/"session.json"; session.write_text(json.dumps(pc_bringup.template()),encoding="utf-8")
        original=session.read_bytes()
        self.tab.report["electrical_status"]="PASS"  # Widget snapshot cannot modify worker-owned evidence.
        self.assertTrue(self.tab.submit("export",out=self.root/"output",b01=session,check="powerup_waveforms"))
        self.pump(lambda:not self.tab.busy)
        self.assertEqual(session.read_bytes(),original); scope.verify_bundle(self.root/"output")
        r=json.loads((self.root/"output/report.json").read_text())
        self.assertEqual(r["b01_sidecar"]["status"],"NOT_TESTED")
        self.assertEqual(r["electrical_status"],"NOT_TESTED")
        self.assertFalse(r["physically_qualified"])

    def test_synthetic_review_rejected_in_worker(self):
        self.imported()
        self.tab.submit("export",out=self.root/"output",review=dict(operator="x",date="x",rationale="x",evidence="x"))
        self.pump(lambda:not self.tab.busy); self.assertIn("synthetic",self.tab.status.text())

    def test_separate_acquisition_amplitude_only(self):
        self.imported(); path,c,_,_=scope_synthetic.generate(self.root/"second","noise")
        self.tab.submit("compare",path=path,config=c,frequency=1000); self.pump(lambda:not self.tab.busy)
        r=self.tab.report["independent_comparison"]
        self.assertIsNone(r["relative_phase_deg"]); self.assertFalse(r["synchronized"])

    def test_source_changed_export_refused(self):
        self.imported(); self.path.write_text("changed",encoding="utf-8")
        self.tab.submit("export",out=self.root/"output"); self.pump(lambda:not self.tab.busy)
        self.assertIn("changed",self.tab.status.text()); self.assertFalse((self.root/"output").exists())

    def test_plot_render_image_and_point_bound(self):
        self.imported(); self.app.processEvents()
        image=self.tab.plot.grab().toImage(); self.assertFalse(image.isNull())
        colors={image.pixelColor(x,y).name() for x in range(0,image.width(),10) for y in range(0,image.height(),10)}
        self.assertGreater(len(colors),3)
        with self.assertRaises(ValueError): self.tab.plot.set_traces([dict(points=[(0,0)]*4097)])

    def test_mainwindow_integrates_independent_scope_no_serial_operations(self):
        window=desktop.MainWindow(); window.show()
        try:
            self.assertTrue(window.scope_tab.thread.isRunning()); self.assertFalse(window.controller.busy)
            self.assertFalse(window.capture_button.isEnabled())
            window.close(); self.pump(lambda:not window.controller.thread.isRunning() and not window.scope_tab.thread.isRunning())
        finally:
            if window.controller.thread.isRunning(): window.controller.shutdown()
            if window.scope_tab.thread.isRunning(): window.scope_tab.shutdown()
            self.pump(lambda:not window.controller.thread.isRunning() and not window.scope_tab.thread.isRunning())
            window.deleteLater()


if __name__=="__main__": unittest.main()

"""Headless real Qt interactions. Automatic confirmations exist only in this test harness."""
import os
from pathlib import Path
import sys
import tempfile
import subprocess
import threading
import time
import unittest
from unittest.mock import patch

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import pc_osl_campaign as guided
import pc_osl_workbench_model as model
try:
    from PySide6.QtWidgets import QApplication, QDialog, QLineEdit
    from PySide6.QtCore import QThread, QTimer
    from PySide6.QtTest import QTest
    from PySide6.QtCore import Qt
    import pc_osl_workbench as desktop
    QT_AVAILABLE = True
except ImportError:
    QT_AVAILABLE = False

BRIDGE = os.environ.get("WTK_CAPTURE_BRIDGE")


@unittest.skipUnless(QT_AVAILABLE, "install tools/requirements-workbench.txt for real Qt tests")
class QtTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.window = desktop.MainWindow(BRIDGE)
        self.window.show()
        self.errors = []
        self.window.controller.updated.connect(lambda result: self.errors.append(result["error"]) if result["error"] else None)
        self.addCleanup(self.close_window)

    def pump(self, predicate, timeout=30):
        deadline = time.monotonic() + timeout
        while not predicate():
            self.app.processEvents()
            if time.monotonic() > deadline:
                self.fail("Qt worker deadline exceeded")
            time.sleep(0.001)
        self.app.processEvents()

    def operation(self, action, **args):
        self.assertTrue(self.window.controller.submit(action, **args))
        self.pump(lambda: not self.window.controller.busy)

    def close_window(self):
        if self.window.controller.thread.isRunning():
            self.window.close()
            self.pump(lambda: not self.window.controller.thread.isRunning())

    def create(self):
        self.operation("create", path=self.root / "campaign.json", inventory=guided.synthetic_inventory())

    def test_startup_disabled_capture_and_export(self):
        self.assertFalse(self.window.capture_button.isEnabled())
        self.assertFalse(self.window.build_button.isEnabled())
        self.assertIn("SIMULATED", self.window.banner.text())

    def test_real_widgets_reference_validation_optional_not_defaulted(self):
        dialog = desktop.ReferencesDialog(guided.synthetic_inventory())
        self.assertEqual(dialog.read(), guided.synthetic_inventory())
        dialog.fields[0]["published_tolerance_pct"].clear()
        self.assertIsNone(dialog.validate())
        self.assertIn("REJECTED", dialog.interval.text())
        self.assertEqual(dialog.fields[0]["actual_impedance_ohms"][0].text(), "")
        dialog.deleteLater()

    def test_reference_actual_provenance_and_interval_forms(self):
        dialog = desktop.ReferencesDialog(guided.synthetic_inventory())
        fields = dialog.fields[0]
        for editor, value in zip(fields["actual_impedance_ohms"], ("10.05", "0")):
            editor.setText(value)
        self.assertIsNone(dialog.validate())
        for key in ("instrument", "operator", "date", "evidence"):
            fields["measurement_" + key].setText("TEST_ONLY")
        for editor, value in zip(fields["measured_interval_ohms"], ("10.04", "10.06")):
            editor.setText(value)
        self.assertIsNotNone(dialog.validate())
        self.assertIn("independent interval", dialog.interval.text())
        dialog.deleteLater()

    def test_reference_bound_editor_is_readonly(self):
        dialog = desktop.ReferencesDialog(guided.synthetic_inventory(), locked=True)
        self.assertTrue(all(editor.isReadOnly() for editor in dialog.findChildren(QLineEdit)))
        dialog.deleteLater()

    def test_reconstructed_matrix_and_incomplete_build_disabled(self):
        self.create()
        self.assertEqual(self.window.table.rowCount(), 33)
        self.assertEqual(self.window.table.item(0, 3).text(), "PENDING")
        self.assertIn("SYNTHETIC_OPEN", self.window.operation.text())
        self.assertFalse(self.window.build_button.isEnabled())
        self.assertFalse(self.window.load_refs.isEnabled())

    def test_corrupt_open_does_not_destroy_progress(self):
        self.create()
        path = self.root / "broken.json"; path.write_text("{}")
        self.operation("open", path=path)
        self.assertTrue(self.errors)
        self.assertEqual(self.window.snapshot["path"], str(self.root / "campaign.json"))

    def test_worker_events_are_on_gui_thread_and_queue_is_bounded(self):
        observed = []
        self.window.controller.updated.connect(lambda result: observed.append(QThread.currentThread()))
        self.assertTrue(self.window.controller.submit("create", path=self.root / "campaign.json", inventory=guided.synthetic_inventory()))
        self.assertFalse(self.window.controller.submit("create", path=self.root / "duplicate.json", inventory=guided.synthetic_inventory()))
        self.pump(lambda: not self.window.controller.busy)
        self.assertTrue(observed)
        self.assertTrue(all(thread == self.app.thread() for thread in observed))
        self.assertNotEqual(self.window.controller.worker.thread(), self.app.thread())
        self.assertFalse((self.root / "duplicate.json").exists())

    def test_unknown_physical_gate_does_not_enable_capture(self):
        self.create()
        snapshot = self.window.snapshot.copy()
        snapshot.update(connected=True, mode="Physical", gate_approved=False)
        self.window.snapshot = snapshot
        self.window.confirm.setChecked(True)
        self.window.interlocks()
        self.assertFalse(self.window.capture_button.isEnabled())
        snapshot.update(mode="Simulation", unknown=True)
        self.window.interlocks()
        self.assertFalse(self.window.capture_button.isEnabled())

    def test_b01_import_view_shows_missing_gates_and_synthetic_provenance(self):
        import json
        import pc_bringup
        self.create()
        session = pc_bringup.template(); session["firmware"]["profile"] = "BRINGUP"
        path = self.root / "b01.json"
        path.write_text(json.dumps(dict(session=session, SIMULATED=True, checks=[])), encoding="utf-8")
        self.operation("evidence", path=path)
        view = self.window.evidence_view.toPlainText()
        for text in ("R02", "R05", "SIMULATED", "3v3_stability"):
            self.assertIn(text, view)
        self.assertFalse(self.window.capture_button.isEnabled())
        self.assertFalse(self.window.gate_button.isEnabled())

    @unittest.skipUnless(BRIDGE, "WTK_CAPTURE_BRIDGE required")
    def test_c_connection_capture_click_confirmation_and_duplicate_prevention(self):
        self.create()
        self.operation("connect", mode="Simulation", address=BRIDGE, operator="SYNTHETIC_OPERATOR")
        self.assertIn("UNCALIBRATED", self.window.device.text())
        self.assertFalse(self.window.capture_button.isEnabled())
        self.window.confirm.setChecked(True)
        QTest.mouseClick(self.window.capture_button, Qt.LeftButton)
        self.assertFalse(self.window.capture_button.isEnabled())
        self.assertFalse(self.window.controller.submit("acquire", confirmed=True))
        self.pump(lambda: not self.window.controller.busy)
        self.assertEqual(self.window.progress.value(), 1)
        self.assertFalse(self.window.confirm.isChecked())
        self.assertFalse(self.window.capture_button.isEnabled())
        self.assertIn("sha256", self.window.quality.toPlainText())
        self.assertEqual(self.window.table.item(0, 3).text(), "ACCEPTED")

    @unittest.skipUnless(BRIDGE, "WTK_CAPTURE_BRIDGE required")
    def test_close_during_capture_uses_cancel_and_worker_cleanup(self):
        self.close_window()
        entered = threading.Event()
        class HeldClient(model.InterruptibleClient):
            def begin(client, *args):
                result = super(HeldClient, client).begin(*args)
                entered.set()
                client.cancel_event.wait(2)
                return result
        self.window = desktop.MainWindow(BRIDGE)
        self.create()
        with patch.object(model, "InterruptibleClient", HeldClient):
            self.operation("connect", mode="Simulation", address=BRIDGE, operator="SYNTHETIC_OPERATOR")
        self.window.confirm.setChecked(True)
        self.window.acquire()
        self.pump(entered.is_set)
        self.window.close()
        self.pump(lambda: not self.window.controller.thread.isRunning())
        doc = guided.Campaign.load(self.root / "campaign.json").document
        self.assertEqual(len(doc["captures"]), 0)
        self.assertIn("CANCELED", doc["attempts"][-1]["reason"])
        self.assertTrue(doc["attempts"][-1]["cancellation"]["transfer_safe"])

    @unittest.skipUnless(BRIDGE, "WTK_CAPTURE_BRIDGE required")
    def test_gui_remains_responsive_and_unknown_cancel_blocks_next_capture(self):
        self.close_window()
        began = threading.Event()
        class LostClient(model.InterruptibleClient):
            def begin(client, *args):
                result = super(LostClient, client).begin(*args)
                began.set()
                client.cancel_event.wait(2)
                client.transport.read = lambda size: (_ for _ in ()).throw(OSError("TEST_ONLY lost connection"))
                return result
        self.window = desktop.MainWindow(BRIDGE)
        self.window.show()
        self.create()
        with patch.object(model, "InterruptibleClient", LostClient):
            self.operation("connect", mode="Simulation", address=BRIDGE, operator="SYNTHETIC_OPERATOR")
        ticks = []
        timer = QTimer(); timer.setInterval(10); timer.timeout.connect(lambda: ticks.append(True)); timer.start()
        self.window.confirm.setChecked(True)
        self.window.acquire()
        self.pump(lambda: began.is_set() and len(ticks) >= 3)
        self.assertTrue(self.window.cancel_button.isEnabled())
        self.window.cancel_button.click()
        self.pump(lambda: not self.window.controller.busy)
        timer.stop()
        self.assertTrue(self.window.snapshot["unknown"])
        self.window.confirm.setChecked(True)
        self.assertFalse(self.window.capture_button.isEnabled())
        self.assertEqual(self.window.progress.value(), 0)
        self.assertIn("UNKNOWN", self.window.operation.text())

    @unittest.skipUnless(BRIDGE, "WTK_CAPTURE_BRIDGE required")
    def test_gui_99_confirmations_restart_at_42_generate_candidate(self):
        # A separate GUI process exits abruptly after the durable 42nd observation.
        # There is no application close handler, cache handoff or in-memory campaign reuse.
        script = '''
import os, sys, time
from pathlib import Path
sys.path.insert(0, sys.argv[1])
import pc_osl_workbench as desktop
import pc_osl_campaign as guided
from PySide6.QtWidgets import QApplication
app = QApplication([])
window = desktop.MainWindow(sys.argv[2]); window.show()
def wait():
    deadline = time.monotonic() + 30
    while window.controller.busy:
        app.processEvents()
        if time.monotonic() > deadline: os._exit(90)
        time.sleep(.001)
    app.processEvents()
window.controller.submit("create", path=Path(sys.argv[3]), inventory=guided.synthetic_inventory()); wait()
window.controller.submit("connect", mode="Simulation", address=sys.argv[2], operator="SYNTHETIC_OPERATOR"); wait()
for index in range(42):
    window.confirm.setChecked(True); window.capture_button.click(); wait()
    if window.progress.value() != index + 1: os._exit(91)
os._exit(17)
'''
        result = subprocess.run([sys.executable, "-c", script, str(Path(desktop.__file__).parent), BRIDGE,
                                 str(self.root / "campaign.json")], timeout=60, capture_output=True)
        self.assertEqual(result.returncode, 17, result.stderr.decode(errors="replace"))
        saved = guided.Campaign.load(self.root / "campaign.json")
        hashes = [r["sha256"] for r in saved.document["captures"]]
        self.close_window()
        self.window = desktop.MainWindow(BRIDGE)
        self.window.show()
        self.operation("open", path=self.root / "campaign.json")
        self.assertEqual(self.window.progress.value(), 42)
        self.assertIn("UNKNOWN", self.window.operation.text())
        self.operation("connect", mode="Simulation", address=BRIDGE, operator="SYNTHETIC_OPERATOR")
        for _ in range(57):
            self.window.confirm.setChecked(True)
            QTest.mouseClick(self.window.capture_button, Qt.LeftButton)
            self.pump(lambda: not self.window.controller.busy)
        self.assertEqual(self.window.progress.value(), 99)
        self.assertTrue(self.window.build_button.isEnabled())
        self.assertTrue(all(self.window.table.item(r, 6).text() == "ACCEPTED" for r in range(33)))
        self.window.export_directory = self.root / "output"
        self.operation("build", out=self.window.export_directory)
        self.assertEqual((self.root / "output/candidate.bin").stat().st_size, 2760)
        self.assertIn("NOT INSTALLED", self.window.quality.toPlainText())
        doc = guided.Campaign.load(self.root / "campaign.json").document
        self.assertEqual(hashes, [r["sha256"] for r in doc["captures"][:42]])
        self.assertEqual(len(doc["confirmations"]), 99)
        self.assertFalse(doc["candidate"]["installed"])
        if os.environ.get("WTK_WORKBENCH_SCREENSHOT"):
            self.window.grab().save(os.environ["WTK_WORKBENCH_SCREENSHOT"])

    def test_ui_has_no_installation_reset_force_or_auto_confirmation_action(self):
        from PySide6.QtWidgets import QPushButton
        labels = [b.text().lower() for b in self.window.findChildren(QPushButton)]
        self.assertFalse(any("install" in label or "reset" in label or "force" in label or "auto" in label for label in labels))

    def test_unknown_worker_operation_is_rejected(self):
        self.operation("install")
        self.assertTrue(any("unsupported desktop" in error for error in self.errors))


if __name__ == "__main__":
    unittest.main()

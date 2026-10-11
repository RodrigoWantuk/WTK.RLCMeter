"""Read-only waveform tab. One worker owns arrays; widgets receive bounded envelopes."""
from pathlib import Path
import copy
import json
import threading

from PySide6.QtCore import QObject, QThread, Signal, Slot, Qt, QPointF
from PySide6.QtGui import QPainter, QPen, QColor, QPolygonF
from PySide6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QFormLayout,
    QLineEdit, QComboBox, QCheckBox, QPushButton, QLabel, QScrollArea, QSplitter,
    QPlainTextEdit, QFileDialog, QGroupBox)

import scope_waveform as scope


class ScopeWorker(QObject):
    done=Signal(object)
    def __init__(self,canceled):
        super().__init__(); self.canceled=canceled; self.wave=None; self.report=None

    @Slot(str,object)
    def run(self,action,args):
        result=dict(action=action,error=None)
        try:
            if action=="import":
                # Failed replacement never leaves the previous file apparently current.
                self.wave=self.report=None
                self.wave=scope.import_csv(args["path"],args["config"],self.canceled.is_set)
                self.report=scope.analyze(self.wave,args["preset"],args["parameters"])
                result["report"]=copy.deepcopy(self.report)
                result["points"]=self.points()
            elif action=="zoom":
                if self.wave is None: raise ValueError("import a valid waveform first")
                result["points"]=self.points(args["start"],args["end"])
            elif action=="export":
                if self.wave is None or self.report is None: raise ValueError("import a valid waveform first")
                scope.export_bundle(self.wave,self.report,**args)
                result["output"]=str(args["out"])
            elif action=="compare":
                if self.wave is None or self.report is None: raise ValueError("import a valid waveform first")
                second=scope.import_csv(args["path"],args["config"],self.canceled.is_set)
                self.report["independent_comparison"]=scope.independent_comparison(self.wave,second,frequency=args.get("frequency"))
                if second.config.metadata["synthetic"]:
                    self.report["evidence_state"]="SYNTHETIC"
                self.report["comparison_import_config"]=scope.asdict(second.config)
                result["report"]=copy.deepcopy(self.report)
            else:
                raise ValueError("unsupported waveform operation")
        except Exception as exc:
            result["error"]=str(exc)
            if action=="import": self.wave=self.report=None
        self.done.emit(result)

    def points(self,start=None,end=None):
        traces=[dict(net=c.net,unit="V",points=scope.envelope(self.wave,v,start,end)) for c,v in zip(self.wave.config.channels,self.wave.channels)]
        if self.report["preset"]=="excitation":
            # Derived voltage envelope, not a duplicate full acquisition buffer.
            from array import array
            diff=array("d",(a-b for a,b in zip(*self.wave.channels)))
            traces.append(dict(net="VEXC_DIFF",unit="V",points=scope.envelope(self.wave,diff,start,end)))
        return traces


class WavePlot(QWidget):
    """QtGui painter only, no QtCharts/QML. Peak-preserving min/max envelopes."""
    def __init__(self):
        super().__init__(); self.traces=[]; self.setMinimumHeight(240)

    def set_traces(self,traces):
        if any(len(t["points"])>4096 for t in traces): raise ValueError("plot point bound exceeded")
        self.traces=traces; self.update()

    def paintEvent(self,event):
        painter=QPainter(self); painter.fillRect(self.rect(),QColor("#fafcfe"))
        painter.setPen(QColor("#203948"))
        if not self.traces:
            painter.drawText(20,30,"Import a waveform. No data is nominal or automatically passing."); return
        points=[p for t in self.traces for p in t["points"]]
        t0=min(p[0] for p in points); t1=max(p[0] for p in points)
        lo=min(p[1] for p in points); hi=max(p[1] for p in points)
        span=max(hi-lo,1e-12); width=max(1,self.width()-100); height=max(1,self.height()-90)
        for i in range(5):
            y=50+height*i/4; painter.setPen(QColor("#dce5eb")); painter.drawLine(QPointF(65,y),QPointF(65+width,y))
            painter.setPen(QColor("#203948")); painter.drawText(3,int(y),f"{hi-span*i/4:.4g}")
        colors=("#087ba6","#ba642a","#29794c")
        for i,trace in enumerate(self.traces):
            painter.setPen(QPen(QColor(colors[i%3]),1.2))
            poly=QPolygonF([QPointF(65+width*(t-t0)/max(t1-t0,1e-30),50+height*(hi-v)/span) for t,v in trace["points"]])
            painter.drawPolyline(poly); painter.drawText(65+210*i,25,trace["net"]+" (V)")
        painter.setPen(QColor("#203948")); painter.drawText(65,self.height()-18,f"{t0:.8g} .. {t1:.8g} s | original-order min/max envelope; zoom to inspect samples")


def edit(form,label,value=""):
    field=QLineEdit(str(value)); form.addRow(label,field); return field


def choice(form,label,values):
    field=QComboBox(); field.addItems(values); form.addRow(label,field); return field


def optional(field):
    return float(field.text()) if field.text().strip() else None


class ScopeTab(QWidget):
    request=Signal(str,object)
    closed=Signal()
    def __init__(self):
        super().__init__(); self.busy=False; self.report=None; self.canceled=threading.Event()
        self.thread=QThread(self); self.worker=ScopeWorker(self.canceled); self.worker.moveToThread(self.thread)
        self.request.connect(self.worker.run); self.worker.done.connect(self.finished)
        self.thread.finished.connect(self.worker.deleteLater); self.thread.finished.connect(self.closed)
        self.thread.start()
        layout=QVBoxLayout(self); warning=QLabel(scope.SAFETY); warning.setWordWrap(True); layout.addWidget(warning)
        split=QSplitter(); layout.addWidget(split,1)
        settings=QWidget(); form=QFormLayout(settings); scroll=QScrollArea(); scroll.setWidgetResizable(True); scroll.setWidget(settings); split.addWidget(scroll)
        self.path=edit(form,"CSV source")
        self.select=QPushButton("Select file…"); self.select.clicked.connect(self.select_file); form.addRow(self.select)
        self.load_settings_button=QPushButton("Load explicit mapping/settings…"); self.load_settings_button.clicked.connect(self.load_settings); form.addRow(self.load_settings_button)
        self.save_settings_button=QPushButton("Save mapping/settings…"); self.save_settings_button.clicked.connect(self.save_settings); form.addRow(self.save_settings_button)
        self.profile=choice(form,"Profile",["generic","hantek_dso2c10"])
        self.preset=choice(form,"Analysis",["basic","supply","excitation","clamp","ret"])
        self.time_column=edit(form,"Time column (zero based)",0)
        self.time_unit=choice(form,"Time unit",["s","ms","us","ns"])
        self.delimiter=choice(form,"Separator",["comma","semicolon","tab"])
        self.decimal=choice(form,"Decimal",[".",","])
        self.header=QCheckBox("First data row is header"); self.header.setChecked(True); form.addRow(self.header)
        self.skip=edit(form,"Opaque preamble rows (skip)",0); self.offset=edit(form,"Time offset (s)",0)
        self.interval=edit(form,"Declared interval (s, optional)")
        self.simultaneous=QCheckBox("Channels from SAME simultaneous acquisition"); form.addRow(self.simultaneous)
        self.synthetic=QCheckBox("SYNTHETIC waveform"); self.synthetic.setChecked(True); form.addRow(self.synthetic)
        self.instrument=edit(form,"Instrument model / serial")
        self.operator=edit(form,"Operator"); self.date=edit(form,"Capture date/time / timezone")
        self.trigger=edit(form,"Trigger / acquisition settings")
        self.probe_notes=edit(form,"Probe model / compensation / loading")
        self.state=edit(form,"Supply state / inactive regulator evidence")
        self.limit=edit(form,"Supply current limit (A, optional)")
        self.adapters=edit(form,"Connected adapters")
        self.population=edit(form,"Fitted / DNP population")
        self.channels=[]
        for i in range(2):
            group=QGroupBox(f"CH{i+1} — tip to net, ground to GND"); cf=QFormLayout(group); form.addRow(group)
            enabled=QCheckBox("Import channel"); enabled.setChecked(True); cf.addRow(enabled)
            fields=dict(enabled=enabled,column=edit(cf,"Column",i+1),net=edit(cf,"Net"),unit=choice(cf,"CSV voltage unit",["V","mV","uV"]),
                scale=edit(cf,"Explicit scale multiplier",1),offset=edit(cf,"Offset at tip (V)",0),probe=edit(cf,"Probe attenuation (known)"),
                reference=choice(cf,"CSV voltage refers to",["tip","connector"]),coupling=choice(cf,"Coupling",["UNKNOWN","DC","AC"]),
                bandwidth=edit(cf,"Effective probe/channel bandwidth Hz"),error=edit(cf,"Optional tip-voltage error bound V"),
                accuracy=edit(cf,"Accuracy / probe evidence"),low=edit(cf,"Known display low (V, optional)"),high=edit(cf,"Known display high (V, optional)"))
            self.channels.append(fields)
        self.frequency=choice(form,"Expected excitation Hz",["1000","100","10000"])
        self.resistor=edit(form,"Fitted series resistor Ω")
        self.component=edit(form,"Output / ADC / resistor component mapping")
        self.mapping=edit(form,"Mapping evidence")
        self.resistor_tol=edit(form,"Resistor tolerance %, optional")
        self.resistor_evidence=edit(form,"Resistor tolerance evidence")
        self.rail=edit(form,"Independent measured +3V3 V, optional")
        self.rail_evidence=edit(form,"Rail instrument / date / evidence")
        self.inactive_start=edit(form,"Regulator inactive start s, optional")
        self.inactive_end=edit(form,"Regulator inactive end s, optional")
        right=QWidget(); rv=QVBoxLayout(right); split.addWidget(right); split.setSizes([390,720])
        buttons=QHBoxLayout(); rv.addLayout(buttons)
        self.import_button=QPushButton("Import & analyze"); self.import_button.clicked.connect(self.import_file); buttons.addWidget(self.import_button)
        self.export_button=QPushButton("Export evidence…"); self.export_button.clicked.connect(self.export); buttons.addWidget(self.export_button)
        self.export_button.setEnabled(False)
        self.status=QLabel("No waveform. Native Hantek layout unverified; explicit generic columns only."); self.status.setWordWrap(True); rv.addWidget(self.status)
        self.plot=WavePlot(); rv.addWidget(self.plot,2)
        zoom=QHBoxLayout(); rv.addLayout(zoom)
        self.start=QLineEdit(); self.end=QLineEdit(); self.start.setPlaceholderText("start s"); self.end.setPlaceholderText("end s")
        zoom.addWidget(self.start); zoom.addWidget(self.end); self.zoom_button=QPushButton("Zoom interval"); self.zoom_button.clicked.connect(self.zoom); zoom.addWidget(self.zoom_button)
        self.full_button=QPushButton("Full trace"); self.full_button.clicked.connect(lambda:self.submit("zoom",start=None,end=None)); zoom.addWidget(self.full_button)
        self.metrics=QPlainTextEdit(); self.metrics.setReadOnly(True); self.metrics.setMinimumHeight(130); rv.addWidget(self.metrics,2)
        self.compare_button=QPushButton("Compare separate CSV (amplitude only)…")
        self.compare_button.clicked.connect(self.compare); rv.addWidget(self.compare_button); self.compare_button.setEnabled(False)
        association=QFormLayout(); rv.addLayout(association)
        self.b01=edit(association,"Optional B01 session/report")
        pick=QPushButton("Select B01…"); pick.clicked.connect(self.select_b01); association.addRow(pick)
        self.check=choice(association,"B01 sidecar association",list(scope.B01_KEYS))
        self.review_operator=edit(form,"Optional review operator (REAL only)")
        self.review_date=edit(form,"Review date / timezone")
        self.review_rationale=edit(form,"Independent review rationale")
        self.review_evidence=edit(form,"Review evidence reference")

    def select_file(self):
        path,_=QFileDialog.getOpenFileName(self,"Waveform CSV","","CSV (*.csv);;All files (*)")
        if path: self.path.setText(path)

    def select_b01(self):
        path,_=QFileDialog.getOpenFileName(self,"B01 session/report","","JSON (*.json)")
        if path: self.b01.setText(path)

    def apply_settings(self,config,preset="basic",parameters=None):
        scope.validate(config)
        p=parameters or {}; m=config.metadata
        self.profile.setCurrentText(config.profile); self.preset.setCurrentText(preset)
        self.time_column.setText(str(config.time_column)); self.time_unit.setCurrentText(config.time_unit)
        self.delimiter.setCurrentText({",":"comma",";":"semicolon","\t":"tab"}[config.delimiter]); self.decimal.setCurrentText(config.decimal)
        self.header.setChecked(config.header); self.skip.setText(str(config.skip_rows)); self.offset.setText(str(config.time_offset_s))
        self.interval.setText(str(config.declared_interval_s) if config.declared_interval_s is not None else "")
        self.simultaneous.setChecked(config.simultaneous); self.synthetic.setChecked(m["synthetic"])
        for widget,key in ((self.instrument,"instrument"),(self.operator,"operator"),(self.date,"capture_date"),(self.trigger,"trigger_acquisition"),
            (self.probe_notes,"probe_notes"),(self.state,"supply_state"),(self.limit,"current_limit_a"),(self.adapters,"adapters"),(self.population,"population")):
            value=m.get(key); widget.setText(str(value) if value is not None else "")
        for i,f in enumerate(self.channels):
            f["enabled"].setChecked(i<len(config.channels))
            if i>=len(config.channels): continue
            c=config.channels[i]
            for name,value in (("column",c.column),("net",c.net),("scale",c.scale),("offset",c.offset_v),("probe",c.probe_attenuation),("bandwidth",c.bandwidth_hz),("error",c.voltage_error_bound_v),("accuracy",c.accuracy_evidence),
                ("low",c.displayed_limits_v[0] if c.displayed_limits_v else None),("high",c.displayed_limits_v[1] if c.displayed_limits_v else None)):
                f[name].setText(str(value) if value is not None else "")
            for name,value in (("unit",c.unit),("reference",c.voltage_reference),("coupling",c.coupling)): f[name].setCurrentText(value)
        self.frequency.setCurrentText(str(p.get("frequency_hz",1000)))
        for widget,key in ((self.resistor,"series_resistor_ohms"),(self.component,"component"),(self.mapping,"mapping_evidence"),(self.resistor_tol,"resistor_tolerance_pct"),
            (self.resistor_evidence,"resistor_evidence"),(self.rail,"measured_3v3_v"),(self.rail_evidence,"rail_evidence")):
            value=p.get(key); widget.setText(str(value) if value is not None else "")
        interval=p.get("regulator_inactive_interval_s")
        self.inactive_start.setText(str(interval[0]) if interval else ""); self.inactive_end.setText(str(interval[1]) if interval else "")

    def load_settings(self):
        path,_=QFileDialog.getOpenFileName(self,"Explicit generic mapping/settings","","JSON (*.json)")
        if not path: return
        try:
            file=Path(path)
            value=scope.settings_json(file); value["channels"]=tuple(scope.Channel(**c) for c in value["channels"])
            parameters={}; params=file.with_name("parameters.json")
            if params.is_file():
                parameters=scope.settings_json(params)
            self.apply_settings(scope.ImportConfig(**value),self.preset.currentText(),parameters)
            candidate=file.with_name("waveform.csv")
            if candidate.is_file(): self.path.setText(str(candidate))
        except (ValueError,TypeError,KeyError,OSError) as exc: self.status.setText("REJECTED settings: "+str(exc))

    def save_settings(self):
        path,_=QFileDialog.getSaveFileName(self,"Save explicit mapping/settings","","JSON (*.json)")
        if not path: return
        try:
            config=self.config(); scope.validate(config)
            Path(path).write_text(json.dumps(scope.asdict(config),indent=2,allow_nan=False)+"\n",encoding="utf-8")
            self.status.setText("Explicit mapping saved. Analysis parameters and settings used are retained in each report.")
        except (ValueError,TypeError,OSError) as exc: self.status.setText("REJECTED settings: "+str(exc))

    def config(self):
        channels=[]
        for f in self.channels:
            if not f["enabled"].isChecked(): continue
            limits=None if not f["low"].text() and not f["high"].text() else (float(f["low"].text()),float(f["high"].text()))
            channels.append(scope.Channel(int(f["column"].text()),f["net"].text(),f["unit"].currentText(),float(f["scale"].text()),float(f["offset"].text()),
                float(f["probe"].text()),f["reference"].currentText(),f["coupling"].currentText(),optional(f["bandwidth"]),optional(f["error"]),f["accuracy"].text() or None,limits))
        metadata=dict(synthetic=self.synthetic.isChecked(),instrument=self.instrument.text() or "UNKNOWN",operator=self.operator.text() or "UNKNOWN",
            capture_date=self.date.text() or "UNKNOWN",trigger_acquisition=self.trigger.text() or "UNKNOWN",probe_notes=self.probe_notes.text() or "UNKNOWN",
            supply_state=self.state.text() or "UNKNOWN",current_limit_a=optional(self.limit),adapters=self.adapters.text() or "UNKNOWN",population=self.population.text() or "UNKNOWN")
        return scope.ImportConfig(int(self.time_column.text()),self.time_unit.currentText(),tuple(channels),{"comma":",","semicolon":";","tab":"\t"}[self.delimiter.currentText()],
            self.decimal.currentText(),self.header.isChecked(),int(self.skip.text()),float(self.offset.text()),self.simultaneous.isChecked(),optional(self.interval),self.profile.currentText(),metadata)

    def parameters(self):
        p={}
        if self.preset.currentText() in ("excitation","ret"): p["frequency_hz"]=int(self.frequency.currentText())
        if self.preset.currentText()=="clamp":
            p=dict(series_resistor_ohms=float(self.resistor.text()),component=self.component.text(),mapping_evidence=self.mapping.text(),
                resistor_tolerance_pct=optional(self.resistor_tol),resistor_evidence=self.resistor_evidence.text(),measured_3v3_v=optional(self.rail),rail_evidence=self.rail_evidence.text())
        if self.inactive_start.text() or self.inactive_end.text():
            p.update(regulator_inactive_interval_s=[float(self.inactive_start.text()),float(self.inactive_end.text())],supply_state_evidence=self.state.text())
        return p

    def submit(self,action,**args):
        if self.busy or not self.thread.isRunning(): return False
        self.busy=True; self.canceled.clear()
        for b in (self.import_button,self.export_button,self.zoom_button,self.full_button,self.select,self.compare_button,self.load_settings_button,self.save_settings_button): b.setEnabled(False)
        self.status.setText("Working offline… no serial or hardware command is issued.")
        self.request.emit(action,args); return True

    def import_file(self):
        if self.busy: return
        self.report=None; self.plot.set_traces([]); self.export_button.setEnabled(False)
        try: self.submit("import",path=Path(self.path.text()),config=self.config(),preset=self.preset.currentText(),parameters=self.parameters())
        except (ValueError,TypeError) as exc: self.status.setText("REJECTED: "+str(exc))

    def zoom(self):
        try: self.submit("zoom",start=float(self.start.text()),end=float(self.end.text()))
        except ValueError as exc: self.status.setText("REJECTED: "+str(exc))

    def export(self):
        out=QFileDialog.getSaveFileName(self,"New evidence directory (must not exist)","","Directory name (*)")[0]
        if not out: return
        review=None
        if any(f.text() for f in (self.review_operator,self.review_date,self.review_rationale,self.review_evidence)):
            review=dict(operator=self.review_operator.text(),date=self.review_date.text(),rationale=self.review_rationale.text(),evidence=self.review_evidence.text())
        self.submit("export",out=Path(out),b01=Path(self.b01.text()) if self.b01.text() else None,check=self.check.currentText(),review=review)

    def compare(self):
        path,_=QFileDialog.getOpenFileName(self,"Independent acquisition: current column/probe settings apply; no synchronized phase","","CSV (*.csv)")
        if path:
            try: self.submit("compare",path=Path(path),config=self.config(),frequency=int(self.frequency.currentText()))
            except ValueError as exc: self.status.setText("REJECTED: "+str(exc))

    @Slot(object)
    def finished(self,result):
        self.busy=False
        for b in (self.import_button,self.select,self.load_settings_button,self.save_settings_button): b.setEnabled(True)
        if result["error"]:
            self.status.setText("REJECTED: "+result["error"])
            if result["action"]=="import": self.report=None; self.plot.set_traces([]); self.metrics.clear()
        else:
            if "report" in result:
                self.report=result["report"]; self.metrics.setPlainText(scope.summary(self.report))
            if "points" in result: self.plot.set_traces(result["points"])
            self.status.setText((self.report["evidence_state"]+" — NOT_TESTED electrical acceptance. No accuracy qualification.") if self.report else "No waveform")
            if "output" in result: self.status.setText("Exported sidecar: "+result["output"]+"; B01 remains unchanged / NOT_TESTED")
        for b in (self.export_button,self.zoom_button,self.full_button,self.compare_button): b.setEnabled(self.report is not None)

    def shutdown(self):
        self.canceled.set(); self.thread.quit()  # Cooperative parser cancellation, bounded analysis drain; never terminate.

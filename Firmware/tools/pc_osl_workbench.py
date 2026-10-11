#!/usr/bin/env python3
"""Offline Windows calibration desktop. Widgets consume the existing Campaign backend."""
from __future__ import annotations

import argparse
import copy
import json
import os
from pathlib import Path
import sys

from PySide6.QtCore import QObject, QThread, Signal, Slot, Qt, QTimer
from PySide6.QtGui import QColor, QDesktopServices, QFont, QFontDatabase
from PySide6.QtCore import QUrl
from PySide6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QFormLayout, QPushButton, QLabel, QComboBox, QLineEdit, QCheckBox, QTableWidget,
    QTableWidgetItem, QHeaderView, QTabWidget, QPlainTextEdit, QFileDialog, QMessageBox,
    QDialog, QDialogButtonBox, QSplitter, QGroupBox, QProgressBar, QScrollArea)

import pc_osl
import pc_osl_campaign as guided
from pc_osl_workbench_model import Workbench, ACKNOWLEDGEMENTS, inventory_save
from scope_workbench import ScopeTab


class Worker(QObject):
    finished = Signal(object)
    message = Signal(str)

    def __init__(self, backend):
        super().__init__()
        self.backend = backend

    @Slot(str, object)
    def execute(self, action, arguments):
        error = None
        try:
            if action not in ("open", "create", "clear", "connect", "status", "acquire", "evidence", "approve", "build", "disconnect"):
                raise ValueError("unsupported desktop operation")
            if action == "acquire":
                arguments = dict(arguments, log=self.message.emit)
            getattr(self.backend, action)(**arguments)
        except KeyboardInterrupt:
            error = "CANCELED: accepted evidence preserved; inspect safe teardown status"
        except Exception as exc:
            error = type(exc).__name__ + ": " + str(exc)
        try:
            snapshot = self.backend.snapshot()
        except Exception as exc:
            snapshot = None
            error = "Campaign inspection failed: " + str(exc)
        self.finished.emit(dict(action=action, error=error, snapshot=snapshot))


class Controller(QObject):
    request = Signal(str, object)
    updated = Signal(object)
    busy_changed = Signal(bool)
    message = Signal(str)
    closed = Signal()

    def __init__(self, backend=None):
        super().__init__()
        backend = backend or Workbench()
        self.cancel_event = backend.cancel_event
        self.thread = QThread(self)
        self.worker = Worker(backend)
        self.worker.moveToThread(self.thread)
        self.request.connect(self.worker.execute)
        self.worker.finished.connect(self.finish)
        self.worker.message.connect(self.message)
        self.thread.finished.connect(self.worker.deleteLater)
        self.busy = False
        self.action = None
        self.closing = False
        self.snapshot = None
        self.thread.start()

    def submit(self, action, **arguments):
        if self.busy or self.closing:
            return False  # No queue of duplicate START/clicks.
        self.busy = True
        self.action = action
        self.busy_changed.emit(True)
        self.request.emit(action, arguments)
        return True

    def cancel(self):
        if self.busy:
            self.cancel_event.set()  # No serial command on the GUI thread.
            self.message.emit("Cancellation requested. Keep fixture unchanged until SAFE is known.")

    @Slot(object)
    def finish(self, result):
        self.busy = False
        self.action = None
        if result["snapshot"] is not None:
            self.snapshot = result["snapshot"]
        if result["error"]:
            self.message.emit(result["error"])
        self.updated.emit(result)
        self.busy_changed.emit(False)
        if self.closing:
            if result["action"] != "disconnect":
                self.busy = True
                self.request.emit("disconnect", {})
            else:
                self.thread.quit()
                self.thread.wait()
                self.closed.emit()

    def shutdown(self):
        if self.closing:
            return
        self.closing = True
        if self.busy:
            self.cancel()
        else:
            self.busy = True
            self.request.emit("disconnect", {})


def line(form, label, value=""):
    editor = QLineEdit(str(value) if value is not None else "")
    form.addRow(label, editor)
    return editor


class ReferencesDialog(QDialog):
    """Forms edit a copy; the backend remains the validation authority."""
    def __init__(self, inventory, locked=False, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Reference inventory" + (" — bound to campaign (read only)" if locked else ""))
        self.resize(700, 710)
        self.inventory = copy.deepcopy(inventory)
        layout = QVBoxLayout(self)
        heading = QLabel("Ohms throughout. Printed tolerance is an interval, not a precision measurement.\n"
                         "Blank optional values remain unknown; actual values require independent provenance.")
        heading.setWordWrap(True)
        layout.addWidget(heading)
        fixtures = QFormLayout()
        self.open_fixture = line(fixtures, "OPEN fixture", inventory["open_fixture"])
        self.short_fixture = line(fixtures, "SHORT fixture", inventory["short_fixture"])
        layout.addLayout(fixtures)
        self.tabs = QTabWidget()
        layout.addWidget(self.tabs)
        self.fields = []
        for ref in inventory["references"]:
            body = QWidget(); form = QFormLayout(body); fields = {}
            for name, label in (("id", "Reference ID"), ("fixture_id", "Fixture ID"),
                ("published_tolerance_pct", "Published tolerance (%)"),
                ("temperature_assumptions", "Temperature assumptions"),
                ("frequency_assumptions", "Frequency assumptions"),
                ("manufacturer", "Manufacturer"), ("part", "Part"), ("notes", "Notes")):
                fields[name] = line(form, label, ref[name])
            for name, label in (("nominal_impedance_ohms", "Nominal"), ("actual_impedance_ohms", "Measured (optional)"),
                                ("measured_interval_ohms", "Characterized interval (optional)")):
                pair = ref[name] or [None, None]
                coordinates = (" lower (Ω)", " upper (Ω)") if name == "measured_interval_ohms" else (" real (Ω)", " imaginary (Ω)")
                fields[name] = [line(form, label + coordinates[i], pair[i]) for i in range(2)]
            for name in ("instrument", "operator", "date", "evidence"):
                fields["measurement_" + name] = line(form, "Measurement " + name, (ref["measurement"] or {}).get(name))
            self.fields.append(fields)
            scroll = QScrollArea(); scroll.setWidgetResizable(True); scroll.setWidget(body)
            self.tabs.addTab(scroll, f'{ref["rref_ohms"]:,} Ω')
        self.interval = QLabel("Validate to see effective intervals; no accuracy qualification.")
        self.interval.setWordWrap(True); layout.addWidget(self.interval)
        actions = QHBoxLayout()
        validate = QPushButton("Validate intervals"); validate.clicked.connect(self.validate)
        actions.addWidget(validate)
        buttons = QDialogButtonBox(QDialogButtonBox.Save | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.save); buttons.rejected.connect(self.reject)
        actions.addWidget(buttons); layout.addLayout(actions)
        if locked:
            for editor in self.findChildren(QLineEdit):
                editor.setReadOnly(True)
            buttons.button(QDialogButtonBox.Save).setEnabled(False)

    def read(self):
        inventory = copy.deepcopy(self.inventory)
        inventory.update(open_fixture=self.open_fixture.text().strip(), short_fixture=self.short_fixture.text().strip())
        for ref, fields in zip(inventory["references"], self.fields):
            for name in ("id", "fixture_id", "temperature_assumptions", "frequency_assumptions", "manufacturer", "part", "notes"):
                ref[name] = fields[name].text().strip() if name == "notes" else fields[name].text().strip() or None
            tolerance = fields["published_tolerance_pct"].text().strip()
            ref["published_tolerance_pct"] = float(tolerance) if tolerance else None
            for name in ("nominal_impedance_ohms", "actual_impedance_ohms", "measured_interval_ohms"):
                values = [editor.text().strip() for editor in fields[name]]
                ref[name] = None if not any(values) else [float(value) for value in values]
            measurement = {name: fields["measurement_" + name].text().strip() for name in ("instrument", "operator", "date", "evidence")}
            ref["measurement"] = measurement if any(measurement.values()) else None
        guided.references(inventory)
        return inventory

    def validate(self):
        try:
            inventory = self.read()
            refs = guided.references(inventory)
            self.interval.setText("\n".join(f'{r} Ω: center {value["impedance_ohms"]}, effective ±{value["tolerance_pct"]:.6g}% '
                + ("(independent interval)" if inventory["references"][i]["measured_interval_ohms"] else "(printed nominal interval retained)")
                for i, (r, value) in enumerate(refs.items())))
            return inventory
        except (ValueError, KeyError, TypeError) as exc:
            self.interval.setText("REJECTED: " + str(exc))
            return None

    def save(self):
        inventory = self.validate()
        if inventory is not None:
            self.inventory = inventory
            self.accept()


class GateDialog(QDialog):
    def __init__(self, snapshot, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Physical evidence review — operator decision, not a safety certificate")
        self.resize(720, 650)
        layout = QVBoxLayout(self)
        view = QPlainTextEdit(); view.setReadOnly(True)
        evidence = snapshot["document"].get("desktop_evidence", {}).get("imports", [])
        view.setPlainText(json.dumps(evidence[-1] if evidence else {}, indent=2))
        layout.addWidget(view)
        missing = QLabel("Missing: " + "\n".join(snapshot["physical_missing"]) if snapshot["physical_missing"] else
            "Imported prerequisites present. Independently review this board and the passive capture scope.")
        missing.setWordWrap(True); layout.addWidget(missing)
        form = QFormLayout()
        self.board = line(form, "Inspected board / fabrication ID")
        self.sha = line(form, "Full deployed BRINGUP_CAL SHA (40 hex)")
        self.review = line(form, "Bench interlock + clamp risk authorization / evidence")
        layout.addLayout(form)
        self.checks = {}
        for key, text in ACKNOWLEDGEMENTS.items():
            box = QCheckBox(text); self.checks[key] = box; layout.addWidget(box)
        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.button(QDialogButtonBox.Ok).setText("Record operator decision")
        buttons.button(QDialogButtonBox.Ok).setEnabled(not snapshot["physical_missing"])
        buttons.accepted.connect(self.accept); buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

    def arguments(self):
        return dict(board=self.board.text().strip(), deployed_sha=self.sha.text().strip(), review=self.review.text().strip(),
                    checks={key: box.isChecked() for key, box in self.checks.items()})


class MainWindow(QMainWindow):
    def __init__(self, bridge=None, backend=None):
        super().__init__()
        if os.name == "nt":
            # Windows offscreen Qt cannot always enumerate native fonts. Use the
            # installed OS font, not a bundled asset or a new firmware dependency.
            font = Path(os.environ.get("WINDIR", "C:/Windows")) / "Fonts/segoeui.ttf"
            if font.is_file():
                QFontDatabase.addApplicationFont(str(font))
            QApplication.instance().setFont(QFont("Segoe UI", 10))
        self.setWindowTitle("WTK · OSL Calibration Workbench")
        self.resize(1180, 850)
        self.controller = Controller(backend)
        self.inventory = guided.reference_template()
        self.snapshot = None
        self.recapture_pair = None
        self.export_directory = None
        self.allow_close = False
        root = QWidget(); self.setCentralWidget(root); layout = QVBoxLayout(root)
        heading = QLabel("WTK.RLCMeter  /  OSL Calibration")
        heading.setStyleSheet("font-size: 22px; font-weight: 600; color: #144c68")
        layout.addWidget(heading)
        self.banner = QLabel("Prepare references and a campaign. All candidates remain UNQUALIFIED.")
        self.banner.setWordWrap(True); layout.addWidget(self.banner)
        row = QHBoxLayout(); layout.addLayout(row)
        self.mode = QComboBox(); self.mode.addItems(["Simulation", "Physical"]); row.addWidget(self.mode)
        self.address = QComboBox(); self.address.setEditable(True)
        self.address.setSizeAdjustPolicy(QComboBox.AdjustToMinimumContentsLengthWithIcon)
        self.address.setMinimumContentsLength(18)
        if bridge:
            self.address.addItem(str(bridge))
        row.addWidget(self.address, 1)
        self.operator = QLineEdit(); self.operator.setPlaceholderText("Operator identity (required)"); row.addWidget(self.operator)
        self.refresh_button = QPushButton("COM ports"); self.refresh_button.clicked.connect(self.ports); row.addWidget(self.refresh_button)
        self.connect_button = QPushButton("Connect"); self.connect_button.clicked.connect(self.connect_device); row.addWidget(self.connect_button)
        self.disconnect_button = QPushButton("Disconnect"); self.disconnect_button.clicked.connect(lambda: self.controller.submit("disconnect")); row.addWidget(self.disconnect_button)
        self.status_button = QPushButton("Refresh status"); self.status_button.clicked.connect(lambda: self.controller.submit("status")); row.addWidget(self.status_button)
        self.device = QLabel("Device disconnected. No captures or installation performed.")
        self.device.setWordWrap(True); self.device.setTextInteractionFlags(Qt.TextSelectableByMouse); layout.addWidget(self.device)
        actions = QHBoxLayout(); layout.addLayout(actions)
        self.new_button = QPushButton("New campaign…"); self.new_button.clicked.connect(self.new_campaign)
        self.open_button = QPushButton("Open / resume…"); self.open_button.clicked.connect(self.open_campaign)
        self.clear_button = QPushButton("Close campaign"); self.clear_button.clicked.connect(lambda: self.controller.submit("clear"))
        self.references_button = QPushButton("References…"); self.references_button.clicked.connect(self.edit_references)
        self.load_refs = QPushButton("Load inventory…"); self.load_refs.clicked.connect(self.load_inventory)
        self.save_refs = QPushButton("Save inventory…"); self.save_refs.clicked.connect(self.save_inventory)
        self.synthetic_refs = QPushButton("Synthetic inventory"); self.synthetic_refs.clicked.connect(self.synthetic_inventory)
        self.order = QComboBox(); self.order.addItems(["standard", "condition"])
        for widget in (self.new_button, self.open_button, self.clear_button, self.references_button, self.load_refs, self.save_refs, self.synthetic_refs, self.order):
            actions.addWidget(widget)
        self.progress = QProgressBar(); self.progress.setMaximum(99); layout.addWidget(self.progress)
        tabs = QTabWidget(); layout.addWidget(tabs, 1)
        campaign_tab = QWidget(); tabs.addTab(campaign_tab, "Campaign")
        body = QHBoxLayout(campaign_tab)
        self.table = QTableWidget(33, 7)
        self.table.setHorizontalHeaderLabels(["RREF Ω", "Hz", "mVrms", "OPEN", "SHORT", "LOAD", "OSL"])
        self.table.setEditTriggers(QTableWidget.NoEditTriggers)
        self.table.setSelectionBehavior(QTableWidget.SelectItems)
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.table.cellClicked.connect(self.inspect)
        body.addWidget(self.table, 3)
        operation = QWidget(); column = QVBoxLayout(operation); body.addWidget(operation, 2)
        self.operation = QLabel("Create or open a campaign."); self.operation.setWordWrap(True)
        self.operation.setStyleSheet("font-size: 16px; padding: 10px; background: #edf5f8; border: 1px solid #bad0dc")
        column.addWidget(self.operation)
        self.confirm = QCheckBox("I confirm this exact passive, discharged fixture\nand reference for ONE observation.")
        self.confirm.stateChanged.connect(lambda: self.interlocks())
        column.addWidget(self.confirm)
        self.capture_button = QPushButton("Confirm fixture & capture once")
        self.capture_button.setStyleSheet("padding: 12px; font-weight: 600; background: #d9eee7")
        self.capture_button.clicked.connect(self.acquire); column.addWidget(self.capture_button)
        controls = QHBoxLayout(); column.addLayout(controls)
        self.cancel_button = QPushButton("Cancel / pause"); self.cancel_button.clicked.connect(self.controller.cancel); controls.addWidget(self.cancel_button)
        self.recapture_button = QPushButton("Recapture selected…"); self.recapture_button.clicked.connect(self.select_recapture); controls.addWidget(self.recapture_button)
        self.quality = QPlainTextEdit(); self.quality.setReadOnly(True); column.addWidget(self.quality, 1)
        self.build_button = QPushButton("Generate candidate & reports…"); self.build_button.clicked.connect(self.build_candidate); column.addWidget(self.build_button)
        self.output_button = QPushButton("Open output directory"); self.output_button.clicked.connect(self.open_output); column.addWidget(self.output_button)
        evidence_tab = QWidget(); ev = QVBoxLayout(evidence_tab); tabs.addTab(evidence_tab, "Physical evidence")
        reminder = QLabel("R02: R_TFT_LED open until LED input characterized. R05: inspect all Bluepill header/power/SWD/HSE pins.\n"
            "A05-Y04: 5 V analog outputs may inject into 3.3 V clamps. Firmware cannot eliminate this risk.\n"
            "B01 uses BRINGUP; calibration uses a separate BRINGUP_CAL SWD deployment. Grounds: circuit GND only.")
        reminder.setWordWrap(True); ev.addWidget(reminder)
        evrow = QHBoxLayout(); ev.addLayout(evrow)
        self.import_button = QPushButton("Import B01 session / report…"); self.import_button.clicked.connect(self.import_b01); evrow.addWidget(self.import_button)
        self.gate_button = QPushButton("Review physical prerequisites…"); self.gate_button.clicked.connect(self.review_gate); evrow.addWidget(self.gate_button)
        self.evidence_view = QPlainTextEdit(); self.evidence_view.setReadOnly(True); ev.addWidget(self.evidence_view)
        self.scope_tab = ScopeTab(); tabs.addTab(self.scope_tab, "Oscilloscope")
        self.scope_tab.closed.connect(self.finish_close)
        self.log = QPlainTextEdit(); self.log.setReadOnly(True); self.log.setMaximumBlockCount(400); self.log.setMaximumHeight(110); layout.addWidget(self.log)
        self.controller.message.connect(self.log.appendPlainText)
        self.controller.updated.connect(self.update_view)
        self.controller.busy_changed.connect(self.interlocks)
        self.controller.closed.connect(self.finish_close)
        self.mode.currentTextChanged.connect(self.mode_changed)
        self.mode_changed()
        self.interlocks()

    def mode_changed(self):
        self.banner.setText("SIMULATED — C fixture / synthetic acquisition; no physical evidence." if self.mode.currentText() == "Simulation" else
            "PHYSICAL — 3.3 V USART1, 115200 8N1, BRINGUP_CAL only. B01 evidence + operator review required.")

    def ports(self):
        if self.mode.currentText() != "Physical":
            self.log.appendPlainText("Simulation: select the native C fixture executable, or pass --bridge.")
            return
        import serial.tools.list_ports
        self.address.clear()
        for port in serial.tools.list_ports.comports():
            self.address.addItem(port.device)

    def connect_device(self):
        self.controller.submit("connect", mode=self.mode.currentText(), address=self.address.currentText(), operator=self.operator.text().strip())

    def new_campaign(self):
        try:
            guided.references(self.inventory)
        except (ValueError, TypeError, KeyError) as exc:
            self.log.appendPlainText("Edit references first: " + str(exc)); return
        path, _ = QFileDialog.getSaveFileName(self, "New campaign (never overwrite existing evidence)", "", "JSON (*.json)")
        if path:
            self.recapture_pair = None
            self.controller.submit("create", path=path, inventory=self.inventory, order=self.order.currentText())

    def open_campaign(self):
        path, _ = QFileDialog.getOpenFileName(self, "Open canonical campaign", "", "JSON (*.json)")
        if path:
            self.recapture_pair = None
            self.controller.submit("open", path=path)

    def edit_references(self):
        locked = bool(self.snapshot and self.snapshot["document"])
        dialog = ReferencesDialog(self.inventory, locked, self)
        if dialog.exec() == QDialog.Accepted and not locked:
            self.inventory = dialog.inventory
            self.log.appendPlainText("Inventory validated; save it or create a new campaign.")

    def synthetic_inventory(self):
        if self.mode.currentText() == "Simulation":
            self.inventory = guided.synthetic_inventory()
            self.log.appendPlainText("Synthetic inventory loaded — six 1% nominal references, NOT physical standards.")

    def load_inventory(self):
        path, _ = QFileDialog.getOpenFileName(self, "Load existing inventory", "", "JSON (*.json)")
        if path:
            try:
                inventory = guided.read_json(path)
                # Unknown templates can be opened for editing; no campaign until validated.
                dialog = ReferencesDialog(inventory, parent=self)
            except (ValueError, TypeError, KeyError, OSError) as exc:
                self.log.appendPlainText(str(exc)); return
            if dialog.exec() == QDialog.Accepted:
                self.inventory = dialog.inventory

    def save_inventory(self):
        path, _ = QFileDialog.getSaveFileName(self, "Save validated inventory", "", "JSON (*.json)")
        if path:
            try:
                inventory_save(path, self.inventory)
            except (ValueError, OSError, TypeError) as exc:
                self.log.appendPlainText("REJECTED: " + str(exc))

    def acquire(self):
        if not self.confirm.isChecked():
            return
        pair = self.recapture_pair
        if self.controller.submit("acquire", pair=pair, recapture=pair is not None, confirmed=True):
            self.confirm.setChecked(False)

    def select_recapture(self):
        r, c = self.table.currentRow(), self.table.currentColumn()
        if r < 0 or c not in (3, 4, 5):
            self.log.appendPlainText("Select an OPEN, SHORT or LOAD cell first."); return
        if QMessageBox.question(self, "Explicit recapture", "Replace/retry this observation? Any previously accepted raw evidence remains in the audit.") == QMessageBox.Yes:
            self.recapture_pair = pc_osl.KEYS[r], ("OPEN", "SHORT", "LOAD")[c - 3]
            self.confirm.setChecked(False)
            self.render_operation()

    def render_operation(self):
        request = (next((r for r in guided.plan(self.inventory, self.snapshot["document"]["order"]) if guided.pair_id(r) == self.recapture_pair), None)
                   if self.recapture_pair else self.snapshot["next"])
        if not request:
            self.operation.setText("99 observations accepted. Generate an UNQUALIFIED candidate; installation is separate."); return
        key = pc_osl.Key.from_json(request["condition"])
        ref = request["reference"]
        self.operation.setText(("RECAPTURE\n" if self.recapture_pair else "NEXT OBSERVATION\n") +
            f'{request["standard"]} · fixture {request["fixture"]}\nRREF {key.rref_ohms:,} Ω\n{key.frequency_hz:,} Hz · {key.amplitude_mvrms} mVrms\n' +
            (f'LOAD reference {ref["id"]}: {ref["impedance_ohms"]} Ω, ±{ref["tolerance_pct"]:.6g}%\n' if ref else "") +
            "Connect only the identified passive, discharged fixture. Confirm this ONE capture.\nDo not change fixture while busy or teardown UNKNOWN.")

    def inspect(self, row, column):
        if not self.snapshot or not self.snapshot["document"]:
            return
        item = self.snapshot["matrix"][row]
        if column in (3, 4, 5):
            cell = item["cells"][("OPEN", "SHORT", "LOAD")[column - 3]]
            observation = cell["observation"]
            info = dict(state=cell["state"], reason=cell["reason"], solver=item["solver"])
            if observation:
                info.update(quality=guided.quality(observation), sha256=observation["sha256"],
                            acquisition=observation["acquisition"], adc=observation["adc"], phasors=observation["phasors"])
            self.quality.setPlainText(json.dumps(info, indent=2))
        else:
            self.quality.setPlainText(json.dumps(item["solver"] or {"OSL": "Incomplete triplet"}, indent=2))

    def import_b01(self):
        path, _ = QFileDialog.getOpenFileName(self, "Import B01 session/report", "", "JSON (*.json)")
        if path:
            self.controller.submit("evidence", path=path)

    def review_gate(self):
        dialog = GateDialog(self.snapshot, self)
        if dialog.exec() == QDialog.Accepted:
            self.controller.submit("approve", **dialog.arguments())

    def build_candidate(self):
        parent = QFileDialog.getExistingDirectory(self, "Choose parent for a NEW candidate output directory")
        if parent:
            self.export_directory = Path(parent) / ("candidate-" + guided.utc().replace(":", "").replace("+", ""))
            self.controller.submit("build", out=self.export_directory)

    def open_output(self):
        if self.export_directory and self.export_directory.exists():
            QDesktopServices.openUrl(QUrl.fromLocalFile(str(self.export_directory.resolve())))

    @Slot(object)
    def update_view(self, result):
        if result["snapshot"] is None:
            return
        self.snapshot = result["snapshot"]
        snap, doc = self.snapshot, self.snapshot["document"]
        if result["action"] == "clear" and not result["error"]:
            self.recapture_pair = None
            self.inventory = guided.reference_template()
            self.table.clearContents()
            self.progress.setValue(0)
            self.operation.setText("Create or open a campaign.")
            self.quality.clear()
            self.evidence_view.clear()
        if doc:
            self.inventory = copy.deepcopy(doc["inventory"])
            p = snap["progress"]
            self.progress.setValue(p["captured"])
            self.progress.setFormat(f'{p["captured"]}/99 observations · {p["complete_conditions"]}/33 conditions · {snap["remaining_changes"]} remaining fixture changes')
            for row, item in enumerate(snap["matrix"]):
                key = pc_osl.Key.from_json(item["condition"])
                texts = [f'{key.rref_ohms:,}', str(key.frequency_hz), str(key.amplitude_mvrms)] + [item["cells"][s]["state"] for s in ("OPEN", "SHORT", "LOAD")] + ["ACCEPTED" if item["complete"] else "REVIEW" if any(c["state"] == "REJECTED" for c in item["cells"].values()) else "PENDING"]
                for col, value in enumerate(texts):
                    cell = QTableWidgetItem(value)
                    if value in ("ACCEPTED", "REJECTED", "REVIEW", "PENDING"):
                        cell.setBackground(QColor({"ACCEPTED": "#d9eee7", "REJECTED": "#ffe2da", "REVIEW": "#fff0cf", "PENDING": "#f0f2f4"}[value]))
                    cell.setToolTip(json.dumps(item["cells"].get(("OPEN", "SHORT", "LOAD")[col-3], {}).get("reason")) if col in (3, 4, 5) else "")
                    self.table.setItem(row, col, cell)
            if result["action"] == "acquire":
                self.recapture_pair = None
                if doc["captures"]:
                    last = doc["captures"][-1]
                    self.quality.setPlainText(json.dumps(dict(quality=guided.quality(last), sha256=last["sha256"]), indent=2))
            self.render_operation()
            self.evidence_view.setPlainText("Missing physical prerequisites:\n" + "\n".join(snap["physical_missing"]) +
                "\n\n" + json.dumps(doc.get("desktop_evidence", {}), indent=2))
            self.banner.setText(f'{p["evidence"]} · {snap["path"]}\nUNQUALIFIED. Identity hashes are not device authentication. No installation performed.')
        if snap["connected"]:
            identity, state, install = doc["device"], snap["status"], snap["installation"]
            self.device.setText(f'BRINGUP_CAL · HW {identity["hardware_revision"]:#x} · FW {identity["firmware_version"]} / git {identity["git"]} · UID {identity["uid"]}\n'
                f'Capabilities {identity["capabilities"]:#x} · W25Q {"available" if install["storage_available"] else "UNAVAILABLE"} · '
                f'Active sequence {state["calibration_sequence"]} ({"OSL present, UNQUALIFIED" if state["calibrated"] else "UNCALIBRATED"}) · successor {install["next_sequence"]}\n'
                f'Faults {state["safety_faults"]:#x} · blockers {state["safety_blocks"]:#x} · busy {state["busy"]} · transfer SAFE {state["transfer_safe"]} · '
                f'Physical operator gate {"acknowledged" if snap["gate_approved"] else "not acknowledged"}')
        else:
            self.device.setText("Disconnected. " + ("UNKNOWN teardown — do not change fixture; reconnect and verify SAFE." if snap["unknown"] else "No active serial owner."))
        if snap["unknown"]:
            self.operation.setText("UNKNOWN teardown. Keep fixture unchanged. Reconnect / verify SAFE after the firmware's independent timeout.")
        if result["action"] == "build" and not result["error"]:
            candidate = doc["candidate"]
            self.quality.setPlainText("CANDIDATE GENERATED — UNQUALIFIED / NOT INSTALLED\n" + json.dumps(candidate, indent=2))
            self.log.appendPlainText(f'Installation is separate: python Firmware/tools/pc_osl_install.py install --port COM5 --candidate "{self.export_directory / "candidate.bin"}" --report installation.json --readback installed.bin')
        self.confirm.setChecked(False)
        self.interlocks()

    def interlocks(self, *_):
        snap = self.snapshot or {}
        busy, connected = self.controller.busy, snap.get("connected", False)
        self.statusBar().showMessage(("ACQUIRING — keep fixture unchanged; bounded cancellation available" if self.controller.action == "acquire" else
                                     "Working: " + str(self.controller.action)) if busy else
                                    "UNKNOWN teardown — keep fixture unchanged" if snap.get("unknown") else "Idle — each observation requires explicit confirmation")
        campaign = bool(snap.get("document"))
        for widget in (self.new_button, self.open_button):
            widget.setEnabled(not busy and not connected)
        self.clear_button.setEnabled(not busy and not connected and campaign)
        for widget in (self.load_refs, self.synthetic_refs, self.order):
            widget.setEnabled(not busy and not campaign)
        self.synthetic_refs.setEnabled(not busy and not campaign and self.mode.currentText() == "Simulation")
        self.references_button.setEnabled(not busy)
        self.save_refs.setEnabled(not busy)
        for widget in (self.mode, self.address, self.operator, self.refresh_button):
            widget.setEnabled(not busy and not connected)
        self.connect_button.setEnabled(not busy and not connected and campaign)
        self.disconnect_button.setEnabled(not busy and connected)
        self.status_button.setEnabled(not busy and connected)
        safe = connected and not snap.get("unknown", False)
        authorized = snap.get("mode") == "Simulation" or snap.get("gate_approved", False)
        available = bool(snap.get("next")) or self.recapture_pair is not None
        self.capture_button.setEnabled(not busy and safe and authorized and available and self.confirm.isChecked())
        self.confirm.setEnabled(not busy and safe and authorized and available)
        self.recapture_button.setEnabled(not busy and safe and authorized and campaign)
        self.cancel_button.setEnabled(busy and self.controller.action == "acquire" and not self.controller.closing)
        self.import_button.setEnabled(not busy and campaign)
        self.gate_button.setEnabled(not busy and connected and snap.get("mode") == "Physical")
        self.build_button.setEnabled(not busy and campaign and not snap["progress"]["missing"] and not snap.get("unknown", False))
        self.output_button.setEnabled(not busy and bool(self.export_directory and self.export_directory.exists()))

    def closeEvent(self, event):
        if self.allow_close:
            event.accept(); return
        event.ignore()
        self.setEnabled(False)
        self.log.appendPlainText("Closing: cancel/drain acquisition and release the serial owner. No fixture changes until SAFE is known.")
        self.scope_tab.shutdown()
        self.controller.shutdown()

    @Slot()
    def finish_close(self):
        if self.controller.thread.isRunning() or self.scope_tab.thread.isRunning():
            return  # Both independent workers must drain before QObject destruction.
        self.allow_close = True
        self.close()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bridge", type=Path, help="Native C fixture executable for Simulation only")
    parser.add_argument("--campaign", type=Path, help="Open an existing campaign")
    parser.add_argument("--smoke-test", action="store_true", help="Startup/cleanup only; no serial requests")
    args = parser.parse_args(argv)
    app = QApplication.instance() or QApplication(sys.argv[:1])
    app.setStyle("Fusion")
    bridge = args.bridge
    if bridge is None and getattr(sys, "frozen", False):
        bridge = Path(sys._MEIPASS) / "simulation" / "wtk_pc_cal_capture_bridge.exe"
    window = MainWindow(bridge)
    window.show()
    if args.campaign:
        window.controller.submit("open", path=args.campaign)
    if args.smoke_test:
        QTimer.singleShot(100, window.close)
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())

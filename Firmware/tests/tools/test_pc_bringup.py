"""B01 synthetic/offline tests. No electrical qualification or active commands."""
from pathlib import Path
import copy
import json
import re
import sys
import tempfile
import unittest
from unittest.mock import patch, MagicMock

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import pc_bringup as b


class BringupTests(unittest.TestCase):
    def setUp(self):
        self.session = b.template()
        self.session["firmware"]["profile"] = "BRINGUP"
        self.fixture = b.load_json(b.FIXTURE)

    def collect(self, fixture=None, **options):
        link = b.FakeSerial(fixture or self.fixture, **options)
        events = b.collect(link, self.session, simulated=True, clock=lambda: link.now, stamp=lambda: "SYNTHETIC", boot_timeout=3, timeout=3)
        return link, events, b.make_report(self.session, events)

    def status(self, report, key):
        return next(c["status"] for c in report["checks"] if c["check"] == key)

    def mutate(self, command, old, new):
        self.fixture["responses"][command] = self.fixture["responses"][command].replace(old, new)
        return self.collect()

    def test_allowlist_exact(self):
        self.assertEqual(len(b.COMMANDS), 9)
        for command in b.COMMANDS:
            self.assertEqual(b.command_bytes(command), command.encode()+b"\n")

    def test_active_and_injected_commands_refused(self):
        for command in ("lab flash selftest", "lab flash erase", "lab flash program", "lab range 1m",
                        "lab range off", "lab relay measure", "lab k1 measure", "lab excitation start",
                        "lab metrology capture 100 100 1m", "lab cal acquire OPEN 100 100 1m",
                        "INSTALL_BEGIN", "reset", "lab fault clear", "lab safety override",
                        "lab fault status\nlab range 1m", "lab fault status\r", "LAB FAULT STATUS"):
            with self.subTest(command=command), self.assertRaises(ValueError):
                b.command_bytes(command)

    def test_complete_blank_snapshot(self):
        link, events, report = self.collect()
        self.assertEqual(link.writes, [b.command_bytes(c) for c in b.COMMANDS])
        self.assertEqual(sum(e["kind"] == "RESPONSE" for e in events), 9)
        self.assertTrue(report["SIMULATED"])
        self.assertFalse(report["physically_validated"])
        self.assertEqual(report["result"], "NOT_TESTED")
        self.assertEqual(self.status(report, "lab cal status"), "WARNING")
        for key in ("R02", "R05", "powerup_waveforms", "bat54s_conduction"):
            self.assertEqual(self.status(report, key), "NOT_TESTED")

    def test_all_packet_splits(self):
        for chunk in (1, 2, 3, 17, 127, 128):
            with self.subTest(chunk=chunk):
                link = b.FakeSerial(self.fixture, chunk=chunk)
                events = b.collect(link, self.session, simulated=True, clock=lambda: link.now, timeout=30, boot_timeout=30)
                self.assertEqual(sum(e["kind"] == "RESPONSE" for e in events), 9)

    def test_cr_lf_variants(self):
        for separator in ("\n", "\r", "\r\n"):
            with self.subTest(separator=separator):
                fixture = copy.deepcopy(self.fixture)
                fixture["banner"] = fixture["banner"].replace("\r\n", separator)
                fixture["responses"] = {c: s.replace("\r\n", separator) for c, s in fixture["responses"].items()}
                self.assertEqual(len(self.collect(fixture)[0].writes), 9)

    def test_delayed_output(self):
        _, events, _ = self.collect(delay_reads=20)
        self.assertFalse(any(e["kind"] == "ERROR" for e in events))

    def test_interleaved_logs_retained(self):
        self.fixture["responses"][b.COMMANDS[0]] = "[100] aux sensors ready\n" + self.fixture["responses"][b.COMMANDS[0]]
        _, events, report = self.collect()
        self.assertTrue(any(e["kind"] == "INTERLEAVED" for e in events))
        self.assertEqual(self.status(report, "interleaved_output"), "WARNING")

    def test_actual_post_banner_startup_before_requests(self):
        link, events, report = self.collect()
        marker = next(i for i, e in enumerate(events) if e["kind"] == "STARTUP" and e["key"] == "safety_block")
        request = next(i for i, e in enumerate(events) if e["kind"] == "TX")
        self.assertLess(marker, request)
        self.assertEqual(len(link.writes), 9)
        self.assertEqual(self.status(report, "lab charger status"), "PASS")  # Startup UNKNOWN is not its response.
        self.fixture["startup"] = self.fixture["startup"].replace("k1: OK", "k1: ERROR")
        self.assertEqual(self.collect()[2]["result"], "STOP")

    def test_missing_console_startup_marker_sends_nothing(self):
        self.fixture["startup"] = ""
        link, _, report = self.collect()
        self.assertEqual(link.writes, [])
        self.assertEqual(report["result"], "UNKNOWN")

    def test_actual_async_safety_display_lines(self):
        self.fixture["responses"][b.COMMANDS[0]] = "display: READY\r\nsafety_block: BLOCKED_RANGE\r\n" + self.fixture["responses"][b.COMMANDS[0]]
        self.assertEqual(len(self.collect()[0].writes), 9)
        self.fixture["responses"][b.COMMANDS[8]] += "safety_block: BLOCKED_FAULT\r\n"
        self.assertEqual(self.collect()[2]["result"], "STOP")

    def test_log_interleaved_inside_multiline_response(self):
        self.fixture["responses"][b.COMMANDS[2]] = self.fixture["responses"][b.COMMANDS[2]].replace("lab sensors ov_raw:", "[110] sensor sweep\r\nlab sensors ov_raw:")
        link, _, report = self.collect()
        self.assertEqual(len(link.writes), 9)
        self.assertEqual(self.status(report, b.COMMANDS[2]), "PASS")

    def test_fault_bit_cannot_masquerade_as_range_block(self):
        _, _, report = self.mutate(b.COMMANDS[3], "flags=8", "flags=32")
        self.assertEqual(self.status(report, b.COMMANDS[3]), "STOP")

    def test_boot_clock_fault_is_stop(self):
        self.fixture["banner"] = self.fixture["banner"].replace("clock_status: OK", "clock_status: ERROR")
        self.assertEqual(self.status(self.collect()[2], "boot_diagnostics"), "STOP")

    def test_truncated_calibration_response_unknown(self):
        self.fixture["responses"][b.COMMANDS[8]] = self.fixture["responses"][b.COMMANDS[8]].splitlines()[0]+"\r\nCAL_SLOT unavailable\r\n"
        self.assertEqual(self.collect()[2]["result"], "UNKNOWN")

    def test_event_bound_still_reports_failure(self):
        with patch.object(b, "MAX_EVENTS", 20):
            _, events, report = self.collect(chunk=1)
        self.assertLessEqual(len(events), 20)
        self.assertEqual(report["result"], "UNKNOWN")

    def test_rx_bound_still_reports_failure(self):
        with patch.object(b, "MAX_RX", 200):
            _, _, report = self.collect()
        self.assertEqual(report["result"], "UNKNOWN")

    def test_report_reparses_lines_and_missing_never_pass(self):
        _, events, _ = self.collect()
        response = next(e for e in events if e["kind"] == "RESPONSE" and e["command"] == b.COMMANDS[0])
        response["lines"] = ["lab fault: 0x00000020"]
        response["values"] = {"fault_mask": 0}
        self.assertEqual(self.status(b.make_report(self.session, events), b.COMMANDS[0]), "STOP")
        self.assertEqual(self.status(b.make_report(self.session, []), b.COMMANDS[0]), "NOT_TESTED")

    def test_timeout_stops_next_command_and_reconnect_requires_banner(self):
        self.fixture["responses"][b.COMMANDS[1]] = ""
        link, events, report = self.collect()
        self.assertEqual(len(link.writes), 2)
        self.assertEqual(self.status(report, "collection"), "UNKNOWN")
        self.assertEqual(self.status(report, b.COMMANDS[1]), "NOT_TESTED")
        self.assertFalse(report["physically_validated"])
        self.assertTrue(any("deadline" in e.get("reason", "") for e in events))
        # No retry or reconnection silently assumes retained identity.
        self.fixture["banner"] = ""
        self.assertEqual(self.collect()[0].writes, [])
        self.fixture = b.load_json(b.FIXTURE)
        self.assertEqual(len(self.collect()[0].writes), 9)

    def test_partial_packet_timeout(self):
        self.fixture["responses"][b.COMMANDS[0]] = "lab fault: 0x0000"
        link, _, report = self.collect()
        self.assertEqual(len(link.writes), 1)
        self.assertNotEqual(self.status(report, b.COMMANDS[0]), "PASS")
        link = b.FakeSerial(b.load_json(b.FIXTURE))
        link.write = lambda data: len(data)-1
        events = b.collect(link, self.session, simulated=True, clock=lambda: link.now)
        self.assertEqual(b.make_report(self.session, events)["result"], "UNKNOWN")
        self.assertEqual(sum(e["kind"] == "TX" for e in events), 1)

    def test_unexpected_reset(self):
        self.fixture["responses"][b.COMMANDS[2]] = self.fixture["banner"]
        link, _, report = self.collect()
        self.assertEqual(len(link.writes), 3)
        self.assertEqual(report["result"], "STOP")
        self.assertIn("STOP active work", report["next_safe_step"])

    def test_disconnection_recorded_and_preserved_in_replay(self):
        link = b.FakeSerial(self.fixture)
        def disconnected(data):
            raise OSError("cable disconnected")
        link.write = disconnected
        events = b.collect(link, self.session, simulated=True, clock=lambda: link.now)
        self.assertEqual(b.make_report(self.session, events)["result"], "STOP")
        self.assertEqual(b.make_report(self.session, b.replay(events), True)["result"], "STOP")

    def test_unknown_firmware_no_transmission(self):
        for banner in ("WTK.RLCMeter SAFE_BOOT\n", "Other instrument\n", "PLC1\x01\n", ""):
            with self.subTest(banner=banner):
                self.fixture["banner"] = banner
                link, _, report = self.collect()
                self.assertEqual(link.writes, [])
                self.assertEqual(report["result"], "UNKNOWN")

    def test_wrong_profile_and_hardware_banner(self):
        for old, new in (("profile: BRINGUP", "profile: BRINGUP_CAL"),
                         ("profile: BRINGUP", "profile: PRODUCT"),
                         ("firmware: 0.1.0", "firmware: UNKNOWN"),
                         ("hardware: Rev1-STM32F103C8T6-BluePill", "hardware: Unknown")):
            fixture = copy.deepcopy(self.fixture); fixture["banner"] = fixture["banner"].replace(old, new)
            self.assertEqual(self.collect(fixture)[0].writes, [])

    def test_declared_wrong_mode_refused_before_read(self):
        for mode in ("PRODUCT", "BRINGUP_CAL", "UNKNOWN"):
            self.session["firmware"]["profile"] = mode
            link = MagicMock()
            with self.assertRaises(ValueError):
                b.collect(link, self.session, simulated=True)
            link.read.assert_not_called(); link.write.assert_not_called()

    def test_expected_sha_mismatch(self):
        self.session["firmware"]["expected_sha"] = "f"*40
        self.assertEqual(self.collect()[0].writes, [])

    def test_charger_present(self):
        _, _, report = self.mutate(b.COMMANDS[1], "ABSENT", "PRESENT")
        self.assertEqual(self.status(report, b.COMMANDS[1]), "STOP")

    def test_safety_fault(self):
        _, _, report = self.mutate(b.COMMANDS[0], "00000000", "00000004")
        self.assertEqual(self.status(report, b.COMMANDS[0]), "STOP")

    def test_invalid_sensor_state(self):
        for old, new, expected in (("vmid_raw: 2048 valid=1", "vmid_raw: 2048 valid=0", "UNKNOWN"),
                                   ("residual_state: SAFE", "residual_state: UNSAFE", "STOP"),
                                   ("residual_state: SAFE", "residual_state: SATURATED", "STOP"),
                                   ("battery_state: OK", "battery_state: CRITICAL", "STOP"),
                                   ("age_ms=10", "age_ms=51", "UNKNOWN"),
                                   ("ntc_temperature_valid=1", "ntc_temperature_valid=0", "UNKNOWN"),
                                   ("vmid: 1650mV", "vmid: 2100mV", "UNKNOWN")):
            fixture = copy.deepcopy(self.fixture); fixture["responses"][b.COMMANDS[2]] = fixture["responses"][b.COMMANDS[2]].replace(old, new)
            self.assertEqual(self.status(self.collect(fixture)[2], b.COMMANDS[2]), expected)

    def test_no_mcu_rail_claim(self):
        _, events, report = self.collect()
        sensor = next(e["values"] for e in events if e.get("command") == b.COMMANDS[2] and e["kind"] == "RESPONSE")
        self.assertEqual(sensor["source"], "MCU_ADC_NOMINAL_3V3_SCALE")
        self.assertNotIn("3v3", sensor)
        self.assertIn("NOT a measurement of the 3V3 rail", b.markdown(report))

    def test_absent_flash(self):
        self.fixture["responses"][b.COMMANDS[6]] = "lab flash info: NOT_DETECTED\r\n"
        self.fixture["responses"][b.COMMANDS[8]] = "\r\n".join(self.fixture["responses"][b.COMMANDS[8]].splitlines()[:3]) + "\r\nCAL_SLOT unavailable\r\n"
        _, _, report = self.collect()
        self.assertEqual(self.status(report, b.COMMANDS[6]), "STOP")

    def test_unexpected_flash_part_warning(self):
        _, _, report = self.mutate(b.COMMANDS[6], "00EF4017 8388608", "00EF4016 4194304")
        self.assertEqual(self.status(report, b.COMMANDS[6]), "WARNING")

    def test_active_range_and_k1_stop(self):
        for command, old, new in ((b.COMMANDS[4], "DISABLED", "READY"), (b.COMMANDS[7], "k1=SAFE", "k1=MEASURE")):
            fixture = copy.deepcopy(self.fixture); fixture["responses"][command] = fixture["responses"][command].replace(old, new)
            self.assertEqual(self.status(self.collect(fixture)[2], command), "STOP")

    def test_inconsistent_safety_stop(self):
        _, _, report = self.mutate(b.COMMANDS[3], "allowed=0", "allowed=1")
        self.assertEqual(self.status(report, b.COMMANDS[3]), "STOP")

    def test_no_false_pass_unknown_response(self):
        for command, old, new in ((b.COMMANDS[1], "ABSENT", "BANANA"),
                                 (b.COMMANDS[6], "8388608", "NaN"),
                                 (b.COMMANDS[8], "svc=NO_VALID_CALIBRATION", "svc=UNKNOWN"),
                                 (b.COMMANDS[2], "vmid_raw: 2048", "vmid_raw: 4096")):
            fixture = copy.deepcopy(self.fixture); fixture["responses"][command] = fixture["responses"][command].replace(old, new)
            link, _, report = self.collect(fixture)
            self.assertNotEqual(self.status(report, command), "PASS")
            self.assertEqual(report["result"], "UNKNOWN")
            self.assertEqual(len(link.writes), b.COMMANDS.index(command)+1)

    def test_duplicate_and_stale_response(self):
        # A late response is not interpreted as the following command.
        self.fixture["responses"][b.COMMANDS[0]] *= 2
        _, _, report = self.collect()
        self.assertEqual(report["result"], "UNKNOWN")
        self.assertNotEqual(self.status(report, b.COMMANDS[1]), "PASS")

    def test_duplicate_final_response_unknown(self):
        self.fixture["responses"][b.COMMANDS[8]] *= 2
        self.assertEqual(self.collect()[2]["result"], "UNKNOWN")

    def test_truncated_transcript_and_bad_events(self):
        _, events, _ = self.collect()
        self.assertEqual(b.make_report(self.session, b.replay(events[:-1]), True)["result"], "UNKNOWN")
        for invalid in ({}, [], ["text"], [{"kind": "ERROR", "status": "PASS", "reason": "bad"}], [{"kind": "RX", "hex": "01z"}], [{"kind": "RX", "hex": "aa"*129}]):
            with self.assertRaises(ValueError):
                b.replay(invalid)

    def test_duplicate_sensor_field(self):
        _, _, report = self.mutate(b.COMMANDS[2], "lab sensors vmid: 1650mV", "lab sensors vmid_raw: 2048 valid=1")
        self.assertEqual(report["result"], "UNKNOWN")

    def test_active_calibration_metadata(self):
        _, _, report = self.mutate(b.COMMANDS[8], "svc=NO_VALID_CALIBRATION act=0", "svc=ACTIVE_VALID act=1 slot=A seq=1 rec=33")
        self.assertEqual(self.status(report, b.COMMANDS[8]), "PASS")
        self.assertIn("NOT an accuracy certificate", b.markdown(report))

    def test_wrong_schema_no_pass(self):
        _, _, report = self.mutate(b.COMMANDS[8], "CAL sch=2", "CAL sch=3")
        self.assertEqual(self.status(report, b.COMMANDS[8]), "UNKNOWN")

    def test_bounded_line_and_binary_rejected(self):
        for response in ("x"*257 + "\n", "lab fault: \x01\n"):
            self.fixture["responses"][b.COMMANDS[0]] = response
            self.assertEqual(self.collect()[2]["result"], "UNKNOWN")

    def test_invalid_timeouts(self):
        for timeout in (0, -1, float("nan"), float("inf"), 61):
            with self.assertRaises(ValueError):
                b.collect(MagicMock(), self.session, simulated=True, timeout=timeout)

    def test_template_no_nominal_measurements(self):
        for entry in b.template()["manual"].values():
            self.assertIsNone(entry["value"])
            self.assertEqual(entry["status"], "NOT_TESTED")
        self.assertEqual(b.make_report(self.session, [])["result"], "UNKNOWN")
        for key in ("board", "firmware", "modules", "instruments", "manual"):
            invalid = copy.deepcopy(self.session); invalid[key] = None
            with self.assertRaises(ValueError):
                b.validate_session(invalid)

    def test_manual_pass_needs_provenance(self):
        entry = self.session["manual"]["3v3"]
        entry.update(status="PASS", value=3.3, unit="V")
        with self.assertRaises(ValueError):
            b.validate_session(self.session)
        entry.update(operator="Operator", instrument="DMM-ID", notes="Independent rail reading", evidence=["rail-photo.png"], timestamp_utc="2026-10-10T00:00:00Z")
        b.validate_session(self.session)
        entry["value"] = 5
        with self.assertRaises(ValueError):
            b.validate_session(self.session)
        entry["value"] = 3.3
        clamp = self.session["manual"]["bat54s_conduction"]
        clamp.update(status="PASS", operator="Operator", instrument="Scope-ID", notes="Later separately authorized capture",
                     evidence=["clamp-trace.csv"], timestamp_utc="2026-10-10T00:00:00Z")
        with self.assertRaises(ValueError):
            b.validate_session(self.session)
        clamp["authorization_reference"] = "owner-signed-later-test-plan.md"
        b.validate_session(self.session)

    def test_real_mode_requires_physical_gates(self):
        link = MagicMock()
        with self.assertRaises(ValueError):
            b.collect(link, self.session)
        link.read.assert_not_called(); link.write.assert_not_called()
        for key in b.SERIAL_GATES:
            entry = self.session["manual"][key]
            entry.update(status="PASS", notes="Recorded", instrument="instrument-ID", operator="Operator", evidence=["real-check.pdf"], timestamp_utc="2026-10-10T00:00:00Z")
            if b.MANUAL[key][2]:
                entry.update(value=3.3 if key in ("3v3", "w25q_supply") else 5, unit="V")
        b.require_serial_gates(self.session)
        self.session["manual"]["R02"]["simulated"] = True
        with self.assertRaises(ValueError):
            b.require_serial_gates(self.session)

    def test_deterministic_replay(self):
        _, events, original = self.collect()
        parsed1, parsed2 = b.replay(events), b.replay(events)
        self.assertEqual(parsed1, parsed2)
        report = b.make_report(self.session, parsed1, True)
        self.assertEqual(report["checks"], original["checks"])
        self.assertTrue(report["OFFLINE_REPLAY"])
        self.assertTrue(report["SIMULATED"])

    def test_replay_does_not_trust_saved_parsed_values(self):
        _, events, _ = self.collect()
        for event in events:
            if event.get("kind") == "RESPONSE":
                event["values"] = {"fault_mask": 0xffffffff}
        replayed = b.replay(events)
        self.assertEqual(self.status(b.make_report(self.session, replayed, True), b.COMMANDS[0]), "PASS")

    def test_replay_active_tx_rejected(self):
        _, events, _ = self.collect()
        for event in events:
            if event["kind"] == "TX":
                event["hex"] = b"lab range 1m\n".hex()
                break
        self.assertEqual(b.make_report(self.session, b.replay(events), True)["result"], "UNKNOWN")

    def test_report_bundle_raw_and_manual_preserved(self):
        _, events, _ = self.collect()
        self.session["notes"] = ["Owner note"]
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory) / "report"
            report = b.save_bundle(out, self.session, events)
            self.assertEqual(b.load_json(out/"report.json"), report)
            self.assertEqual((out/"uart-rx.bin").read_bytes(), b"".join(bytes.fromhex(e["hex"]) for e in events if e["kind"] == "RX"))
            self.assertEqual(report["session"]["notes"], ["Owner note"])
            self.assertIn("**SIMULATED**", (out/"report.md").read_text())
            self.assertIn("never VMID", b.markdown(report))
            with self.assertRaises(FileExistsError):
                b.save_bundle(out, self.session, events)

    def test_clamp_estimate_interval_and_no_safety_claim(self):
        estimate = b.clamp_estimate(4, 3.5, 1000, .01, 10)
        self.assertAlmostEqual(estimate["resistor_current_a"], .0005)
        self.assertLess(estimate["interval_a"][0], .0005)
        self.assertGreater(estimate["interval_a"][1], .0005)
        self.assertIn("Not proof", estimate["meaning"])
        for args in ((4, 3.5, 0, .01, 0), (4, 3.5, 1000, -.01, 0), (4, 3.5, 1000, .01, 1000), (float("nan"), 3.5, 1000, .01, 0)):
            with self.assertRaises(ValueError):
                b.clamp_estimate(*args)

    def test_no_reset_control_lines_on_serial_open(self):
        serial = MagicMock()
        with patch.dict(sys.modules, {"serial": serial}):
            link = b.open_serial("COM5")
        serial.Serial.assert_called_once_with(port=None, baudrate=115200, bytesize=8, parity="N", stopbits=1,
            timeout=.05, write_timeout=1, xonxoff=False, rtscts=False, dsrdtr=False)
        self.assertFalse(link.dtr); self.assertFalse(link.rts)
        self.assertEqual(link.port, "COM5")
        link.open.assert_called_once()

    def test_cli_offline_prepare_fake_replay(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); session = root/"session.json"
            self.assertEqual(b.main(["prepare", "--session", str(session)]), 0)
            self.assertEqual(b.main(["fake", "--session", str(session), "--out", str(root/"fake")]), 0)
            self.assertEqual(b.main(["replay", "--session", str(session), "--transcript", str(root/"fake/transcript.json"), "--out", str(root/"replay")]), 0)
            self.assertTrue(b.load_json(root/"replay/report.json")["SIMULATED"])
            with self.assertRaises(SystemExit):
                b.main(["prepare", "--session", str(session)])

    def test_cli_record_retains_manual_history(self):
        with tempfile.TemporaryDirectory() as directory:
            session = Path(directory)/"session.json"
            b.write_json(session, self.session)
            args = ["record", "--session", str(session), "--check", "R02", "--status", "WARNING",
                    "--instrument", "inspection", "--operator", "Owner", "--evidence", "photo.png", "--notes", "Link remains open"]
            self.assertEqual(b.main(args), 0)
            updated = b.load_json(session)
            self.assertEqual(updated["manual_history"][0]["previous"], self.session["manual"]["R02"])
            self.assertEqual(updated["manual"]["R05"], self.session["manual"]["R05"])

    def test_cli_refuses_before_serial_open(self):
        with tempfile.TemporaryDirectory() as directory:
            session = Path(directory)/"session.json"
            b.write_json(session, self.session)
            with patch.object(b, "open_serial") as serial, self.assertRaises(SystemExit):
                b.main(["snapshot", "--session", str(session), "--port", "COM5", "--out", str(Path(directory)/"out")])
            serial.assert_not_called()

    def test_cli_failed_open_produces_stop_bundle(self):
        with tempfile.TemporaryDirectory() as directory:
            session = Path(directory)/"session.json"; out = Path(directory)/"out"
            b.write_json(session, self.session)
            with patch.object(b, "require_serial_gates"), patch.object(b, "open_serial", side_effect=OSError("COM port missing")):
                self.assertEqual(b.main(["snapshot", "--session", str(session), "--port", "COM5", "--out", str(out)]), 2)
            report = b.load_json(out/"report.json")
            self.assertEqual(report["result"], "STOP")
            self.assertFalse(report["SIMULATED"])
            self.assertEqual((out/"uart-rx.bin").read_bytes(), b"")

    def test_source_command_branches_and_status_writers(self):
        firmware = Path(__file__).resolve().parents[2]
        source = (firmware/"src/app/app_bringup_console.c").read_text()
        calls = (None, None, "write_sensors_status", "write_safety_status", "write_range_status", "write_adc_status", "write_flash_info", "write_mvp_status", "write_calibration_status")
        for command, writer in zip(b.COMMANDS, calls):
            found = re.search(r'else if \(text_equals\(line, "' + re.escape(command) + r'"\)\)\s*\{(.*?)\n    \}', source, re.S)
            self.assertIsNotNone(found, command)
            body = found[1]
            self.assertFalse(any(token in body for token in ("request", "start", "erase", "program", "force", "rescan", "clear", "cancel")))
            if writer:
                self.assertIn(writer+"(", body)
                definition = re.search(r'static void '+writer+r'\([^;]+?\n\{(.*?)\n\}', source, re.S)[1]
                self.assertFalse(any(token in definition for token in ("erase_sector", "program_start", "hw_range_request", "hw_k1_request", "rescan_calibration")))
        banner = (firmware/"src/bsp/bsp_diagnostics.c").read_text()
        for key in b.identity_from_lines(self.fixture["banner"].splitlines()):
            self.assertIn('"'+key+'"', banner)
        for literal in ("lab sensors vmid_raw: ", "lab sensors ntc: raw=", "MVP_BEGIN\\r\\nclock=", "CAL sch=", "CAL_SLOT unavailable\\r\\n"):
            self.assertIn('"'+literal+'"', source)
        shell = (firmware/"src/app/app_shell.c").read_text()
        for key in b.STARTUP_KEYS:
            self.assertRegex(shell, '"'+re.escape(key)+r'(?:: |")')


if __name__ == "__main__":
    unittest.main()

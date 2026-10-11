"""Guided campaign tests; physical hardware is never claimed qualified."""
import copy
import hashlib
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
import types
import unittest
from unittest.mock import patch
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import pc_cal_capture as capture
import pc_osl
import pc_osl_campaign as guided
import pc_osl_install as install

BRIDGE = os.environ.get("WTK_CAPTURE_BRIDGE")


class InventoryTests(unittest.TestCase):
    def setUp(self):
        self.inventory = guided.synthetic_inventory()

    def test_unknown_template_never_passes(self):
        template = guided.reference_template()
        self.assertIsNone(template["references"][0]["published_tolerance_pct"])
        self.assertIsNone(template["references"][0]["actual_impedance_ohms"])
        with self.assertRaises(ValueError):
            guided.references(template)

    def test_six_ordinary_one_percent_references(self):
        refs = guided.references(self.inventory)
        self.assertEqual(set(refs), {10, 100, 1000, 10000, 100000, 1000000})
        self.assertTrue(all(ref["tolerance_pct"] == 1 for ref in refs.values()))
        self.assertNotIn("qualified", refs[10])

    def test_missing_zero_nonfinite_tolerance(self):
        for value in (None, 0, -1, 100, float("nan"), True):
            with self.subTest(value=value):
                self.inventory["references"][0]["published_tolerance_pct"] = value
                with self.assertRaises(ValueError):
                    guided.references(self.inventory)

    def test_duplicate_missing_reference(self):
        self.inventory["references"][1]["id"] = self.inventory["references"][0]["id"]
        with self.assertRaisesRegex(ValueError, "duplicate"):
            guided.references(self.inventory)
        self.inventory = guided.synthetic_inventory()
        self.inventory["references"].pop()
        with self.assertRaisesRegex(ValueError, "six"):
            guided.references(self.inventory)

    def test_actual_value_retains_nominal_interval(self):
        ref = self.inventory["references"][0]
        ref.update(actual_impedance_ohms=[10.05, 0], measurement=dict(
            instrument="Synthetic DMM", operator="Synthetic operator", date="2026-10-10", evidence="synthetic-only"))
        result = guided.references(self.inventory)[10]
        self.assertEqual(result["impedance_ohms"], [10.05, 0])
        self.assertAlmostEqual(result["tolerance_pct"], (0.1 + 0.05) / 10.05 * 100)

    def test_actual_value_needs_provenance(self):
        self.inventory["references"][0]["actual_impedance_ohms"] = [10.05, 0]
        with self.assertRaisesRegex(ValueError, "provenance"):
            guided.references(self.inventory)

    def test_measured_interval_is_explicit_not_qualification(self):
        ref = self.inventory["references"][0]
        ref.update(actual_impedance_ohms=[10.05, 0], measured_interval_ohms=[10.04, 10.06],
                   measurement=dict(instrument="Synthetic DMM", operator="Synthetic operator", date="2026-10-10", evidence="test"))
        self.assertAlmostEqual(guided.references(self.inventory)[10]["tolerance_pct"], 0.01 / 10.05 * 100)
        ref["measured_interval_ohms"] = [10.06, 10.08]
        with self.assertRaisesRegex(ValueError, "inside"):
            guided.references(self.inventory)

    def test_reference_units_assumptions_and_passivity(self):
        for field, value in (("unit", "kohm"), ("fixture_id", "UNKNOWN"),
                             ("temperature_assumptions", ""), ("frequency_assumptions", ""),
                             ("nominal_impedance_ohms", [-10, 0])):
            inventory = copy.deepcopy(self.inventory)
            inventory["references"][0][field] = value
            with self.assertRaises(ValueError):
                guided.references(inventory)

    def test_complex_reference_retains_explicit_radial_tolerance(self):
        self.inventory["references"][0]["nominal_impedance_ohms"] = [10, -1]
        self.assertEqual(guided.references(self.inventory)[10]["impedance_ohms"], [10, -1])


class PlanningPersistenceTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / "campaign.json"
        self.campaign = guided.Campaign.create(self.path, guided.synthetic_inventory())

    def test_default_order_99_unique_pairs_eight_fixture_setups(self):
        plan = self.campaign.missing()
        self.assertEqual(len(plan), 99)
        self.assertEqual(len({guided.pair_id(row) for row in plan}), 99)
        self.assertEqual([row["standard"] for row in plan[:33]], ["OPEN"] * 33)
        self.assertEqual([row["standard"] for row in plan[33:66]], ["SHORT"] * 33)
        self.assertEqual(sum(i == 0 or row["fixture"] != plan[i - 1]["fixture"] for i, row in enumerate(plan)), 8)
        self.assertFalse(any(row["condition"]["rref_ohms"] == 10 and row["condition"]["amplitude_mvrms"] == 500 for row in plan))

    def test_condition_order(self):
        plan = guided.plan(self.campaign.document["inventory"], "condition")
        self.assertEqual([row["standard"] for row in plan[:3]], ["OPEN", "SHORT", "LOAD"])
        self.assertEqual(plan[0]["condition"], plan[2]["condition"])
        with self.assertRaises(ValueError):
            guided.plan(self.campaign.document["inventory"], "wrong")

    def test_offline_unknown_identity_no_false_completion(self):
        document = guided.Campaign.load(self.path).document
        self.assertIsNone(document["device"])
        self.assertIsNone(document["adc"])
        self.assertEqual(self.campaign.progress()["complete_conditions"], 0)
        with self.assertRaisesRegex(ValueError, "incomplete"):
            self.campaign.build()
        self.assertIn("REQUIRES_BENCH_VALIDATION", self.campaign.report())

    def test_existing_campaign_not_overwritten(self):
        before = self.path.read_bytes()
        with self.assertRaises(ValueError):
            guided.Campaign.create(self.path, guided.synthetic_inventory())
        self.assertEqual(self.path.read_bytes(), before)

    def test_manifest_edit_and_duplicate_json_rejected(self):
        data = self.path.read_text()
        self.path.write_text(data.replace('"standard"', '"condition"', 1))
        with self.assertRaisesRegex(ValueError, "integrity"):
            guided.Campaign.load(self.path)
        self.path.write_text('{"format": "a", "format": "b"}')
        with self.assertRaisesRegex(ValueError, "duplicate JSON"):
            guided.Campaign.load(self.path)

    def test_atomic_failure_retains_progress_and_removes_temporary(self):
        before = self.path.read_bytes()
        self.campaign.document["order"] = "condition"
        with patch.object(guided.os, "replace", side_effect=OSError("interrupted replace")):
            with self.assertRaises(OSError):
                self.campaign.save()
        self.assertEqual(self.path.read_bytes(), before)
        self.assertEqual(list(self.path.parent.glob("*.tmp")), [])
        self.assertEqual(guided.Campaign.load(self.path).document["order"], "standard")

    def test_input_size_bounded(self):
        with patch.object(guided, "MAX_DOCUMENT", 1):
            with self.assertRaisesRegex(ValueError, "8 MiB"):
                guided.Campaign.load(self.path)

    def test_stale_writer_cannot_replace_newer_progress(self):
        stale = guided.Campaign.load(self.path)
        self.campaign.document["connections"].append(dict(note="newer evidence"))
        self.campaign.save()
        before = self.path.read_bytes()
        with self.assertRaisesRegex(ValueError, "changed since load"):
            stale.save()
        self.assertEqual(self.path.read_bytes(), before)

    def test_progress_lock_excludes_simultaneous_writer_then_releases(self):
        with guided.progress_lock(self.path):
            with self.assertRaisesRegex(ValueError, "busy"):
                self.campaign.save()
        self.campaign.save()
        guided.Campaign.load(self.path)

    def test_real_com_configuration_and_operator_requirement(self):
        class Transport:
            def __init__(self, **kwargs):
                self.config = kwargs
                self.opened = False
            def open(self):
                self.opened = True
            def close(self):
                self.closed = True
        transport = Transport(port=None, baudrate=115200, timeout=0.05, write_timeout=1)
        module = types.SimpleNamespace(Serial=lambda **kwargs: transport)
        with patch.dict(sys.modules, {"serial": module}), patch.object(guided.Campaign, "connect", return_value=dict(calibrated=False)) as connection, patch.object(guided.Campaign, "run", return_value=False):
            self.assertEqual(guided.main(["resume", "--campaign", str(self.path), "--port", "COM5", "--operator", "Test operator"]), 0)
        self.assertEqual(transport.port, "COM5")
        self.assertFalse(transport.dtr)
        self.assertFalse(transport.rts)
        self.assertTrue(transport.opened)
        self.assertTrue(transport.closed)
        self.assertTrue(connection.call_args.kwargs["physical"])
        self.assertEqual(connection.call_args.kwargs["operator"], "Test operator")
        self.assertEqual(guided.main(["resume", "--campaign", str(self.path), "--port", "COM5"]), 2)
        self.assertEqual(guided.main(["resume", "--campaign", str(self.path), "--bridge", "synthetic"]), 2)
        out = self.path.parent / "finished"
        with patch.dict(sys.modules, {"serial": module}), patch.object(guided.Campaign, "connect", return_value=dict(calibrated=False)), patch.object(guided.Campaign, "run", return_value=True), patch.object(guided, "write_outputs", return_value=dict(installed=False)) as export:
            self.assertEqual(guided.main(["resume", "--campaign", str(self.path), "--port", "COM5", "--operator", "Test operator", "--out", str(out)]), 0)
        self.assertEqual(export.call_count, 1)
        self.assertEqual(export.call_args.args[1], out)

    def test_cli_offline_references_plan_report(self):
        inventory = self.path.parent / "inventory.json"
        self.assertEqual(guided.main(["references", "--out", str(inventory)]), 0)
        self.assertEqual(guided.main(["references", "--inventory", str(inventory)]), 2)
        inventory.write_text(json.dumps(guided.synthetic_inventory()))
        with patch("builtins.print"):
            self.assertEqual(guided.main(["plan", "--inventory", str(inventory)]), 0)
            self.assertEqual(guided.main(["status", "--campaign", str(self.path)]), 0)
        report = self.path.parent / "pending.md"
        self.assertEqual(guided.main(["report", "--campaign", str(self.path), "--out", str(report)]), 0)
        self.assertIn("Captured 0/99", report.read_text())
        self.assertEqual(guided.main(["report", "--campaign", str(self.path), "--out", str(report)]), 2)


@unittest.skipUnless(BRIDGE, "C fixture required via WTK_CAPTURE_BRIDGE")
class CSerialCampaignTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / "campaign.json"
        self.campaign = guided.Campaign.create(self.path, guided.synthetic_inventory())
        self.serial = capture.FakeSerial(BRIDGE)
        self.addCleanup(self.serial.close)
        self.client = capture.Client(self.serial, timeout=1)
        self.campaign.connect(self.client)
        self.key = pc_osl.Key(1000, 1000, 100)

    def run_one(self, confirm=lambda request: True):
        return self.campaign.run(self.client, confirm, output=lambda message: None, limit=1)

    def raw(self, standard="OPEN"):
        ref = guided.references(self.campaign.document["inventory"])[self.key.rref_ohms]
        return self.client.capture(self.key, standard, ref)

    def change_raw(self, row, change):
        raw = bytearray.fromhex(row["artifact_hex"])
        change(raw)
        struct.pack_into("<I", raw, 184, zlib.crc32(raw[:184]))
        return capture.observation(bytes(raw), row["device"], row["capture_id"],
                                   pc_osl.Key.from_json(row["condition"]), row["standard"], row.get("reference"))

    def test_c99_interrupted_42_resume_43_frame_installer_compatibility(self):
        confirmations = []
        self.campaign.run(self.client, lambda row: confirmations.append(row) or True, output=lambda message: None, limit=42)
        before = [row["sha256"] for row in self.campaign.document["captures"]]
        self.assertEqual(len(before), 42)
        self.assertEqual(self.campaign.progress()["complete_conditions"], 0)
        self.serial.control(4, struct.pack("<H", 0))
        self.serial.pending.clear()
        self.campaign = guided.Campaign.load(self.path)
        self.client = capture.Client(self.serial)
        self.campaign.connect(self.client)
        self.assertEqual(self.campaign.missing()[0], guided.plan(guided.synthetic_inventory())[42])
        self.campaign.run(self.client, lambda row: confirmations.append(row) or True, output=lambda message: None)
        self.assertEqual(len(confirmations), 99)
        self.assertEqual([row["sha256"] for row in self.campaign.document["captures"][:42]], before)
        self.assertEqual(self.campaign.progress()["complete_conditions"], 33)
        out = self.path.parent / "candidate"
        result = guided.write_outputs(self.campaign, out)
        self.assertEqual((result["bytes"], result["conditions"], result["sequence"]), (2760, 33, 1))
        self.assertEqual(install.validate_candidate((out / "candidate.bin").read_bytes()).sequence, 1)
        self.assertFalse(self.client.status()["calibrated"])
        self.assertEqual(install.Installer(self.client).status()["state"], install.IDLE)
        self.assertTrue(all(row["physically_qualified"] is False for row in self.campaign.document["captures"]))
        self.assertIn("SIMULATED", (out / "report.md").read_text())
        self.assertFalse(json.loads((out / "report.json").read_text())["physically_qualified"])
        self.assertEqual(guided.Campaign.load(out / "campaign.json").progress()["captured"], 99)
        self.assertEqual(json.loads((out / "report.json").read_text())["campaign_sha256"],
                         hashlib.sha256((out / "campaign.json").read_bytes()).hexdigest())
        with self.assertRaises(ValueError):
            guided.write_outputs(self.campaign, out)
        with self.assertRaisesRegex(ValueError, "successor"):
            self.campaign.build(2)

    def test_simulate_command_blank_device_does_not_install(self):
        out = self.path.parent / "simulation"
        result = guided.simulate(BRIDGE, out)
        self.assertEqual((result["captures"], result["resumed_after"], result["bytes"]), (99, 42, 2760))
        self.assertFalse(result["installed"])
        self.assertTrue(result["active_calibration_unchanged"])

    def test_no_confirmation_no_start(self):
        next_id = self.client.next_id
        self.assertFalse(self.run_one(lambda request: False))
        self.assertEqual(self.client.next_id, next_id)
        self.assertFalse(self.campaign.document["captures"])

    def test_each_capture_requires_confirmation(self):
        confirmations = []
        self.campaign.run(self.client, lambda request: confirmations.append(request) or len(confirmations) < 2,
                          output=lambda message: None)
        self.assertEqual(len(confirmations), 2)
        self.assertEqual(len(self.campaign.document["captures"]), 1)
        self.assertEqual(guided.Campaign.load(self.path).progress()["captured"], 1)

    def test_duplicate_and_explicit_recapture_audit(self):
        row = self.raw()
        self.campaign.accept(row)
        with self.assertRaisesRegex(ValueError, "duplicate"):
            self.campaign.accept(row)
        confirmations = []
        self.campaign.run(self.client, lambda request: confirmations.append(request) or True,
                          output=lambda message: None, recapture=(self.key, "OPEN"))
        restored = guided.Campaign.load(self.path)
        self.assertEqual(restored.progress()["captured"], 1)
        self.assertEqual(restored.document["audit"][0]["replaced"], row)
        self.assertNotEqual(restored.document["captures"][0]["capture_id"], row["capture_id"])
        self.assertEqual(len(confirmations), 1)

    def test_wrong_device_firmware_adc_identity_and_profile(self):
        original = self.client.identify
        for field, value in (("uid", "00" * 12), ("git", "ffffffff"), ("firmware_version", "wrong"),
                             ("adc_provenance", "invented"), ("profile", 0), ("capabilities", 0)):
            def changed(field=field, value=value):
                identity = dict(original())
                identity[field] = value
                self.client.identity = identity
                return identity
            with patch.object(self.client, "identify", side_effect=changed):
                with self.assertRaises(ValueError):
                    self.campaign.connect(self.client)
        self.assertEqual(self.campaign.progress()["captured"], 0)

    def test_synthetic_identity_rejected_in_physical_mode(self):
        with self.assertRaisesRegex(ValueError, "synthetic"):
            self.campaign.connect(self.client, physical=True)
        row = self.raw()
        self.campaign.document["device"] = dict(row["device"], synthetic=False)
        row["device"] = self.campaign.document["device"]
        with self.assertRaisesRegex(ValueError, "synthetic provenance"):
            self.campaign.accept(row)

    def test_wrong_load_reference(self):
        row = self.raw("LOAD")
        row["reference"] = dict(row["reference"], id="WRONG")
        with self.assertRaisesRegex(ValueError, "reference"):
            self.campaign.accept(row)

    def test_artifact_wrong_frequency_and_amplitude(self):
        for offset in (19, 20):
            row = self.raw()
            with self.assertRaisesRegex(ValueError, "condition"):
                self.change_raw(row, lambda raw: raw.__setitem__(offset, (raw[offset] + 1) % (3 if offset == 19 else 2)))

    def test_raw_hash_and_metadata_tampering(self):
        for field in ("sha256", "phasors", "quality", "acquisition"):
            row = self.raw()
            row[field] = "tampered"
            with self.assertRaises(ValueError):
                self.campaign.accept(row)
        self.assertFalse(self.campaign.document["captures"])

    def test_adc_scale_provenance_mismatch(self):
        self.campaign.accept(self.raw())
        second = self.raw("SHORT")
        second = self.change_raw(second, lambda raw: struct.pack_into("<f", raw, 96, 0.001))
        with self.assertRaisesRegex(ValueError, "ADC"):
            self.campaign.accept(second)
        self.assertEqual(self.campaign.progress()["captured"], 1)

    def test_adc_provenance_flag_is_required(self):
        row = self.raw()
        with self.assertRaisesRegex(ValueError, "ADC provenance"):
            self.change_raw(row, lambda raw: struct.pack_into("<H", raw, 44, 0))

    def test_saturated_paths_rejected_but_unusable_hg_with_1x_kept(self):
        row = self.raw()
        flags = row["acquisition"]["flags"]
        clipped = self.change_raw(row, lambda raw: struct.pack_into("<H", raw, 22, (flags & ~28) | 384))
        with self.assertRaisesRegex(ValueError, "unobservable"):
            self.campaign.accept(clipped)
        hg_clipped = self.change_raw(row, lambda raw: struct.pack_into("<H", raw, 22, (flags | 4 | 256) & ~24))
        self.campaign.accept(hg_clipped)
        self.assertEqual(guided.quality(hg_clipped)["usable_paths"], ["1X"])
        self.assertFalse(guided.quality(hg_clipped)["hg_observed"])

    def test_weak_source_rejected(self):
        row = self.raw()
        vmid = (pc_osl.complex_value(row["phasors"]["vmid_adc1"]) + pc_osl.complex_value(row["phasors"]["vmid_adc2"])) / 2
        def weak(raw):
            struct.pack_into("<2f", raw, 48, vmid.real, vmid.imag)
            struct.pack_into("<2f", raw, 64, vmid.real, vmid.imag)
        with self.assertRaisesRegex(ValueError, "source too small"):
            self.change_raw(row, weak)

    def test_physically_unobservable_triplet_not_completed(self):
        opened, short, load = self.raw(), self.raw("SHORT"), self.raw("LOAD")
        self.campaign.accept(opened)
        self.campaign.accept(short)
        def degenerate(raw):
            raw[48:96] = bytes.fromhex(short["artifact_hex"])[48:96]
        load = self.change_raw(load, degenerate)
        with self.assertRaisesRegex(ValueError, "nondegenerate"):
            self.campaign.accept(load)
        self.assertEqual(self.campaign.progress()["complete_conditions"], 0)
        self.assertEqual(self.campaign.progress()["captured"], 2)

    def test_safety_rejection_no_start_and_no_automatic_retry(self):
        for injection in (1, 2):
            self.serial.inject(injection)
            with self.assertRaisesRegex(ValueError, "unsafe"):
                self.run_one()
            self.assertFalse(self.client.status()["result_valid"])
            self.assertEqual(self.campaign.progress()["captured"], 0)
            self.serial.inject(0)
        self.assertEqual(len(self.campaign.document["attempts"]), 2)

    def test_acquisition_rejection_is_preserved_then_explicit_resume(self):
        self.serial.inject(4)
        with self.assertRaisesRegex(ValueError, "ACQUISITION_ERROR"):
            self.run_one()
        self.campaign = guided.Campaign.load(self.path)
        self.assertEqual(self.campaign.progress()["captured"], 0)
        self.assertIn("ACQUISITION_ERROR", self.campaign.document["attempts"][0]["reason"])
        self.serial.inject(0)
        self.campaign.connect(capture.Client(self.serial))
        self.run_one()
        self.assertEqual(self.campaign.progress()["captured"], 1)

    def test_timeout_not_accepted_and_canceled(self):
        self.serial.inject(8)
        self.serial.read_ticks = 1
        self.client.timeout = 0.02
        with self.assertRaises(TimeoutError):
            self.run_one()
        self.assertFalse(self.client.status()["busy"])
        self.assertEqual(self.campaign.progress()["captured"], 0)
        self.assertEqual(len(self.campaign.document["attempts"]), 1)

    def test_keyboard_cancel_drains_existing_service(self):
        self.serial.inject(8)
        def interrupted(key, standard, ref):
            self.client.begin(key, standard)
            raise KeyboardInterrupt()
        with patch.object(self.client, "capture", side_effect=interrupted):
            with self.assertRaises(KeyboardInterrupt):
                self.run_one()
        state = self.client.status()
        self.assertFalse(state["busy"])
        self.assertTrue(state["transfer_safe"])
        self.assertFalse(state["result_valid"])
        self.assertEqual(guided.Campaign.load(self.path).progress()["captured"], 0)

    def test_disconnection_not_accepted_and_no_retry(self):
        with patch.object(self.serial, "read", side_effect=OSError("COM disconnected")):
            with self.assertRaisesRegex(OSError, "disconnected"):
                self.run_one()
        self.assertEqual(self.campaign.progress()["captured"], 0)
        self.assertEqual(len(self.campaign.document["attempts"]), 1)

    def test_lost_start_acknowledgment_cancels_without_replay(self):
        self.serial.inject(8)
        calls = []
        def missing_ack(key, standard, ref):
            calls.append(self.client.begin(key, standard))
            raise TimeoutError("lost START acknowledgment")
        with patch.object(self.client, "capture", side_effect=missing_ack):
            with self.assertRaisesRegex(TimeoutError, "acknowledgment"):
                self.run_one()
        self.assertEqual(len(calls), 1)
        self.assertFalse(self.client.status()["busy"])
        self.assertTrue(self.campaign.document["attempts"][0]["cancellation"]["transfer_safe"])
        self.assertEqual(self.campaign.progress()["captured"], 0)

    def test_reconnect_negotiates_stale_id_without_repeating(self):
        self.run_one()
        self.client.next_id = 0x70000001
        self.client.identify()
        reconnect = capture.Client(self.serial)
        self.campaign = guided.Campaign.load(self.path)
        self.campaign.connect(reconnect)
        self.assertGreater(reconnect.next_id, 0x70000001)
        self.assertEqual(self.campaign.progress()["captured"], 1)

    def test_corrupt_capture_detected_even_with_recomputed_manifest_seal(self):
        self.run_one()
        self.campaign.document["captures"][0]["artifact_hex"] = "00" * 188
        self.campaign.save()
        with self.assertRaisesRegex(ValueError, "CRC"):
            guided.Campaign.load(self.path)

    def test_sequence_changed_after_capture_blocks_resume(self):
        original = install.Installer.status
        with patch.object(install.Installer, "status", side_effect=lambda: dict(original(install.Installer(self.client)), next_sequence=2)):
            with self.assertRaisesRegex(ValueError, "sequence changed"):
                self.campaign.connect(self.client)

    def test_failed_atomic_accept_restores_memory_and_disk(self):
        row = self.raw()
        before = self.path.read_bytes()
        with patch.object(guided.os, "replace", side_effect=OSError("interrupted save")):
            with self.assertRaises(OSError):
                self.campaign.accept(row)
        self.assertFalse(self.campaign.document["captures"])
        self.assertEqual(self.path.read_bytes(), before)

    def test_stale_campaign_cannot_erase_accepted_capture(self):
        stale = guided.Campaign.load(self.path)
        self.run_one()
        with self.assertRaisesRegex(ValueError, "changed since load"):
            stale.save()
        self.assertEqual(guided.Campaign.load(self.path).progress()["captured"], 1)

    def test_command_contract_has_no_install_writes(self):
        commands = []
        original = self.client.command
        def command(kind, body=b""):
            commands.append(kind)
            return original(kind, body)
        with patch.object(self.client, "command", side_effect=command):
            self.campaign.connect(self.client)
            self.run_one()
        self.assertTrue(set(commands) <= {capture.IDENTIFY, capture.STATUS, capture.START, capture.RESULT, capture.CANCEL, install.STATUS})
        self.assertNotIn(install.BEGIN, commands)
        self.assertNotIn(install.COMMIT, commands)


if __name__ == "__main__":
    unittest.main()

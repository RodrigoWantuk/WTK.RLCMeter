"""Desktop policy tests; fixture values are test evidence, never bench qualification."""
import copy
import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile
import threading
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import pc_bringup as b01
import pc_osl
import pc_osl_campaign as guided
import pc_osl_workbench_model as model
from pc_osl_install import validate_candidate, Installer

BRIDGE = os.environ.get("WTK_CAPTURE_BRIDGE")


def test_only_session():
    """Policy input with claimed PASS; it is not an actual physical evidence file."""
    value = b01.template()
    value["firmware"].update(profile="BRINGUP", expected_sha="a" * 40)
    value["board"].update(fabrication_identity="TEST_BOARD", population_notes="TEST_ONLY",
                          dnp_verification="TEST_ONLY", photographs=["test.jpg"])
    for group in ("modules", "instruments"):
        value[group] = {key: "TEST_ONLY" for key in value[group]}
    for key in model.PHYSICAL_CHECKS:
        entry = value["manual"][key]
        limits = b01.MANUAL[key][2]
        entry.update(status="PASS", notes="TEST_ONLY", operator="TEST_ONLY", timestamp_utc="TEST_ONLY",
                     instrument="TEST_ONLY", evidence=["test-only"],
                     value=sum(limits) / 2 if limits else None, unit="V" if limits else None)
    return value


class ModelPolicyTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.backend = model.Workbench()
        self.backend.create(self.root / "campaign.json", guided.synthetic_inventory())

    def evidence_file(self, value):
        path = self.root / "b01.json"
        path.write_text(json.dumps(value), encoding="utf-8")
        return path

    def test_matrix_33_keys_pending_and_no_forbidden_key(self):
        rows = self.backend.snapshot()["matrix"]
        self.assertEqual(len(rows), 33)
        self.assertTrue(all(cell["state"] == "PENDING" for r in rows for cell in r["cells"].values()))
        self.assertFalse(any(r["condition"]["rref_ohms"] == 10 and r["condition"]["amplitude_mvrms"] == 500 for r in rows))

    def test_default_progress_and_seven_remaining_fixture_changes(self):
        snapshot = self.backend.snapshot()
        self.assertEqual(snapshot["progress"]["captured"], 0)
        self.assertEqual(snapshot["remaining_changes"], 7)
        self.assertFalse(snapshot["progress"]["installed"])

    def test_fresh_physical_gate_is_required_even_with_no_device_fault(self):
        self.backend.client = Mock()
        self.backend.mode = "Physical"
        with self.assertRaisesRegex(ValueError, "operator gate"):
            self.backend.acquire(confirmed=True)
        self.backend.client.capture.assert_not_called()

    def test_snapshot_cannot_mutate_campaign(self):
        snapshot = self.backend.snapshot()
        snapshot["document"]["inventory"]["references"][0]["id"] = "CHANGED"
        self.assertEqual(self.backend.campaign.document["inventory"]["references"][0]["id"], "R10")

    def test_blank_evidence_never_passes(self):
        self.assertTrue(model.physical_missing(None))
        value = b01.template(); value["firmware"]["profile"] = "BRINGUP"
        imported = model.import_evidence(self.evidence_file(value))
        missing = model.physical_missing(imported)
        self.assertTrue(any("R02" in m for m in missing))
        self.assertTrue(any("R05" in m for m in missing))
        self.assertTrue(any("3v3_stability" in m for m in missing))

    def test_report_generic_false_is_not_an_approval_or_rejection(self):
        session = test_only_session()
        value = dict(session=session, physically_validated=False, SIMULATED=False, checks=[])
        imported = model.import_evidence(self.evidence_file(value))
        self.assertEqual(model.physical_missing(imported), [])
        session["manual"]["R02"]["status"] = "NOT_TESTED"
        self.assertTrue(model.physical_missing(model.import_evidence(self.evidence_file(value))))

    def test_report_synthetic_cannot_satisfy_physical_gate(self):
        for flag in ("SIMULATED", "OFFLINE_REPLAY"):
            value = dict(session=test_only_session(), checks=[], **{flag: True})
            self.assertTrue(any("SIMULATED" in m for m in model.physical_missing(model.import_evidence(self.evidence_file(value)))))

    def test_manual_synthetic_cannot_satisfy_physical_gate(self):
        value = test_only_session(); value["manual"]["R02"]["simulated"] = True
        self.assertTrue(model.import_evidence(self.evidence_file(value))["SIMULATED"])

    def test_stop_in_optional_check_blocks_capture(self):
        value = test_only_session(); value["manual"]["buzzer"]["status"] = "STOP"
        self.assertTrue(any("STOP" in m for m in model.physical_missing(model.import_evidence(self.evidence_file(value)))))

    def test_unknown_or_stop_diagnostic_report_blocks(self):
        for status in ("UNKNOWN", "STOP"):
            value = dict(session=test_only_session(), checks=[dict(check="residual", status=status)])
            self.assertIn("residual: " + status, model.physical_missing(model.import_evidence(self.evidence_file(value))))

    def test_wrong_b01_profile_refused(self):
        value = test_only_session(); value["firmware"]["profile"] = "BRINGUP_CAL"
        with self.assertRaisesRegex(ValueError, "conventional"):
            model.import_evidence(self.evidence_file(value))

    def test_evidence_retained_with_hash_using_existing_campaign_seal(self):
        path = self.evidence_file(test_only_session())
        self.backend.evidence(path)
        loaded = guided.Campaign.load(self.backend.campaign.path)
        imported = loaded.document["desktop_evidence"]["imports"][0]
        self.assertEqual(imported["sha256"], hashlib.sha256(path.read_bytes()).hexdigest())
        path.write_text("changed later", encoding="utf-8")
        self.assertEqual(loaded.document["desktop_evidence"]["imports"][0]["session"]["board"]["fabrication_identity"], "TEST_BOARD")

    def test_inventory_validated_without_new_schema(self):
        path = self.root / "inventory.json"
        model.inventory_save(path, guided.synthetic_inventory())
        self.assertEqual(guided.read_json(path), guided.synthetic_inventory())
        with self.assertRaises((ValueError, TypeError)):
            model.inventory_save(path, guided.reference_template())
        self.assertEqual(guided.read_json(path), guided.synthetic_inventory())

    def test_no_confirmation_no_start(self):
        with self.assertRaisesRegex(ValueError, "confirmation"):
            self.backend.acquire(confirmed=False)
        self.assertEqual(self.backend.campaign.document["confirmations"], [])

    def test_disconnected_capture_refused(self):
        with self.assertRaisesRegex(ValueError, "disconnected"):
            self.backend.acquire(confirmed=True)

    def test_incomplete_candidate_refused(self):
        with self.assertRaisesRegex(ValueError, "99"):
            self.backend.build(self.root / "output")
        self.assertFalse((self.root / "output").exists())

    def test_corrupt_campaign_keeps_previous_context(self):
        path = self.root / "broken.json"; path.write_text('{"format":"bad"}')
        with self.assertRaises(ValueError):
            self.backend.open(path)
        self.assertEqual(self.backend.campaign.path.name, "campaign.json")

    def test_clear_preserves_disk_and_allows_new_inventory(self):
        self.backend.clear()
        self.assertIsNone(self.backend.snapshot()["document"])
        self.assertEqual(len(guided.Campaign.load(self.root / "campaign.json").missing()), 99)

    def test_operator_decision_records_swd_transition_not_equal_git(self):
        self.backend.evidence(self.evidence_file(test_only_session()))
        self.backend.client = Mock(identity=dict(git="bbbbbbbb", uid="TEST_ONLY"))
        self.backend.mode = "Physical"; self.backend.operator = "TEST_ONLY"
        self.backend.state = dict(calibration_sequence=0)
        with patch.object(self.backend, "status"):
            self.backend.approve("TEST_BOARD", "b" * 40, "TEST_ONLY reviewed interlocks/clamp", {k: True for k in model.ACKNOWLEDGEMENTS})
        record = guided.Campaign.load(self.root / "campaign.json").document["desktop_evidence"]["decisions"][-1]
        self.assertEqual(record["from_firmware"]["expected_sha"], "a" * 40)
        self.assertEqual(record["deployed_sha"], "b" * 40)
        self.assertFalse(record["physically_qualified"])
        self.assertTrue(self.backend.gate_approved)

    def test_operator_wrong_board_sha_and_unchecked_gate_refused(self):
        self.backend.evidence(self.evidence_file(test_only_session()))
        self.backend.client = Mock(identity=dict(git="bbbbbbbb", uid="TEST_ONLY"))
        self.backend.mode = "Physical"; self.backend.operator = "TEST_ONLY"
        for board, sha, checks in (("WRONG", "b" * 40, {k: True for k in model.ACKNOWLEDGEMENTS}),
                                   ("TEST_BOARD", "c" * 40, {k: True for k in model.ACKNOWLEDGEMENTS}),
                                   ("TEST_BOARD", "b" * 40, {k: False for k in model.ACKNOWLEDGEMENTS})):
            with self.assertRaises(ValueError):
                self.backend.approve(board, sha, "TEST_ONLY", checks)
        self.assertFalse(self.backend.gate_approved)


@unittest.skipUnless(BRIDGE, "WTK_CAPTURE_BRIDGE required")
class CBackendTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.backend = model.Workbench()
        self.backend.create(self.root / "campaign.json", guided.synthetic_inventory())
        self.backend.connect("Simulation", BRIDGE, "SYNTHETIC_OPERATOR")
        self.addCleanup(self.backend.disconnect)

    def test_real_c_identity_blank_status_and_one_confirmation(self):
        self.assertFalse(self.backend.state["calibrated"])
        self.assertEqual(self.backend.installation["next_sequence"], 1)
        self.backend.acquire(confirmed=True)
        snap = self.backend.snapshot()
        self.assertEqual(snap["progress"]["captured"], 1)
        self.assertEqual(snap["matrix"][0]["cells"]["OPEN"]["state"], "ACCEPTED")
        self.assertEqual(len(snap["document"]["confirmations"]), 1)
        self.assertIn("SIMULATED", snap["progress"]["evidence"])

    def test_emitted_protocol_commands_exclude_every_flash_write(self):
        commands = []
        original = self.backend.transport.write
        def write(data):
            commands.append(data[5])
            return original(data)
        self.backend.transport.write = write
        self.backend.acquire(confirmed=True)
        self.backend.status()
        self.assertTrue({0x50, 0x51, 0x52, 0x53, 0x59}.issubset(set(commands)))
        self.assertTrue(set(commands).issubset({0x50, 0x51, 0x52, 0x53, 0x54, 0x59}))

    def test_rejected_safety_then_explicit_retry(self):
        self.backend.transport.inject(1)
        with self.assertRaises(ValueError):
            self.backend.acquire(confirmed=True)
        self.assertEqual(self.backend.snapshot()["matrix"][0]["cells"]["OPEN"]["state"], "REJECTED")
        self.assertEqual(self.backend.snapshot()["progress"]["captured"], 0)
        self.backend.transport.inject(0)
        self.backend.status()
        self.backend.acquire(confirmed=True)
        self.assertEqual(self.backend.snapshot()["progress"]["captured"], 1)

    def test_recapture_audit_and_duplicate_wrong_next_refused(self):
        self.backend.acquire(confirmed=True)
        pair = pc_osl.KEYS[0], "OPEN"
        old = self.backend.campaign.document["captures"][0]["sha256"]
        with self.assertRaisesRegex(ValueError, "displayed"):
            self.backend.acquire(pair=pair, confirmed=True)
        self.backend.acquire(pair=pair, recapture=True, confirmed=True)
        doc = guided.Campaign.load(self.root / "campaign.json").document
        self.assertEqual(len(doc["captures"]), 1)
        self.assertEqual(doc["audit"][0]["replaced"]["sha256"], old)

    def test_no_physical_synthetic_connection(self):
        self.backend.disconnect()
        physical = model.Workbench(lambda mode, address: model.capture.FakeSerial(address))
        physical.open(self.root / "campaign.json")
        with self.assertRaisesRegex(ValueError, "inventory"):
            physical.connect("Physical", BRIDGE, "TEST_ONLY")
        physical.campaign.document["inventory"]["open_fixture"] = "OPEN_TEST_ONLY"
        physical.campaign.document["inventory"]["short_fixture"] = "SHORT_TEST_ONLY"
        for reference in physical.campaign.document["inventory"]["references"]:
            reference["fixture_id"] = "TEST_ONLY_" + reference["id"]
        physical.campaign.save()
        with self.assertRaisesRegex(ValueError, "synthetic"):
            physical.connect("Physical", BRIDGE, "TEST_ONLY")

    def test_adc_or_identity_binding_changes_rejected(self):
        self.backend.acquire(confirmed=True)
        row = copy.deepcopy(self.backend.campaign.document["captures"][0])
        row["adc"]["provenance"] = "FABRICATED"
        with self.assertRaises(ValueError):
            self.backend.campaign.accept(row, replace=True)
        self.backend.campaign.document["device"]["git"] = "CHANGED"
        with self.assertRaises(ValueError):
            self.backend.acquire(confirmed=True)

    def test_cancellation_between_requests_uses_real_cancel(self):
        original = self.backend.client.begin
        def begin(*args):
            result = original(*args)
            self.backend.cancel_event.set()
            return result
        self.backend.client.begin = begin
        with self.assertRaises(KeyboardInterrupt):
            self.backend.acquire(confirmed=True)
        self.assertEqual(len(self.backend.campaign.document["captures"]), 0)
        self.assertFalse(self.backend.client.status()["busy"])
        self.assertTrue(self.backend.client.status()["transfer_safe"])

    def test_disconnect_timeout_unknown_no_false_acceptance(self):
        with patch.object(self.backend.client, "identify", side_effect=TimeoutError("lost serial")):
            with self.assertRaises(TimeoutError):
                self.backend.acquire(confirmed=True)
        self.assertTrue(self.backend.unknown)
        self.assertEqual(len(self.backend.campaign.document["captures"]), 0)
        with self.assertRaisesRegex(ValueError, "UNKNOWN"):
            self.backend.acquire(confirmed=True)

    def test_resume_42_then_candidate_real_solver_no_installation(self):
        for _ in range(42):
            self.backend.acquire(confirmed=True)
        hashes = [r["sha256"] for r in self.backend.campaign.document["captures"]]
        self.backend.disconnect()
        self.backend.open(self.root / "campaign.json")
        self.backend.connect("Simulation", BRIDGE, "SYNTHETIC_OPERATOR")
        self.assertEqual(self.backend.snapshot()["progress"]["captured"], 42)
        for _ in range(57):
            self.backend.acquire(confirmed=True)
        self.backend.build(self.root / "output")
        frame = (self.root / "output/candidate.bin").read_bytes()
        self.assertEqual(len(frame), 2760)
        self.assertEqual(validate_candidate(frame).sequence, 1)
        self.assertEqual(hashes, [r["sha256"] for r in self.backend.campaign.document["captures"][:42]])
        self.assertTrue(all(row["complete"] for row in self.backend.snapshot()["matrix"]))
        self.assertFalse(Installer(self.backend.client).status()["active_valid"])
        self.assertEqual(Installer(self.backend.client).status()["state"], 0)
        self.assertFalse(json.loads((self.root / "output/report.json").read_text())["physically_qualified"])


if __name__ == "__main__":
    unittest.main()

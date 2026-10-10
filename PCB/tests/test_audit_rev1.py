"""Malformed exports and drift must fail; no physical qualification is inferred."""
import copy
import csv
import json
from pathlib import Path
import sys
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import audit_rev1 as audit


def record(kind, identifier, body, ticket=1):
    header = {"type": kind, "id": identifier, "ticket": ticket}
    return json.dumps(header) + "||" + (json.dumps(body) if body is not None else "") + "|"


def pin(net, x=1, y=2, hole=0):
    return {"net": net, "x": x, "y": y, "hole": hole}


class ParserTests(unittest.TestCase):
    def test_deleted_component_does_not_survive(self):
        text = "\n".join([record("DOCHEAD", "", {"uuid": "p", "docType": "PCB"}),
                           record("COMPONENT", "a", {"x": 1}),
                           record("COMPONENT", "a", None, 2)])
        self.assertEqual(audit.live(audit.parse_epru(text)[0], "COMPONENT"), [])

    def test_update_is_scoped_to_document(self):
        text = "\n".join([record("DOCHEAD", "", {"uuid": "p", "docType": "PCB"}),
                           record("ATTR", "a", {"value": 1}),
                           record("ATTR", "a", {"value": 2}, 2),
                           record("DOCHEAD", "", {"uuid": "f", "docType": "FOOTPRINT"}),
                           record("ATTR", "a", {"value": 3})])
        docs = audit.parse_epru(text)
        self.assertEqual([audit.live(d, "ATTR")[0][2]["value"] for d in docs], [2, 3])

    def test_final_json_without_terminator(self):
        text = record("DOCHEAD", "", {"uuid": "p", "docType": "FONT"})[:-1]
        self.assertEqual(len(audit.parse_epru(text)), 1)

    def test_bad_envelope_and_empty_project(self):
        for text in ("", "unrecognized proprietary data", record("ATTR", "a", {})):
            with self.subTest(text=text), self.assertRaises(audit.AuditError):
                audit.parse_epru(text)

    def test_bom_duplicate_and_quantity(self):
        for refs, count in (("R1,R1", 2), ("R1,R2", 1), ("", 1)):
            content = f"No.\tQuantity\tDesignator\n1\t{count}\t{refs}\n".encode("utf-16")
            with self.subTest(refs=refs), self.assertRaises(audit.AuditError):
                audit.read_bom(content)


class ReconciliationTests(unittest.TestCase):
    def setUp(self):
        self.source = {("R1", "1"): pin("A"), ("R2", "1"): pin("A"),
                       ("R1", "2"): pin(""), ("R2", "2"): pin("")}
        self.fab = {key: {**value, "net": "renamed" if value["net"] else f"NC{i}"}
                    for i, (key, value) in enumerate(self.source.items())}

    def test_alias_and_isolated_pads(self):
        self.assertEqual(audit.reconcile_pins(self.source, self.fab), {"A": "renamed"})

    def test_net_split_fails(self):
        self.fab[("R2", "1")]["net"] = "other"
        with self.assertRaisesRegex(audit.AuditError, "splits"):
            audit.reconcile_pins(self.source, self.fab)

    def test_unconnected_merge_fails(self):
        self.fab[("R2", "2")]["net"] = self.fab[("R1", "2")]["net"]
        with self.assertRaisesRegex(audit.AuditError, "merges"):
            audit.reconcile_pins(self.source, self.fab)

    def test_missing_pad_fails(self):
        del self.fab[("R1", "1")]
        with self.assertRaisesRegex(audit.AuditError, "pad set"):
            audit.reconcile_pins(self.source, self.fab)

    def test_geometry_exception_is_not_global_tolerance(self):
        self.fab[("R1", "1")]["y"] += 5
        with self.assertRaisesRegex(audit.AuditError, "position"):
            audit.reconcile_pins(self.source, self.fab)
        audit.reconcile_pins(self.source, self.fab, {("R1", "1"): (0, 5, 0)})
        self.fab[("R2", "1")]["x"] += 1
        with self.assertRaisesRegex(audit.AuditError, "position"):
            audit.reconcile_pins(self.source, self.fab, {("R1", "1"): (0, 5, 0)})

    def test_missing_copper_and_wrong_format_fail(self):
        text = "%FSLAX45Y45*%\n%MOMM*%\nX2540Y5080D03*"
        self.assertEqual(audit.check_top_copper(text, {("R1", "1"): pin("A")}), 1)
        with self.assertRaisesRegex(audit.AuditError, "missing top copper"):
            audit.check_top_copper(text, {("R1", "1"): pin("A", x=5)})
        with self.assertRaisesRegex(audit.AuditError, "unsupported Gerber"):
            audit.check_top_copper(text.replace("MOMM", "MOIN"), {})

    def test_missing_or_wrong_drill_fails(self):
        text = "METRIC,LZ,0000.00000\nT01C1.0\nT01\nX0.0254Y0.0508"
        data = {("J1", "1"): pin("A", hole=1 / .0254)}
        self.assertEqual(audit.check_drills(text, data), 1)
        with self.assertRaisesRegex(audit.AuditError, "missing component drill"):
            audit.check_drills(text.replace("C1.0", "C0.9"), data)

    def test_fabrication_duplicate_conflict_and_pseudo_refs(self):
        fields = ["PIN_NAME", "NET_NAME", "PIN_X", "PIN_Y", "HOLE_SIZE"]
        export = {"lengthUnit": "mil", "pins": {"fields": fields,
                  "rows": [["R_TFT_LED_1", "A", 1, 2, 0], ["PAD422_1", "A", 1, 2, 0]]}}
        self.assertEqual(set(audit.fabrication_pins(export, {"R_TFT_LED"})), {("R_TFT_LED", "1")})
        changed = copy.deepcopy(export)
        changed["pins"]["rows"].append(["R_TFT_LED_1", "B", 1, 2, 0])
        with self.assertRaisesRegex(audit.AuditError, "conflicting"):
            audit.fabrication_pins(changed, {"R_TFT_LED"})
        export["pins"]["rows"].append(["U_FAKE_1", "A", 1, 2, 0])
        with self.assertRaisesRegex(audit.AuditError, "fabrication-only"):
            audit.fabrication_pins(export, {"R_TFT_LED"})


class RepositoryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with zipfile.ZipFile(audit.ROOT / audit.SOURCE) as archive:
            cls.components, cls.pins = audit.extract_source(audit.parse_epru(
                archive.read("WTK RLC Meter.epru").decode("utf-8-sig")))

    def test_complete_review_reconciliation(self):
        result = audit.audit(check=True)
        self.assertEqual((result["components"], result["pads"], result["connected_source_nets"]), (149, 434, 115))
        self.assertEqual(result["top_copper_pad_flashes_verified"], 434)
        self.assertEqual(result["review"]["findings"]["RED"], 6)

    def test_critical_connector_and_fail_safe_contacts(self):
        expected = {("J_UART", "1"): "GND", ("J_UART", "2"): "DEBUG_TX",
                    ("J_UART", "3"): "DEBUG_RX", ("J_TFT", "1"): "+3V3",
                    ("J_TFT", "2"): "GND", ("J_TFT", "8"): "$5N113",
                    ("R_TFT_LED", "1"): "TFT_BL", ("R_TFT_LED", "2"): "$5N113",
                    ("K1", "13"): "TEST_HI", ("K1", "11"): "SAFE_HI",
                    ("K1", "9"): "RET", ("K1", "4"): "TEST_LO",
                    ("K1", "6"): "SAFE_LO", ("K1", "8"): "VMID"}
        for key, net in expected.items():
            with self.subTest(pad=key):
                self.assertEqual(self.pins[key]["net"], net)
        self.assertEqual(len(self.components["J_TFT"]["pads"]), 9)

    def test_firmware_pin_change_is_detected(self):
        changed = copy.deepcopy(self.pins)
        changed[("USTM32", "16")]["net"] = "WRONG_RANGE_ENABLE"
        with self.assertRaisesRegex(audit.AuditError, "GPIO/source drift"):
            audit.check_firmware(audit.ROOT, self.components, changed)

    def test_review_hard_link_and_feedback_population(self):
        path = audit.ROOT / "docs/review/a05/assembly-population.csv"
        with path.open(encoding="utf-8-sig", newline="") as stream:
            decisions = {row["reference"]: row["decision"] for row in csv.DictReader(stream)}
        self.assertEqual(decisions["K2"], "DNP")
        self.assertTrue(all(decisions[r] == "POPULATE" for r in ("R0_BANK", "R_BYP_VM", "R_BYP_VEXC")))
        self.assertTrue(all(decisions[r] == "HOLD FOR REVIEW" for r in ("R_TFT_LED", "BUZZER1", "USTM32")))


if __name__ == "__main__":
    unittest.main()

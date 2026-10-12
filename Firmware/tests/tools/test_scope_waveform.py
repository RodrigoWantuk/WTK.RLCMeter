"""Independent analytic oracles; synthetic data is never physical qualification."""
from dataclasses import replace
import hashlib
import json
import math
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"tools"))
import scope_waveform as sw
import scope_synthetic as synthetic
import pc_bringup
from pc_osl_workbench_model import import_evidence, physical_missing


class ScopeTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)

    def fixture(self,kind="excitation",frequency=1000,**kwargs):
        path,c,p,a=synthetic.generate(self.root/kind,kind,frequency,**kwargs)
        return path,c,sw.import_csv(path,c),p,a

    def rows(self,text,config=None):
        path=self.root/"data.csv"; path.write_text(text,encoding="utf-8")
        c=config or sw.ImportConfig(0,"s",(sw.Channel(1,"A","V",1,0,10,"tip","DC",100000),),header=False,metadata={"synthetic":True})
        return sw.import_csv(path,c)

    def test_cosine_oracle_10_1k_10k(self):
        for f in (10,1000,10000):
            path,c= None,None
            p,c,preset,params=synthetic.generate(self.root/str(f),"excitation",f)
            wave=sw.import_csv(p,c); result=sw.analyze(wave,preset,params)
            fit=result["analysis"]["fundamental"]["fit"]
            self.assertTrue(result["analysis"]["fundamental"]["valid"])
            self.assertAlmostEqual(fit["peak_v"],.5,places=9)
            self.assertAlmostEqual(fit["phase_deg"],math.degrees(.3),places=7)
            self.assertLess(abs(fit["frequency_hz"]/f-1),1e-10)
            self.assertAlmostEqual(result["analysis"]["VEXC_DIFF"]["ac_rms_v"],.5/math.sqrt(2),places=12)
            self.assertAlmostEqual(result["channels"]["VMID"]["mean_v"],1.65,places=12)

    def test_basic_min_max_dc_rms(self):
        wave=self.rows("0,1\n1,2\n2,3\n3,4\n")
        r=sw.analyze(wave)["channels"]["A"]
        self.assertEqual((r["min_v"],r["max_v"],r["vpp_v"],r["mean_v"]),(1,4,3,2.5))
        self.assertAlmostEqual(r["rms_v"],math.sqrt(7.5)); self.assertAlmostEqual(r["ac_rms_v"],math.sqrt(1.25))

    def test_ac_small_relative_to_dc_stable(self):
        r=sw.statistics([1e6+x*1e-3 for x in (1,2,3,4)])
        self.assertAlmostEqual(r["ac_rms_v"],math.sqrt(1.25)*1e-3,places=9)

    def test_noisy_ground_truth(self):
        _,_,w,p,a=self.fixture("noise"); r=sw.analyze(w,p,a)
        fit=r["analysis"]["fundamental"]["fit"]
        self.assertIsNotNone(fit)
        self.assertLess(abs(fit["peak_v"]-.5),.0005)
        self.assertLess(abs(fit["phase_deg"]-math.degrees(.3)),.05)
        self.assertLess(abs(fit["frequency_hz"]-1000),.5)
        self.assertGreater(fit["residual_rms_v"],.001)

    def test_carrier_numeric_projection(self):
        _,_,w,p,a=self.fixture("carrier"); r=sw.analyze(w,p,a)
        carrier=r["analysis"]["carrier_450khz"]
        self.assertTrue(carrier["valid"])
        # Coherent 20k points contain 2 low-frequency cycles and 900 carrier cycles.
        self.assertAlmostEqual(carrier["fit"]["peak_v"],.01,places=9)
        self.assertFalse(r["analysis"]["fundamental"]["valid"])  # low fundamental lacks four cycles

    def test_low_rate_cannot_validate_carrier(self):
        _,_,w,p,a=self.fixture(); r=sw.analyze(w,p,a)
        self.assertFalse(r["analysis"]["carrier_450khz"]["valid"])
        self.assertIn("INSUFFICIENT",r["analysis"]["carrier_450khz"]["reason"])

    def test_unknown_bandwidth_cannot_validate_fundamental_or_carrier(self):
        path,c,_,p,a=self.fixture(); c=replace(c,channels=tuple(replace(ch,bandwidth_hz=None) for ch in c.channels))
        r=sw.analyze(sw.import_csv(path,c),p,a)
        self.assertFalse(r["analysis"]["fundamental"]["valid"]); self.assertFalse(r["analysis"]["carrier_450khz"]["valid"])

    def test_aliased_expected_signal_rejected(self):
        _,_,w,p,a=self.fixture(rate=15000)
        self.assertFalse(sw.analyze(w,p,a)["analysis"]["fundamental"]["valid"])

    def test_insufficient_cycles(self):
        _,_,w,p,a=self.fixture(samples=200)
        self.assertFalse(sw.analyze(w,p,a)["analysis"]["fundamental"]["valid"])

    def test_nonuniform_statistics_not_frequency(self):
        w=self.rows("0,1\n1,2\n2.5,3\n3,4\n")
        r=sw.analyze(w)
        self.assertEqual(r["channels"]["A"]["mean_v"],2.5)
        self.assertFalse(r["channels"]["A"]["fundamental"]["valid"])
        self.assertIn("NONUNIFORM_TIME",r["warnings"][0])

    def test_staggered_rails(self):
        _,_,w,p,a=self.fixture("powerup"); r=sw.analyze(w,p,a)["analysis"]
        self.assertAlmostEqual(r["relative_start_s"],.002,places=12)
        self.assertAlmostEqual(r["rail_5v"]["duration_10_90_s"],.0024,places=12)
        self.assertAlmostEqual(r["rail_3v3"]["end_v"],3.3,places=12)

    def test_simultaneous_rails(self):
        _,_,w,p,a=self.fixture("simultaneous")
        self.assertAlmostEqual(sw.analyze(w,p,a)["analysis"]["relative_start_s"],0,places=12)

    def test_powerdown_rails(self):
        _,_,w,p,a=self.fixture("powerdown"); r=sw.analyze(w,p,a)
        self.assertEqual(r["analysis"]["rail_5v"]["direction"],"fall")
        self.assertAlmostEqual(r["analysis"]["relative_start_s"],.002,places=12)

    def test_overshoot_detected_not_pass(self):
        _,_,w,p,a=self.fixture("overshoot"); r=sw.analyze(w,p,a)
        self.assertAlmostEqual(r["analysis"]["rail_5v"]["overshoot_v"],.5,places=12)
        self.assertTrue(any("exceeds" in x for x in r["warnings"]))
        self.assertEqual(r["electrical_status"],"NOT_TESTED")

    def test_backfeed_requires_regulator_evidence(self):
        _,_,w,p,a=self.fixture("powerup")
        with self.assertRaises(ValueError): sw.analyze(w,p,{"regulator_inactive_interval_s":[0,.005]})
        r=sw.analyze(w,p,{"regulator_inactive_interval_s":[0,.005],"supply_state_evidence":"SYNTHETIC state recorded"})
        self.assertGreater(r["analysis"]["regulator_inactive_observation"]["max_v"],0)
        self.assertEqual(r["electrical_status"],"NOT_TESTED")

    def test_clamp_series_current_oracle(self):
        _,_,w,p,a=self.fixture("clamp"); r=sw.analyze(w,p,a)["analysis"]
        self.assertAlmostEqual(r["resistor_current"]["peak_positive_a"],.00115,places=12)
        self.assertAlmostEqual(r["resistor_current"]["peak_negative_a"],-.00115,places=12)
        oracle=math.sqrt(math.fsum(((x-y)/1000)**2 for x,y in zip(*w.channels))/len(w.time))
        self.assertAlmostEqual(r["resistor_current"]["rms_a"],oracle,places=12)
        self.assertIsNone(r["diode_current_a"]); self.assertIsNone(r["actual_3v3_injection_a"])
        self.assertGreater(r["adc_nominal_limits"]["samples_above"],0); self.assertGreater(r["adc_nominal_limits"]["samples_below"],0)

    def test_current_uncertainty_explicit_only(self):
        path,c,_,p,a=self.fixture("clamp")
        c=replace(c,channels=tuple(replace(ch,voltage_error_bound_v=.02,accuracy_evidence="SYNTHETIC tip uncertainty") for ch in c.channels))
        r=sw.analyze(sw.import_csv(path,c),p,a)["analysis"]
        self.assertAlmostEqual(r["current_bound_a"],(.04+1.15*.01)/(1000*.99),places=12)

    def test_missing_resistor_mapping_rejected(self):
        _,_,w,p,a=self.fixture("clamp")
        a.pop("mapping_evidence")
        with self.assertRaises(ValueError): sw.analyze(w,p,a)

    def test_rail_without_provenance_rejected(self):
        _,_,w,p,a=self.fixture("clamp"); a.pop("rail_evidence")
        with self.assertRaises(ValueError): sw.analyze(w,p,a)

    def test_ret_gain_phase_oracle(self):
        _,_,w,p,a=self.fixture("ret"); r=sw.analyze(w,p,a)["analysis"]
        self.assertAlmostEqual(r["gain_ratio"],1+68000/4700,places=7)
        self.assertAlmostEqual(r["relative_phase_deg"],math.degrees(.2),places=6)

    def test_saturated_hg_rejects_comparison(self):
        _,_,w,p,a=self.fixture("saturated"); r=sw.analyze(w,p,a)
        self.assertEqual(r["channels"]["RET_1X"]["clipping_status"],"UNKNOWN: no declared display/input limits")
        self.assertGreater(r["channels"]["RET_HG"]["display_limit_samples"],0)
        self.assertIsNone(r["analysis"]["gain_ratio"])

    def test_harmonic_projection_closed_form(self):
        n=2000; fs=100000; f=1000
        text="".join(f"{i/fs:.12g},{1+.5*math.cos(math.tau*f*i/fs)+.05*math.cos(2*math.tau*f*i/fs):.15g}\n" for i in range(n))
        w=self.rows(text); r=sw.channel_metrics(w,0,f)["fundamental"]
        self.assertTrue(r["valid"])
        self.assertAlmostEqual(r["harmonics"]["thd_2_3_estimate"],.1,places=9)

    def test_independent_files_no_phase(self):
        _,_,w,_,_=self.fixture(); _,_,second,_,_=self.fixture("noise")
        r=sw.independent_comparison(w,second,frequency=1000)
        self.assertIsNone(r["relative_phase_deg"]); self.assertFalse(r["synchronized"])

    def test_unsynchronized_pair_rejected(self):
        path,c,_,p,a=self.fixture()
        with self.assertRaisesRegex(ValueError,"simultaneous"): sw.analyze(sw.import_csv(path,replace(c,simultaneous=False)),p,a)

    def test_ac_coupled_rail_refused(self):
        path,c,_,p,a=self.fixture("powerup")
        c=replace(c,channels=tuple(replace(ch,coupling="AC") for ch in c.channels))
        with self.assertRaisesRegex(ValueError,"DC coupling"): sw.analyze(sw.import_csv(path,c),p,a)

    def test_probe_attenuation_not_double_applied(self):
        w=self.rows("0,1\n1,2\n2,3\n3,4\n")
        c=w.config
        connector=replace(c,channels=(replace(c.channels[0],voltage_reference="connector"),))
        scaled=self.rows("0,1\n1,2\n2,3\n3,4\n",connector)
        self.assertEqual(w.channels[0][0],1); self.assertEqual(scaled.channels[0][0],10)

    def test_explicit_ms_mv_offset(self):
        c=sw.ImportConfig(0,"ms",(sw.Channel(1,"A","mV",2,.1,10,"tip","DC",100000),),header=False,time_offset_s=-.002,metadata={"synthetic":True})
        w=self.rows("0,100\n1,200\n2,300\n3,400\n",c)
        self.assertAlmostEqual(w.interval_s,.001); self.assertAlmostEqual(w.time[0],-.002); self.assertAlmostEqual(w.channels[0][0],.3)

    def test_scientific_notation(self):
        w=self.rows("0,1e-3\n1e-3,+2E-3\n2e-3,3.0e-3\n3e-3,.004\n")
        self.assertEqual(w.channels[0][-1],.004)

    def test_semicolon_decimal_comma(self):
        c=sw.ImportConfig(0,"s",(sw.Channel(1,"A","V",1,0,10,"tip","DC",100000),),delimiter=";",decimal=",",header=False,metadata={"synthetic":True})
        w=self.rows("0;1,5\n0,1;2,5\n0,2;3,5\n0,3;4,5\n",c)
        self.assertEqual(w.channels[0][0],1.5)

    def test_source_hash_unmodified(self):
        path,c,w,_,_=self.fixture()
        self.assertEqual(w.source["sha256"],hashlib.sha256(path.read_bytes()).hexdigest())

    def test_header_unit_mismatch_rejected(self):
        c=sw.ImportConfig(0,"s",(sw.Channel(1,"A","V",1,0,10,"tip","DC",100000),),metadata={"synthetic":True})
        with self.assertRaisesRegex(ValueError,"ambiguity"): self.rows("time(s),A(mV)\n0,1\n1,2\n2,3\n3,4\n",c)

    def test_adc_code_header_not_volts(self):
        c=sw.ImportConfig(0,"s",(sw.Channel(1,"A","V",1,0,10,"tip","DC",100000),),metadata={"synthetic":True})
        with self.assertRaises(ValueError): self.rows("time(s),ADC codes\n0,1\n1,2\n2,3\n3,4\n",c)

    def test_native_hantek_profile_explicitly_unsupported(self):
        path,c,_,_,_=self.fixture()
        with self.assertRaisesRegex(ValueError,"UNSUPPORTED Hantek"): sw.import_csv(path,replace(c,profile="hantek_dso2c10"))
        import subprocess
        config=path.with_name("config.json")
        value=json.loads(config.read_text()); value["profile"]="hantek_dso2c10"; config.write_text(json.dumps(value))
        run=subprocess.run([sys.executable,str(Path(sw.__file__)),str(path),"--config",str(config),"--out",str(self.root/"out")],capture_output=True,text=True)
        self.assertEqual(run.returncode,2); self.assertIn("UNSUPPORTED Hantek",run.stderr); self.assertNotIn("Traceback",run.stderr)
        config.write_text("x"*16385)
        with self.assertRaises(ValueError): sw.settings_json(config)

    def test_unknown_scale_probe_units_rejected(self):
        path,c,_,_,_=self.fixture()
        for change in (dict(scale=None),dict(scale=-1),dict(unit="codes"),dict(probe_attenuation=None),dict(voltage_reference="guess")):
            with self.subTest(change=change),self.assertRaises(ValueError): sw.import_csv(path,replace(c,channels=(replace(c.channels[0],**change),)))

    def test_timestamp_duplicates_backward_missing(self):
        for rows in ("0,1\n1,2\n1,3\n2,4\n","0,1\n1,2\n.5,3\n2,4\n","0,1\n,2\n2,3\n3,4\n"):
            with self.subTest(rows=rows),self.assertRaises(ValueError): self.rows(rows)

    def test_nonfinite_corrupt_truncated(self):
        for rows in ("0,1\n1,nan\n2,3\n3,4\n","0,1\n1,inf\n2,3\n3,4\n","0,1\n1,2\n2,3\n3\n","0,1\n1,2\n2,3\n3,1e999\n"):
            with self.subTest(rows=rows),self.assertRaises(ValueError): self.rows(rows)

    def test_blank_line_not_silently_skipped(self):
        with self.assertRaises(ValueError): self.rows("0,1\n\n1,2\n2,3\n3,4\n")

    def test_declared_interval_mismatch(self):
        path,c,_,_,_=self.fixture()
        with self.assertRaises(ValueError): sw.import_csv(path,replace(c,declared_interval_s=.1))

    def test_oversized_sample_line_and_file(self):
        path,c,_,_,_=self.fixture()
        with self.assertRaisesRegex(ValueError,"sample bound"): sw.import_csv(path,replace(c,max_samples=4))
        with self.assertRaises(ValueError): self.rows("0,"+"1"*5000+"\n")
        from unittest.mock import patch
        with patch.object(sw,"MAX_BYTES",10),self.assertRaises(ValueError): sw.import_csv(path,c)

    def test_insufficient_samples(self):
        with self.assertRaises(ValueError): self.rows("0,1\n1,2\n")

    def test_import_cancellation(self):
        path,c,_,_,_=self.fixture()
        with self.assertRaisesRegex(ValueError,"canceled"): sw.import_csv(path,c,lambda:True)

    def test_extreme_numeric_overflow_rejected(self):
        with self.assertRaises(ValueError): sw.analyze(self.rows("0,1e308\n1,-1e308\n2,1e308\n3,-1e308\n"))
        with self.assertRaises(ValueError): self.rows("-1e308,1\n-1e307,1\n1e307,1\n1e308,1\n")

    def test_envelope_bounded_retains_narrow_peak(self):
        _,_,w,_,_=self.fixture(samples=10000)
        w.channels[0][123]=99
        points=sw.envelope(w,w.channels[0],buckets=100)
        self.assertLessEqual(len(points),202); self.assertEqual(max(p[1] for p in points),99)
        zoom=sw.envelope(w,w.channels[0],w.time[120],w.time[125])
        self.assertEqual(max(p[1] for p in zoom),99)

    def test_b01_sidecar_no_pass_synthetic_gate(self):
        _,_,w,p,a=self.fixture(); session=self.root/"session.json"
        value=pc_bringup.template(); value["firmware"]["profile"]="BRINGUP"
        session.write_text(json.dumps(value),encoding="utf-8")
        before=session.read_bytes(); r=sw.export_bundle(w,sw.analyze(w,p,a),self.root/"out",session,"powerup_waveforms")
        self.assertEqual(session.read_bytes(),before)
        self.assertEqual(r["b01_sidecar"]["status"],"NOT_TESTED"); self.assertFalse(r["physically_qualified"])
        self.assertTrue(physical_missing(import_evidence(session)))
        self.assertTrue(sw.verify_bundle(self.root/"out")["complete"])

    def test_synthetic_never_reviewed_physical(self):
        _,_,w,p,a=self.fixture()
        with self.assertRaisesRegex(ValueError,"synthetic"): sw.export_bundle(w,sw.analyze(w,p,a),self.root/"out",review=dict(operator="x",date="now",rationale="test",evidence="x"))

    def test_manual_review_no_physical_qualification(self):
        path,c,_,p,a=self.fixture(); c=replace(c,metadata=dict(synthetic=False,instrument="DECLARED",operator="owner",capture_date="recorded"))
        w=sw.import_csv(path,c)
        r=sw.export_bundle(w,sw.analyze(w,p,a),self.root/"out",review=dict(operator="owner",date="date",rationale="limited review",evidence="bench log"))
        self.assertEqual(r["evidence_state"],"MANUALLY_REVIEWED"); self.assertFalse(r["physically_qualified"])
        self.assertEqual(r["electrical_status"],"NOT_TESTED")

    def test_unknown_real_provenance_insufficient(self):
        path,c,_,_,_=self.fixture(); c=replace(c,metadata={"synthetic":False})
        self.assertEqual(sw.analyze(sw.import_csv(path,c))["evidence_state"],"INSUFFICIENT_EVIDENCE")

    def test_export_refuses_changed_source(self):
        path,_,w,p,a=self.fixture(); path.write_text("changed",encoding="utf-8")
        with self.assertRaisesRegex(ValueError,"changed"): sw.export_bundle(w,sw.analyze(w,p,a),self.root/"out")

    def test_bundle_corruption_detected(self):
        _,_,w,p,a=self.fixture(); sw.export_bundle(w,sw.analyze(w,p,a),self.root/"out")
        (self.root/"out/source.csv").write_text("changed",encoding="utf-8")
        with self.assertRaisesRegex(ValueError,"checksum"): sw.verify_bundle(self.root/"out")

    def test_bundle_repeatable_bytes_no_overwrite(self):
        _,_,w,p,a=self.fixture(); report=sw.analyze(w,p,a)
        for name in ("one","two"): sw.export_bundle(w,report,self.root/name)
        for filename in ("report.json","report.md","manifest.json","waveform.svg","source.csv"):
            self.assertEqual((self.root/"one"/filename).read_bytes(),(self.root/"two"/filename).read_bytes())
        with self.assertRaises(ValueError): sw.export_bundle(w,report,self.root/"one")

    def test_incomplete_bundle_refused(self):
        directory=self.root/"incomplete"; directory.mkdir(); (directory/"source.csv").write_text("raw")
        with self.assertRaisesRegex(ValueError,"incomplete"): sw.verify_bundle(directory)

    def test_forged_pass_or_qualification_export_rejected(self):
        _,_,w,p,a=self.fixture()
        for key,value in (("physically_qualified",True),("electrical_status","PASS"),("evidence_state","MANUALLY_REVIEWED")):
            report=sw.analyze(w,p,a); report[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError): sw.export_bundle(w,report,self.root/key)

    def test_scope_report_cannot_replace_b01_session(self):
        _,_,w,p,a=self.fixture(); sw.export_bundle(w,sw.analyze(w,p,a),self.root/"out")
        with self.assertRaises(ValueError): import_evidence(self.root/"out/report.json")


if __name__=="__main__": unittest.main()

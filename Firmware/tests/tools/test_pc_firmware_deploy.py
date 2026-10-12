"""Host orchestration with real ELF outputs; fake SWD evidence is always SIMULATED."""
import copy
from dataclasses import dataclass
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"tools"))
import pc_firmware_deploy as d
import pc_bringup as b01
import pc_osl
import pc_osl_synthetic

UID="0102030405060708090a0b0c"
ACKS=dict(safe_outputs=True,debug_halt_watchdog=True,no_app_bootloader=True,internal_flash=True)
IMAGE_ROOT=os.environ.get("WTK_DEPLOY_IMAGES")
OPENOCD=os.environ.get("WTK_TEST_OPENOCD")
BRIDGE=os.environ.get("WTK_CAPTURE_BRIDGE")


def session(path,simulated=True):
    value=b01.template(); value["firmware"]["profile"]="BRINGUP"
    value["board"].update(fabrication_identity="TEST_BOARD",photographs=["test-photo"],population_notes="SYNTHETIC" if simulated else "declared inspected",dnp_verification="recorded")
    for key,entry in value["manual"].items():
        entry.update(status="PASS",simulated=simulated,operator="TEST ONLY" if simulated else "owner",instrument="DECLARED",timestamp_utc="2026-10-11T00:00:00Z",evidence=["test-reference"],notes="explicit test fixture; not physical approval")
        if b01.MANUAL[key][2]: entry.update(value=sum(b01.MANUAL[key][2])/2,unit="V")
        if key=="bat54s_conduction": entry["authorization_reference"]="TEST scoped reference"
    for category in ("modules","instruments"):
        value[category]={k:"TEST declared "+k for k in value[category]}
    b01.validate_session(value); path.write_text(json.dumps(value),encoding="utf-8"); return value


def image_fixture(root,profile):
    # Minimal ARM ELF fixture tests parser/limits. Integration below uses linked firmware.
    spec=d.PROFILES[profile]; length=1100
    binary=bytearray(length); binary[:8]=__import__('struct').pack('<II',d.RAM_BASE+d.RAM,d.BASE+101)
    import struct
    names=b'\0.isr_vector\0.data\0.bss\0.noinit\0._user_heap_stack\0.shstrtab\0'
    sections=[(0,0,0,0,0,0,0,0,0,0)]
    for name,kind,addr,offset,size in ((".isr_vector",1,d.BASE,0x1000,length),(".data",1,d.RAM_BASE,0,0),(".bss",8,d.RAM_BASE,0,100),(".noinit",8,d.RAM_BASE+100,0,0),("._user_heap_stack",8,d.RAM_BASE+100,0,2048),(".shstrtab",3,0,0x2000,len(names))):
        sections.append((names.index(name.encode()),kind,2 if kind!=3 else 0,addr,offset,size,0,0,4,0))
    header=struct.pack('<16sHHIIIIIHHHHHH',b'\x7fELF\x01\x01\x01'+bytes(9),2,40,1,d.BASE+101,52,0x2100,0,52,32,1,40,len(sections),6)
    elf=bytearray(0x2100+len(sections)*40); elf[:52]=header
    elf[52:84]=struct.pack('<8I',1,0x1000,d.BASE,d.BASE,length,length,5,4); elf[0x1000:0x1000+length]=binary
    elf[0x2000:0x2000+len(names)]=names
    for i,row in enumerate(sections): elf[0x2100+i*40:0x2100+(i+1)*40]=struct.pack('<10I',*row)
    root.mkdir(parents=True)
    config=(f'#define WTK_PROJECT_VERSION "0.1.0"\n#define WTK_FIRMWARE_PROFILE "{spec[1]}"\n#define WTK_BUILD_TYPE "{spec[3]}"\n#define WTK_PRODUCT_FACTORY_PROVISIONED {1 if spec[2] else 0}u\n#define WTK_ENABLE_SUPPLEMENTARY_CURVES 0u\n#define WTK_GIT_COMMIT "'+"a"*12+'"\n#define WTK_HARDWARE_COMPATIBILITY "'+d.HARDWARE+'"\n').encode()
    linker=b'FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 64K\nRAM (xrw) : ORIGIN = 0x20000000, LENGTH = 20K\n'
    files={}
    for name,raw in (("firmware.bin",binary),("firmware.elf",elf),("build_config.h",config),("linker.ld",linker)):
        (root/name).write_bytes(raw); files[name]=dict(bytes=len(raw),sha256=d.sha(raw))
    _,metrics=d.elf_info(elf)
    value=dict(schema="WTK_FIRMWARE_1",source=dict(git_sha="a"*40,dirty=False),profile=profile,factory_provisioned=spec[2],supplementary_curves=False,preset=spec[0],build_configuration=spec[3],hardware=d.HARDWARE,
        memory=dict(flash_base=d.BASE,flash_bytes=d.FLASH,ram_base=d.RAM_BASE,ram_bytes=d.RAM,internal_page_bytes=1024),linked=metrics,binary_size=length,files=files,firmware_version="0.1.0",expected_boot=d.boot_identity(profile,"a"*40,"0.1.0"))
    value["seal_sha256"]=d.seal(value); d.save(root/"manifest.json",value)
    d.inspect(root/"manifest.json"); return root/"manifest.json"


class DeployTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup); self.root=Path(self.temp.name)
        self.image=image_fixture(self.root/"image with spaces [exec dangerous] $value","BRINGUP")
        self.backend=d.FakeSWD(self.root/"fake-target"); self.session=self.root/"session.json"; session(self.session)

    def approval(self,image=None,simulated=True,**kwargs):
        value=d.inspect(image or self.image); phrase=f'PROGRAM {value["profile"]} {value["files"]["firmware.bin"]["sha256"][:12]} {UID}'
        defaults=dict(image=value,session_path=self.session,board="TEST_BOARD",operator="TEST ONLY",supply="SYNTHETIC independently limited source",previous="FACTORY_BLANK",uid=UID,accept=phrase,acks=ACKS,simulated=simulated)
        defaults.update(kwargs); return d.authorize(**defaults)

    def run_deploy(self,scenario="normal",action="program",out=None,image=None):
        self.backend.scenario=scenario
        return d.deployment(action,image or self.image,self.backend,out or self.root/"out",self.approval(image),UID)

    def test_manifest_binary_and_elf_consistency(self):
        info=d.inspect(self.image)
        self.assertEqual(info["profile"],"BRINGUP"); self.assertEqual(info["binary_size"],1100)

    def test_wrong_profile_rejected(self):
        with self.assertRaises(ValueError): d.inspect(self.image,"FACTORY_PRODUCT")

    def test_manifest_tamper(self):
        value=d.load(self.image); value["factory_provisioned"]=True; d.save(self.image,value)
        with self.assertRaisesRegex(ValueError,"seal"): d.inspect(self.image)

    def test_stale_binary_rejected(self):
        self.image.with_name("firmware.bin").write_bytes(b"wrong")
        with self.assertRaisesRegex(ValueError,"stale"): d.inspect(self.image)

    def test_resealed_wrong_configuration_rejected(self):
        value=d.load(self.image); value["supplementary_curves"]=True; value["seal_sha256"]=d.seal(value); d.save(self.image,value)
        with self.assertRaises(ValueError): d.inspect(self.image)

    def test_oversized_image(self):
        self.image.with_name("firmware.bin").write_bytes(bytes(65538))
        with self.assertRaises(ValueError): d.inspect(self.image)

    def test_dirty_source_cannot_program_physical(self):
        value=d.inspect(self.image); value["source"]["dirty"]=True
        with self.assertRaisesRegex(ValueError,"dirty"): d.authorize(value,self.session,"TEST_BOARD","owner","controlled", "FACTORY_BLANK",UID,"approval",ACKS,False)

    def test_missing_approval_does_not_touch_target(self):
        with self.assertRaisesRegex(ValueError,"explicit"): d.deployment("program",self.image,self.backend,self.root/"out",uid=UID)
        self.assertFalse(self.backend.path.exists())

    def test_wrong_approval_phrase(self):
        with self.assertRaisesRegex(ValueError,"approval"): self.approval(accept="yes")

    def test_all_program_stage_acknowledgements_required(self):
        for key in ACKS:
            with self.subTest(key=key),self.assertRaises(ValueError): self.approval(acks=dict(ACKS,**{key:False}))

    def test_synthetic_evidence_rejects_real_program(self):
        with self.assertRaisesRegex(ValueError,"synthetic"): self.approval(simulated=False)

    def test_unknown_b01_gate_not_pass(self):
        value=d.load(self.session); value["manual"]["R05"]["status"]="NOT_TESTED"; d.save(self.session,value)
        with self.assertRaisesRegex(ValueError,"R05"): self.approval()

    def test_manual_stop_not_overridden(self):
        value=d.load(self.session); value["manual"]["bat54s_conduction"]["status"]="STOP"; d.save(self.session,value)
        with self.assertRaisesRegex(ValueError,"STOP"): self.approval()

    def test_board_identity_mismatch(self):
        with self.assertRaisesRegex(ValueError,"board"): self.approval(board="OTHER")

    def test_probe_is_not_electrically_passive(self):
        record=d.deployment("probe",None,self.backend,self.root/"probe")
        self.assertIn("resets/halts",record["warning"]); self.assertEqual(record["boot"],"NOT_TESTED")

    def test_program_and_exact_readback(self):
        before=self.backend.external_w25q; result=self.run_deploy()
        self.assertEqual(result["outcome"],"SIMULATED_VERIFIED"); self.assertTrue(result["SIMULATED"])
        self.assertEqual(result["internal_readback"],"PASS"); self.assertEqual(result["programming"],"CONFIRMED")
        self.assertEqual(result["readback"]["expected_sha256"],result["readback"]["actual_sha256"])
        self.assertFalse(result["external_w25q_access"]); self.assertEqual(self.backend.external_w25q,before)
        self.assertEqual(self.backend.operations,[("erase_page",0,0),("erase_page",1,1)])
        self.assertEqual(result["boot"],"NOT_TESTED"); self.assertFalse(result["physically_validated"])

    def test_readback_mismatch_not_deployment_success(self):
        result=self.run_deploy("readback-mismatch")
        self.assertEqual(result["outcome"],"FAILED_OR_UNKNOWN"); self.assertEqual(result["internal_readback"],"FAIL")

    def test_interrupted_program_no_retry(self):
        result=self.run_deploy("interrupted")
        self.assertEqual(result["programming"],"UNKNOWN_MAY_BE_PARTIAL")
        self.assertEqual(self.backend.operations,[("erase_page",0,0)])

    def test_timeout_is_unknown(self):
        result=self.run_deploy("timeout")
        self.assertEqual(result["internal_readback"],"UNKNOWN"); self.assertEqual(result["programming"],"NOT_STARTED")

    def test_zero_exit_without_completion_not_success(self):
        result=self.run_deploy("missing-complete")
        self.assertEqual(result["outcome"],"FAILED_OR_UNKNOWN"); self.assertFalse(self.backend.operations)

    def test_reset_during_program_is_failure(self):
        result=self.run_deploy("unexpected-reset")
        self.assertEqual(result["outcome"],"FAILED_OR_UNKNOWN"); self.assertEqual(result["boot"],"NOT_TESTED")

    def test_bad_target_preflight_no_erase(self):
        for scenario in ("missing-adapter","unpowered","wrong-id","geometry","rdp"):
            with self.subTest(scenario=scenario):
                record=self.run_deploy(scenario,out=self.root/scenario)
                self.assertEqual(record["outcome"],"FAILED_OR_UNKNOWN"); self.assertFalse(self.backend.operations)

    def test_wrong_uid_no_program(self):
        with self.assertRaisesRegex(ValueError,"authorization"): d.deployment("program",self.image,self.backend,self.root/"out",self.approval(),"f"*24)
        self.assertFalse(self.backend.operations)

    def test_report_output_path_reserved_no_duplicate_request(self):
        self.run_deploy(); before=readback=(self.backend.path/"internal.bin").read_bytes()
        with self.assertRaises(FileExistsError): self.run_deploy()
        self.assertEqual(before,(self.backend.path/"internal.bin").read_bytes())

    def test_read_only_verify_no_write(self):
        self.run_deploy(); before=(self.backend.path/"internal.bin").read_bytes(); count=len(self.backend.operations)
        result=d.deployment("verify",self.image,self.backend,self.root/"verify",uid=UID)
        self.assertEqual(result["internal_readback"],"PASS"); self.assertEqual(result["programming"],"NOT_TESTED")
        self.assertEqual(before,(self.backend.path/"internal.bin").read_bytes()); self.assertEqual(count,len(self.backend.operations))

    def test_program_uses_verified_staged_copy_during_artifact_change(self):
        expected=self.image.with_name("firmware.bin").read_bytes(); original=self.backend.execute
        def changed(action,*args,**kwargs):
            result=original(action,*args,**kwargs)
            if action=="probe": self.image.with_name("firmware.bin").write_bytes(b"untrusted changed input")
            return result
        with patch.object(self.backend,"execute",changed): result=self.run_deploy()
        self.assertEqual(result["internal_readback"],"PASS")
        self.assertEqual((self.root/"out/image-readback.bin").read_bytes(),expected)

    def test_fake_dry_run_does_not_access_or_create_target(self):
        record=d.deployment("program",self.image,self.backend,self.root/"out",dry_run=True)
        self.assertFalse(record["target_access"]); self.assertFalse(self.backend.path.exists()); self.assertFalse((self.root/"out").exists())

    def test_openocd_dry_run_no_subprocess_or_target(self):
        backend=d.OpenOCD(self.root/"programmer with spaces.exe",self.root/"scripts","SERIAL123")
        with patch.object(d.subprocess,"Popen",side_effect=AssertionError("no subprocess")),patch.object(d.subprocess,"run",side_effect=AssertionError("no version call")):
            result=d.deployment("program",self.image,backend,self.root/"out",uid=UID,dry_run=True)
        plan=result["commands"]; self.assertIsInstance(plan["argv"],list)
        self.assertEqual(plan["program_range"],[d.BASE,d.BASE+1100])
        self.assertEqual(plan["erase_range"],[d.BASE,d.BASE+2048])
        self.assertNotIn(str(self.image.parent)," ".join(plan["argv"]))
        self.assertEqual(plan["environment"]["WTK_IMAGE"],str(self.image.with_name("firmware.bin").resolve()))

    def test_real_probe_requires_nonprogramming_authorization(self):
        backend=d.OpenOCD(self.root/"programmer.exe",self.root/"scripts","SERIAL123")
        with patch.object(backend,"execute",side_effect=AssertionError("no target access")),self.assertRaisesRegex(ValueError,"authorization"):
            d.deployment("probe",None,backend,self.root/"out")
        self.assertFalse((self.root/"out").exists())

    def test_real_debug_gate_requires_physical_evidence_and_acknowledgement(self):
        with self.assertRaisesRegex(ValueError,"synthetic"): d.debug_authorization(self.session,"TEST_BOARD","owner","stable",ACKS)
        session(self.session,False)
        with self.assertRaisesRegex(ValueError,"acknowledgement"): d.debug_authorization(self.session,"TEST_BOARD","owner","stable",{})
        permission=d.debug_authorization(self.session,"TEST_BOARD","owner","stable",ACKS)
        d.checked(permission); self.assertEqual(permission["permission"],"RESET_HALT_READ_ONLY_NO_PROGRAM")

    def test_destructive_script_paths_not_tcl_code(self):
        source=(d.TOOLS/"swd_deploy.tcl").read_text()
        for forbidden in ("mass_erase","options_write","unlock","flash protect","reset run","resume","load_image"):
            self.assertNotIn(forbidden,source.replace("no unlock",""))
        self.assertIn("flash erase_sector 0 $page $page",source)
        self.assertIn("flash write_bank 0 $::env(WTK_IMAGE) 0",source)

    def test_target_voltage_gates_and_unknown(self):
        text="WTK_B011_TARGET|20036410|411fc231|64|00000000|ffffffff|"+UID+"|134217728|65536|stm32f1x|halted\n"
        for volts in (.0,2.5,5.0):
            with self.subTest(volts=volts),self.assertRaises(ValueError): d.parse_target(text+f"Info : Target voltage: {volts}\n")
        self.assertIsNone(d.parse_target(text)["target_voltage_v"])
        self.assertEqual(d.parse_target(text+"Info : Target voltage: 3.3\n")["target_voltage_v"],3.3)

    def test_missing_or_duplicate_identity_marker(self):
        for text in ("Info : device 0x410 flash 64k", "WTK_B011_TARGET|bad\n"):
            with self.subTest(text=text),self.assertRaises(ValueError): d.parse_target(text)

    def test_report_tampering_detected(self):
        self.run_deploy(); path=self.root/"out/deployment.json"; value=d.load(path); value["boot"]="PASS"; d.save(path,value)
        with self.assertRaises(ValueError): d.write_report(path,self.root/"report.md")

    def test_transcript_import_preserves_unknown_native_grammar(self):
        self.run_deploy(); text=self.root/"actual-user.log"; text.write_text("OpenOCD ... unknown native log\n")
        result=d.import_transcript(self.root/"out/deployment.json",text,self.root/"sidecar.json")
        self.assertIn("UNKNOWN",result["interpretation"]); self.assertEqual(result["internal_readback"],"NOT_TESTED")

    def test_factory_transition_missing_osl_evidence(self):
        image=image_fixture(self.root/"factory","FACTORY_PRODUCT")
        with self.assertRaisesRegex(ValueError,"OSL"): self.approval(image,previous="BRINGUP_CAL")

    def test_osl_frame_report_mismatch_not_accepted(self):
        image=image_fixture(self.root/"factory","FACTORY_PRODUCT")
        frame,_=pc_osl.build_candidate(pc_osl_synthetic.campaign(),1)
        readback=self.root/"readback.bin"; readback.write_bytes(frame)
        report=self.root/"installation.json"; report.write_text(json.dumps(dict(verified=True,device=dict(uid=UID,synthetic=True),status=dict(state=4,error=0,active_sequence=1,active_valid=True),candidate_sha256="wrong",readback_sha256=d.sha(frame))))
        with self.assertRaisesRegex(ValueError,"mismatch"): self.approval(image,previous="BRINGUP_CAL",installation=report,cal_readback=readback)

    def test_missing_resource_evidence_stays_unknown(self):
        approval=self.approval()
        self.assertEqual(approval["resource_pack"]["status"],"UNKNOWN")
        self.assertIsNone(approval["resource_pack"]["reference"])

    def test_generic_resource_json_pass_not_readiness(self):
        resource=self.root/"resources.json"; resource.write_text('{"status":"PASS"}')
        result=self.approval(resource=resource)
        self.assertEqual(result["resource_pack"]["status"],"EXTERNAL_REFERENCE_REQUIRES_OPERATOR_REVIEW")

    @unittest.skipUnless(BRIDGE,"C fixture required for real protocol/OSL installation evidence")
    def test_bringup_cal_to_factory_with_real_c_installation_readback(self):
        from pc_osl_install import provision_simulation
        report=provision_simulation(BRIDGE,self.root/"osl")
        self.assertTrue(report["verified"]); self.assertTrue(report["reboot_verified"])
        image=image_fixture(self.root/"factory","FACTORY_PRODUCT")
        approval=self.approval(image,previous="BRINGUP_CAL",installation=self.root/"osl/installation.json",cal_readback=self.root/"osl/readback.bin")
        self.assertEqual(approval["osl"]["sequence"],1)
        record=d.deployment("program",image,self.backend,self.root/"product-deploy",approval,UID)
        self.assertEqual(record["internal_readback"],"PASS"); self.assertEqual(record["application"],"NOT_TESTED")
        self.assertEqual(record["authorization"]["resource_pack"]["status"],"UNKNOWN")
        self.assertFalse(record["external_w25q_access"])

    @unittest.skipUnless(OPENOCD,"set WTK_TEST_OPENOCD for actual offline Jim/API fixture")
    def test_actual_openocd_tcl_contract_and_path_injection_without_target(self):
        script=Path(__file__).parent/"fixtures/swd_openocd_api_mock.tcl"
        dangerous=str(self.root/'image with spaces; [exec dangerous] $variable {braces}.bin')
        env=dict(os.environ,WTK_LIBRARY=str(d.TOOLS/"swd_deploy.tcl"),WTK_UID=UID,WTK_ACTION="program",WTK_LENGTH="65448",WTK_IMAGE=dangerous,WTK_BEFORE=str(self.root/"before"),WTK_READBACK=str(self.root/"readback"))
        run=subprocess.run([OPENOCD,"-f",str(script)],env=env,capture_output=True,text=True,timeout=10)
        text=run.stdout+run.stderr
        self.assertEqual(run.returncode,0,text); target=d.parse_target(text); self.assertEqual(target["uid"],UID)
        self.assertIn("WTK_TEST_PAGES|64",text); self.assertIn("WTK_TEST_PATH|"+dangerous,text)
        self.assertIn("WTK_B011_COMPLETE",text); self.assertFalse((self.root/"readback").exists())

    def test_transition_requires_reviewed_safe_boot(self):
        image=image_fixture(self.root/"cal","BRINGUP_CAL")
        value=d.load(self.session); value["manual"]["safe_boot"]["status"]="NOT_TESTED"; d.save(self.session,value)
        with self.assertRaisesRegex(ValueError,"safe_boot"): self.approval(image,previous="BRINGUP")

    def test_bringup_to_bringup_cal(self):
        self.run_deploy(); image=image_fixture(self.root/"cal","BRINGUP_CAL")
        approval=self.approval(image,previous="BRINGUP")
        result=d.deployment("program",image,self.backend,self.root/"cal-install",approval,UID)
        self.assertEqual(result["internal_readback"],"PASS"); self.assertEqual(result["authorization"]["previous_profile"],"BRINGUP")
        self.assertEqual(result["authorization"]["next_profile"],"BRINGUP_CAL")


@unittest.skipUnless(IMAGE_ROOT,"set WTK_DEPLOY_IMAGES for linked firmware artifact integration")
class RealArtifactTests(unittest.TestCase):
    def test_all_linked_profiles(self):
        for profile in d.PROFILES:
            with self.subTest(profile=profile):
                result=d.inspect(Path(IMAGE_ROOT)/profile/"manifest.json")
                self.assertEqual(result["profile"],profile); self.assertLessEqual(result["binary_size"],65536)

    def test_real_bringup_64_single_page_erases_and_external_preservation(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp); backend=d.FakeSWD(root/"fake")
            (root/"fake").mkdir(); memory=bytearray(b"\x55"*65536); (root/"fake/internal.bin").write_bytes(memory)
            image=Path(IMAGE_ROOT)/"BRINGUP/manifest.json"
            session_file=root/"session.json"; session(session_file)
            artifact=d.inspect(image); phrase=f'PROGRAM BRINGUP {artifact["files"]["firmware.bin"]["sha256"][:12]} {UID}'
            approval=d.authorize(artifact,session_file,"TEST_BOARD","TEST","SYNTHETIC","FACTORY_BLANK",UID,phrase,ACKS,True)
            result=d.deployment("program",image,backend,root/"out",approval,UID)
            self.assertEqual(result["internal_readback"],"PASS"); self.assertEqual(len(backend.operations),64)
            self.assertTrue(all(first==last for op,first,last in backend.operations))


if __name__=="__main__": unittest.main()

#!/usr/bin/env python3
"""B01.1 internal-Flash deployment. No UART, calibration installation or W25Q access."""
from __future__ import annotations

import argparse
from dataclasses import dataclass
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import time
import uuid

import pc_bringup as b01
import ntc_factory_profile as ntc_factory
from pc_osl_install import validate_candidate

TOOLS=Path(__file__).resolve().parent
REPO=TOOLS.parents[1]
BASE,FLASH,RAM_BASE,RAM=0x08000000,65536,0x20000000,20480
UNIT128_UID="54ff70068370525760380567"
HARDWARE="Rev1-STM32F103C8T6-BluePill"
OPENOCD_VERSION="xPack Open On-Chip Debugger 0.12.0+dev-01850-geb6f2745b-dirty (2025-02-07-10:08)"
OPENOCD_EXE_SHA256="0aa3936470f75969138b0ad72ea1496dd5324c59a771b7a7ddf132ff1f10f09e"
CONFIG_HASHES={"interface/stlink.cfg":"3ba77e7fd28e90d4fd04833e234a3b36fd961d56bf7c8c0fcbdc8c8e49ddc0d1",
               "target/stm32f1x.cfg":"fe44c3ade7395db34a751c57401ebc2a176ed1ca793c47081cc9f06a8618125e"}
PROFILES={"BRINGUP":("stm32-bringup","BRINGUP",False,"MinSizeRel"),
          "BRINGUP_CAL":("stm32-bringup-cal","BRINGUP_CAL",False,"MinSizeRel"),
          "FACTORY_PRODUCT":("stm32-factory-release","PRODUCT",True,"Release"),
          "FACTORY_PRODUCT_UNIT128":("stm32-unit-release","PRODUCT",True,"Release")}
WARNING="SWD init/reset-init resets/halts the core; upstream config freezes watchdogs while halted and uses SRAM. Confirm physical K1 SAFE, range/excitation OFF. Never debugger-power the target. No automatic boot/resume."


def utc(): return datetime.now(timezone.utc).isoformat()


def sha(data): return hashlib.sha256(data).hexdigest()


def read(path,limit=32*1024*1024):
    path=Path(path)
    if not path.is_file() or path.stat().st_size>limit: raise ValueError("missing/oversized input: "+str(path))
    with path.open("rb") as f: data=f.read(limit+1)
    if len(data)>limit: raise ValueError("input grew beyond bound")
    return data


def load(path):
    return json.loads(read(path,2*1024*1024),parse_constant=lambda x: (_ for _ in ()).throw(ValueError("nonfinite JSON")))


def save(path,value):
    path=Path(path); temporary=path.with_name(path.name+"."+uuid.uuid4().hex+".tmp")
    temporary.write_text(json.dumps(value,indent=2,allow_nan=False)+"\n",encoding="utf-8")
    # Windows scanners can briefly lock the destination. Keep atomic replacement
    # and bounded retries; a persistent failure still stops before the next
    # hardware action and leaves the temporary evidence available for inspection.
    for attempt in range(8):
        try:
            os.replace(temporary,path)
            return
        except PermissionError:
            if attempt == 7: raise
            time.sleep(0.05)


def seal(value):
    data=dict(value); data.pop("seal_sha256",None)
    return sha(json.dumps(data,sort_keys=True,separators=(",",":"),allow_nan=False).encode())


def checked(value):
    if value.get("seal_sha256")!=seal(value): raise ValueError("tampered manifest/report seal")
    return value


def command(argv,cwd=None,timeout=300):
    result=subprocess.run([str(v) for v in argv],cwd=cwd,check=True,text=True,capture_output=True,
        timeout=timeout,creationflags=getattr(subprocess,"CREATE_NO_WINDOW",0))
    if len(result.stdout)+len(result.stderr)>4*1024*1024: raise ValueError("command output bound exceeded")
    return result.stdout.strip()


def source_state(repo=REPO):
    head=command(["git","rev-parse","HEAD"],repo)
    status=command(["git","status","--porcelain=v1","--untracked-files=all"],repo)
    # Hash actual build inputs, including existing dirty changes; detect edits during build.
    paths=command(["git","ls-files","--cached","--others","--exclude-standard","-z","Firmware"],repo).split("\0")
    digest=hashlib.sha256()
    for name in sorted(set(p for p in paths if p)):
        path=Path(repo)/name
        digest.update(name.encode()); digest.update(b"\0")
        if path.is_file(): digest.update(read(path,128*1024*1024))
        elif path.is_dir(): digest.update(command(["git","rev-parse","HEAD"],path).encode()); digest.update(command(["git","status","--porcelain"],path).encode())
    return dict(git_sha=head,dirty=bool(status),git_status=status,input_sha256=digest.hexdigest())


def profile_flash_bytes(profile):
    return 131072 if profile == "FACTORY_PRODUCT_UNIT128" else FLASH


def validate_memory_binding(profile,flags):
    capacity=profile_flash_bytes(profile)
    if int(flags.get("WTK_INTERNAL_FLASH_BYTES", "65536u").removesuffix("u")) != capacity:
        raise ValueError("configured Flash differs from selected deployment profile")
    if capacity > FLASH and (flags.get("WTK_EXTENDED_FLASH_UID") != UNIT128_UID or
                            flags.get("WTK_NTC_FACTORY_UID") != UNIT128_UID):
        raise ValueError("extended Flash requires verified UID and preserved unit NTC identity")
    return capacity


def elf_info(data,flash_bytes=FLASH):
    """ELF32 ARM PT_LOAD/sections, independent from filenames or external size tools."""
    if flash_bytes not in (FLASH,131072): raise ValueError("unsupported ELF Flash bounds")
    if len(data)<52 or data[:7]!=b"\x7fELF\x01\x01\x01": raise ValueError("expected ELF32 little-endian")
    header=struct.unpack_from("<16sHHIIIIIHHHHHH",data)
    if header[1:3]!=(2,40): raise ValueError("expected ARM executable")
    phoff,shoff=header[5:7]; phsize,phnum,shsize,shnum,shnames=header[9:14]
    if phsize!=32 or shsize!=40 or not 1<=phnum<=16 or not 1<=shnum<=512 or shnames>=shnum:
        raise ValueError("unsupported ELF headers")
    if phoff+phnum*32>len(data) or shoff+shnum*40>len(data): raise ValueError("truncated ELF")
    segments=[]
    for i in range(phnum):
        kind,offset,vma,lma,size,mem,flags,align=struct.unpack_from("<8I",data,phoff+i*32)
        if kind!=1: continue
        if size>mem or offset+size>len(data): raise ValueError("bad ELF load segment")
        if mem and not (BASE<=vma and vma+mem<=BASE+flash_bytes or RAM_BASE<=vma and vma+mem<=RAM_BASE+RAM): raise ValueError("ELF memory outside Rev1")
        if size and not (BASE<=lma and lma+size<=BASE+flash_bytes): raise ValueError("ELF program data outside internal Flash")
        if size: segments.append((lma,data[offset:offset+size]))
    if not segments or min(s[0] for s in segments)!=BASE: raise ValueError("application must start at 0x08000000, no bootloader offset")
    end=max(addr+len(block) for addr,block in segments); binary=bytearray(end-BASE); occupied=set()
    for addr,block in segments:
        offsets=range(addr-BASE,addr-BASE+len(block))
        if any(i in occupied for i in offsets): raise ValueError("overlapping ELF Flash segments")
        occupied.update(offsets); binary[addr-BASE:addr-BASE+len(block)]=block
    sections=[struct.unpack_from("<10I",data,shoff+i*40) for i in range(shnum)]
    names=sections[shnames]; strings=data[names[4]:names[4]+names[5]]
    sizes={}
    for row in sections:
        name_offset=row[0]
        if name_offset>=len(strings): raise ValueError("invalid ELF section name")
        name=strings[name_offset:].split(b"\0",1)[0].decode("ascii")
        sizes[name]=row[5]
    import firmware_size
    usage=firmware_size.ram_usage(sizes)
    if usage["ram_accounted_bytes"]>RAM: raise ValueError("ELF RAM exceeds silicon")
    sp,reset=struct.unpack_from("<II",binary)
    if not (RAM_BASE<sp<=RAM_BASE+RAM and sp%8==0 and reset&1 and BASE<=reset-1<end): raise ValueError("invalid initial SP/reset vector")
    return bytes(binary),dict(flash_bytes=len(binary),**usage)


def parse_config(raw):
    fields=dict(re.findall(r'^#define\s+(WTK_[A-Z0-9_]+)\s+([^\r\n]+)',raw.decode(),re.M))
    return {k:v.strip().strip('"') for k,v in fields.items()}


def boot_identity(profile,sha_value,version,detailed_bringup=True):
    if profile=="BRINGUP" and not detailed_bringup:
        return dict(interface="LIMITED_UART_UI_REQUIRED",firmware_version=version,hardware=HARDWARE,
            git_prefix=None,limitation="Minimal SAFE_BOOT banner; use SWD byte verification and independent bench acceptance")
    return dict(interface="BRINGUP_UART" if profile=="BRINGUP" else "PLC1_IDENTIFY" if profile=="BRINGUP_CAL" else "LIMITED_UART_UI_REQUIRED",
        firmware_version=version,hardware=HARDWARE,git_prefix=sha_value[:12] if profile=="BRINGUP" else sha_value[:8] if profile=="BRINGUP_CAL" else None,
        limitation="PRODUCT banner is only WTK.RLCMeter SAFE_BOOT; UI/bench acceptance separate" if profile.startswith("FACTORY_PRODUCT") else "reported short Git identity is not binary authentication")


def manifest(build_dir,out,profile,state,preset):
    build_dir=Path(build_dir); out=Path(out); spec=PROFILES[profile]
    config=read(build_dir/"generated/wtk_build_config.h"); flags=parse_config(config)
    capacity=validate_memory_binding(profile,flags)
    expected={"WTK_FIRMWARE_PROFILE":spec[1],"WTK_BUILD_TYPE":spec[3],"WTK_PRODUCT_FACTORY_PROVISIONED":"1u" if spec[2] else "0u",
              "WTK_ENABLE_SUPPLEMENTARY_CURVES":"0u","WTK_GIT_COMMIT":state["git_sha"][:12],"WTK_HARDWARE_COMPATIBILITY":HARDWARE}
    if any(flags.get(k)!=v for k,v in expected.items()): raise ValueError("wrong/stale firmware build configuration")
    binary=read(build_dir/"WTK.RLCMeter.bin",capacity); elf=read(build_dir/"WTK.RLCMeter.elf")
    reconstructed,metrics=elf_info(elf,capacity)
    if binary!=reconstructed: raise ValueError("ELF/BIN bytes differ")
    sizes=load(build_dir/"WTK.RLCMeter.size.json")
    if any(metrics[k]!=sizes[k] for k in ("flash_bytes","ram_accounted_bytes","reserved_stack_bytes")): raise ValueError("linked-size evidence mismatch")
    cache=read(build_dir/"CMakeCache.txt").decode()
    compiler=re.search(r'set\(CMAKE_C_COMPILER "([^"]+)"\)',read(next((build_dir/"CMakeFiles").glob("*/CMakeCCompiler.cmake"))).decode())[1]
    version=command([compiler,"--version"]).splitlines()[0]
    out.mkdir(exist_ok=True)
    files={}
    for name,raw in (("firmware.bin",binary),("firmware.elf",elf),("build_config.h",config),
                     ("linker.ld",read(build_dir/"generated/WTK_FLASH.ld" if (build_dir/"generated/WTK_FLASH.ld").exists() else REPO/"Firmware/cmake/stm32/STM32F103C8Tx_FLASH.ld"))):
        (out/name).write_bytes(raw); files[name]=dict(bytes=len(raw),sha256=sha(raw))
    identity=boot_identity(profile,state["git_sha"],flags["WTK_PROJECT_VERSION"],
        flags.get("WTK_DIAGNOSTIC_LOG_LEVEL_DEFAULT","5u") not in ("1u","2u","3u"))
    value=dict(schema="WTK_FIRMWARE_2" if flags.get("WTK_NTC_FACTORY_CALIBRATION","0u")=="1u" else "WTK_FIRMWARE_1",source=state,profile=profile,factory_provisioned=spec[2],supplementary_curves=False,
        ntc_factory_profile=load(build_dir/"generated/wtk_ntc_factory_profile.json") if flags.get("WTK_NTC_FACTORY_CALIBRATION","0u")=="1u" else None,
        bringup_k1_test=flags.get("WTK_ENABLE_K1_BENCH_TEST","0u")=="1u",
        bringup_bench_supply=flags.get("WTK_ENABLE_BENCH_SUPPLY","0u")=="1u",
        bringup_peripheral_test=flags.get("WTK_ENABLE_PERIPHERAL_TEST","0u")=="1u",
        preset=preset,build_configuration=spec[3],hardware=HARDWARE,firmware_version=flags["WTK_PROJECT_VERSION"],expected_boot=identity,
        toolchain=dict(compiler_path=compiler,compiler_version=version,cmake=command(["cmake","--version"]).splitlines()[0],cache_sha256=sha(cache.encode())),
        memory=dict(flash_base=BASE,flash_bytes=capacity,ram_base=RAM_BASE,ram_bytes=RAM,internal_page_bytes=1024),linked=metrics,
        binary_size=len(binary),files=files,built_utc=utc())
    value["seal_sha256"]=seal(value); save(out/"manifest.json",value)
    inspect(out/"manifest.json")
    return value


def inspect(path,profile=None):
    path=Path(path); value=checked(load(path)); root=path.parent
    if value.get("schema") not in ("WTK_FIRMWARE_1","WTK_FIRMWARE_2") or value.get("profile") not in PROFILES: raise ValueError("unsupported image manifest")
    spec=PROFILES[value["profile"]]
    capacity=profile_flash_bytes(value["profile"])
    if profile and value["profile"]!=profile: raise ValueError("selected profile differs from artifact")
    if (value["factory_provisioned"],value["supplementary_curves"],value["preset"],value["build_configuration"],value["hardware"])!=(spec[2],False,spec[0],spec[3],HARDWARE): raise ValueError("unsupported configuration")
    if value["memory"]!=dict(flash_base=BASE,flash_bytes=capacity,ram_base=RAM_BASE,ram_bytes=RAM,internal_page_bytes=1024): raise ValueError("wrong linker memory contract")
    if set(value["files"])!={"firmware.bin","firmware.elf","build_config.h","linker.ld"}: raise ValueError("artifact file set mismatch")
    for name,entry in value["files"].items():
        raw=read(root/name)
        if len(raw)!=entry["bytes"] or sha(raw)!=entry["sha256"]: raise ValueError("stale/tampered artifact: "+name)
    binary,metrics=elf_info(read(root/"firmware.elf"),capacity)
    if binary!=read(root/"firmware.bin",capacity) or value["linked"]!=metrics or len(binary)!=value["binary_size"] or len(binary)%2:
        raise ValueError("manifest/ELF/BIN/size mismatch")
    budget=capacity-1024 if spec[2] else capacity
    ram_budget=17408 if spec[2] else 18432
    if len(binary)>budget or metrics["ram_accounted_bytes"]>ram_budget or metrics["reserved_stack_bytes"]<2048: raise ValueError("project memory gate violated")
    flags=parse_config(read(root/"build_config.h"))
    validate_memory_binding(value["profile"],flags)
    ntc_factory.validate_binary(value.get("ntc_factory_profile"), binary, flags)
    if (value["schema"]=="WTK_FIRMWARE_2") != (value.get("ntc_factory_profile") is not None):
        raise ValueError("UID-bound NTC profiles require manifest schema 2")
    bench=value.get("bringup_k1_test",False)
    if (type(bench) is not bool or bench != (flags.get("WTK_ENABLE_K1_BENCH_TEST","0u")=="1u") or
        (bench and (value["profile"]!="BRINGUP" or flags.get("WTK_ENABLE_DC_PILOT")!="0u"))):
        raise ValueError("invalid BRINGUP K1 test configuration")
    supply=value.get("bringup_bench_supply",False)
    if (type(supply) is not bool or supply != (flags.get("WTK_ENABLE_BENCH_SUPPLY","0u")=="1u") or
        (supply and (value["profile"]!="BRINGUP" or bench or flags.get("WTK_ENABLE_DC_PILOT")!="0u"))):
        raise ValueError("invalid BRINGUP bench supply configuration")
    peripheral=value.get("bringup_peripheral_test",False)
    if (type(peripheral) is not bool or peripheral != (flags.get("WTK_ENABLE_PERIPHERAL_TEST","0u")=="1u") or
        (peripheral and (value["profile"]!="BRINGUP" or bench or supply or flags.get("WTK_ENABLE_DC_PILOT")!="0u"))):
        raise ValueError("invalid BRINGUP peripheral test configuration")
    if (flags["WTK_FIRMWARE_PROFILE"],flags["WTK_PRODUCT_FACTORY_PROVISIONED"],flags["WTK_GIT_COMMIT"],flags["WTK_ENABLE_SUPPLEMENTARY_CURVES"],flags["WTK_BUILD_TYPE"],flags["WTK_HARDWARE_COMPATIBILITY"])!=(spec[1],"1u" if spec[2] else "0u",value["source"]["git_sha"][:12],"0u",spec[3],HARDWARE): raise ValueError("generated configuration mismatch")
    if value.get("firmware_version")!=flags.get("WTK_PROJECT_VERSION") or value.get("expected_boot")!=boot_identity(value["profile"],value["source"]["git_sha"],flags["WTK_PROJECT_VERSION"],flags.get("WTK_DIAGNOSTIC_LOG_LEVEL_DEFAULT","5u") not in ("1u","2u","3u")): raise ValueError("boot/version metadata mismatch")
    linker=read(root/"linker.ld").decode()
    if not re.search(r'FLASH\s*\(rx\)\s*:\s*ORIGIN\s*=\s*0x08000000,\s*LENGTH\s*=\s*'+str(capacity//1024)+r'K',linker) or not re.search(r'RAM\s*\(xrw\)\s*:\s*ORIGIN\s*=\s*0x20000000,\s*LENGTH\s*=\s*20K',linker): raise ValueError("linker contract mismatch")
    if not re.fullmatch("[0-9a-f]{40}",value["source"]["git_sha"]): raise ValueError("full source SHA required")
    return value


def build(profile,out,dry_run=False,k1_bench_test=False,bench_supply=False,peripheral_test=False,ntc_factory_profile=None):
    if profile == "FACTORY_PRODUCT_UNIT128" and ntc_factory_profile is None:
        ntc_factory_profile=REPO/"Firmware/config/units"/(UNIT128_UID+"-ntc-engineering.json")
    if k1_bench_test and profile!="BRINGUP": raise ValueError("K1 bench test requires BRINGUP")
    if bench_supply and (profile!="BRINGUP" or k1_bench_test): raise ValueError("bench supply requires a separate BRINGUP image")
    if peripheral_test and (profile!="BRINGUP" or k1_bench_test or bench_supply):
        raise ValueError("peripheral test requires a separate BRINGUP image")
    spec=PROFILES[profile]; out=Path(out).resolve(); folder=out/"build"
    argv=["cmake","--preset",spec[0],"-B",str(folder),"-DWTK_ENABLE_SUPPLEMENTARY_CURVES=OFF",f'-DWTK_PRODUCT_FACTORY_PROVISIONED={"ON" if spec[2] else "OFF"}',
          f'-DWTK_BRINGUP_K1_TEST={"ON" if k1_bench_test else "OFF"}',
          f'-DWTK_BRINGUP_BENCH_SUPPLY={"ON" if bench_supply else "OFF"}',
          f'-DWTK_BRINGUP_PERIPHERAL_TEST={"ON" if peripheral_test else "OFF"}']
    argv.append("-DWTK_NTC_FACTORY_PROFILE="+str(Path(ntc_factory_profile).resolve()) if ntc_factory_profile else "-DWTK_NTC_FACTORY_PROFILE=")
    commands=[argv,["cmake","--build",str(folder),"-j","6"]]
    if dry_run: return dict(dry_run=True,commands=commands,target_access=False)
    out.mkdir(parents=True,exist_ok=False); before=source_state()
    for i,args in enumerate(commands):
        log=command(args,REPO/"Firmware",600); (out/f"build-{i}.log").write_text(log,encoding="utf-8")
    after=source_state()
    if before!=after: raise ValueError("source changed during build; no trustworthy manifest")
    return manifest(folder,out/"image",profile,before,spec[0])


def parse_target(transcript,flash_bytes=FLASH):
    if len(transcript)>1024*1024: raise ValueError("programmer transcript bound")
    transcript=transcript.replace("\r\n","\n")
    matches=re.findall(r'^WTK_B011_TARGET\|([^\r\n]+)$',transcript,re.M)
    if len(matches)!=1: raise ValueError("missing/ambiguous toolkit preflight marker; native log is not identity proof")
    fields=matches[0].split("|")
    if len(fields)!=10: raise ValueError("bad preflight fields")
    device,cpu,kb,obr,wrpr,uid,base,size,driver,state=fields
    target=dict(device_id=int(device,16),cpuid=int(cpu,16),flash_kib=int(kb),obr=int(obr,16),wrpr=int(wrpr,16),uid=uid,
        flash_base=int(base),flash_bytes=int(size),driver=driver,state=state,target_voltage_v=None)
    volts=re.findall(r'^Info\s*:\s*Target voltage:\s*(\d+(?:\.\d+)?)\s*$',transcript,re.M)
    if len(volts)==1: target["target_voltage_v"]=float(volts[0])
    validate_target(target,flash_bytes=flash_bytes)
    return target


def validate_target(t,uid=None,flash_bytes=FLASH):
    if flash_bytes not in (FLASH,131072) or (flash_bytes > FLASH and t["uid"] != UNIT128_UID):
        raise ValueError("unverified extended-Flash unit")
    if t["device_id"]&0xfff!=0x410 or t["cpuid"]>>24!=0x41 or (t["cpuid"]>>4)&0xfff!=0xc23: raise ValueError("unknown STM32 medium-density/Cortex-M3 identity")
    if (t["flash_kib"],t["flash_base"],t["flash_bytes"],t["driver"],t["state"])!=(64,BASE,flash_bytes,"stm32f1x","halted"): raise ValueError("ambiguous/unsupported geometry or reset state")
    if t["obr"]&3 or t["wrpr"]!=0xffffffff: raise ValueError("readout/option/write protection: STOP, never unlock")
    if not re.fullmatch("[0-9a-f]{24}",t["uid"]) or (uid and t["uid"]!=uid): raise ValueError("target UID mismatch")
    if t["target_voltage_v"] is not None and not 3.0<=t["target_voltage_v"]<=3.6: raise ValueError("adapter-reported voltage outside existing A05 3V3 gate")
    return t


class OpenOCD:
    simulated=False
    def __init__(self,executable,scripts,serial,flash_bytes=FLASH):
        self.executable=Path(executable).resolve(); self.scripts=Path(scripts).resolve(); self.serial=serial
        if flash_bytes not in (FLASH,131072): raise ValueError("unsupported programmer geometry")
        self.flash_bytes=flash_bytes
        if not re.fullmatch("[A-Za-z0-9._-]{6,64}",serial): raise ValueError("explicit ST-Link serial selection required (supported ASCII serial)")

    def doctor(self):
        return self.provenance()

    def provenance(self):
        run=subprocess.run([str(self.executable),"--version"],capture_output=True,text=True,timeout=5,creationflags=getattr(subprocess,"CREATE_NO_WINDOW",0))
        version=(run.stdout+run.stderr).splitlines()[0] if run.stdout or run.stderr else "UNKNOWN"
        if run.returncode or version!=OPENOCD_VERSION: raise ValueError("unsupported programmer version; pinned xPack 0.12.0-6 required")
        binary_sha=sha(read(self.executable))
        if binary_sha!=OPENOCD_EXE_SHA256: raise ValueError("programmer executable differs from verified Windows xPack package")
        for name,digest in CONFIG_HASHES.items():
            if sha(read(self.scripts/name))!=digest: raise ValueError("unsupported/modified programmer configuration: "+name)
        return dict(version=version,executable=str(self.executable),executable_sha256=binary_sha,scripts=str(self.scripts),config_hashes=CONFIG_HASHES,
            toolkit_script_sha256=sha(read(TOOLS/"swd_deploy.tcl")),deployment_tool_sha256=sha(read(__file__)),adapter_serial_selection=self.serial)

    def plan(self,action,folder,image=None,uid=""):
        if self.flash_bytes > FLASH and uid != UNIT128_UID: raise ValueError("extended Flash requires the verified expected UID")
        if image and image.get("memory",{}).get("flash_bytes",FLASH) != self.flash_bytes: raise ValueError("image/programmer geometry differs")
        argv=[str(self.executable),"-s",str(self.scripts),"-c","source $::env(WTK_INTERFACE)","-c","transport select dapdirect_swd",
            "-c","adapter serial $::env(WTK_SERIAL)","-c","set FLASH_SIZE $::env(WTK_FLASH_BYTES)","-c","source $::env(WTK_TARGET)",
            "-c","gdb_port disabled; tcl_port disabled; telnet_port disabled","-c","source $::env(WTK_LIBRARY)",
            "-c",'if {[catch {init; reset init; wtk_operation} reason]} {echo "WTK_B011_FAILURE"; echo $reason; shutdown error} else {shutdown}']
        return dict(argv=argv,environment=dict(WTK_SERIAL=self.serial,WTK_ACTION=action,WTK_UID=uid,WTK_FLASH_BYTES=str(self.flash_bytes),WTK_LENGTH=str(image["binary_size"] if image else 0),
            WTK_INTERFACE=str(self.scripts/"interface/stlink.cfg"),WTK_TARGET=str(self.scripts/"target/stm32f1x.cfg"),WTK_LIBRARY=str(TOOLS/"swd_deploy.tcl"),
            WTK_IMAGE=str(Path(image.get("staged_binary",Path(image["manifest_path"]).parent/"firmware.bin")).resolve()) if image else "",
            WTK_BEFORE=str((Path(folder)/"before.bin").resolve()),WTK_READBACK=str((Path(folder)/"full-readback.bin").resolve())),
            internal_flash_range=[BASE,BASE+self.flash_bytes],program_range=[BASE,BASE+image["binary_size"]] if image and action=="program" else None,
            erase_range=[BASE,BASE+math.ceil(image["binary_size"]/1024)*1024] if image and action=="program" else None,
            external_w25q_access=False,warning=WARNING)

    def execute(self,action,folder,image=None,uid=""):
        provenance=self.provenance(); plan=self.plan(action,folder,image,uid); path=Path(folder)/"openocd.txt"
        with path.open("wb") as log:
            process=subprocess.Popen(plan["argv"],stdout=log,stderr=subprocess.STDOUT,env=dict(os.environ,**plan["environment"]),
                creationflags=getattr(subprocess,"CREATE_NO_WINDOW",0))
            start=time.monotonic()
            while process.poll() is None:
                if time.monotonic()-start>60 or path.stat().st_size>1024*1024:
                    process.kill(); process.wait(); raise TimeoutError("programmer timeout/output bound; target/programming outcome UNKNOWN")
                time.sleep(.02)
        transcript=read(path,1024*1024).decode("utf-8",errors="replace")
        if process.returncode or "WTK_B011_FAILURE" in transcript or transcript.count("WTK_B011_COMPLETE")!=1:
            raise ValueError("programmer failed or completion marker missing; inspect transcript")
        target=parse_target(transcript,self.flash_bytes); validate_target(target,uid,self.flash_bytes)
        if action=="program" and transcript.count("WTK_B011_PROGRAMMED")!=1: raise ValueError("programming not confirmed")
        if action!="probe" and transcript.count(f"WTK_B011_READBACK|134217728|{self.flash_bytes}|halted")!=1: raise ValueError("readback marker missing")
        return target,provenance,plan


class FakeSWD:
    """Toolkit contract simulation, NOT a fabricated native OpenOCD output parser."""
    simulated=True
    def __init__(self,path,scenario="normal",flash_bytes=FLASH):
        self.path=Path(path); self.scenario=scenario; self.operations=[]
        if flash_bytes not in (FLASH,131072): raise ValueError("unsupported simulated geometry")
        self.flash_bytes=flash_bytes
        self.external_w25q=bytes.fromhex("1234567890abcdef")

    def plan(self,action,folder,image=None,uid=""):
        if self.flash_bytes > FLASH and uid != UNIT128_UID: raise ValueError("extended simulated profile requires verified UID binding")
        return dict(SIMULATED=True,action=action,internal_flash_range=[BASE,BASE+self.flash_bytes],program_range=[BASE,BASE+image["binary_size"]] if image and action=="program" else None,
            erase_pages=list(range(math.ceil(image["binary_size"]/1024))) if image and action=="program" else [],external_w25q_access=False)

    def execute(self,action,folder,image=None,uid=""):
        if self.scenario in ("missing-adapter","unpowered"): raise OSError("SIMULATED adapter/target unavailable")
        if self.scenario=="timeout": raise TimeoutError("SIMULATED timeout, outcome UNKNOWN")
        self.path.mkdir(parents=True,exist_ok=True); file=self.path/"internal.bin"
        data=bytearray(read(file,self.flash_bytes)) if file.exists() else bytearray(b"\xff"*self.flash_bytes)
        if len(data)!=self.flash_bytes: raise ValueError("bad simulated target memory")
        device=0x20036410 if self.scenario!="wrong-id" else 0x999
        kb=128 if self.scenario=="geometry" else 64; obr=2 if self.scenario=="rdp" else 0
        target_uid=UNIT128_UID if self.flash_bytes > FLASH else "0102030405060708090a0b0c"
        line=f"WTK_B011_TARGET|{device:08x}|411fc231|{kb}|{obr:08x}|ffffffff|{target_uid}|134217728|{self.flash_bytes}|stm32f1x|halted\n"
        target=parse_target(line,self.flash_bytes); validate_target(target,uid,self.flash_bytes)
        plan=self.plan(action,folder,image,uid)
        if action=="program":
            (Path(folder)/"before.bin").write_bytes(data)
            for page in plan["erase_pages"]:
                self.operations.append(("erase_page",page,page)); data[page*1024:(page+1)*1024]=b"\xff"*1024
                if self.scenario=="interrupted": file.write_bytes(data); raise OSError("SIMULATED interrupted erase; no automatic retry")
            binary=read(image.get("staged_binary",Path(image["manifest_path"]).parent/"firmware.bin"),self.flash_bytes); data[:len(binary)]=binary
            file.write_bytes(data); line+="WTK_B011_PROGRAMMED\n"
            if self.scenario=="unexpected-reset": raise ValueError("SIMULATED unexpected reset after program")
        if action!="probe":
            if self.scenario=="readback-mismatch": data[0]^=1
            (Path(folder)/"full-readback.bin").write_bytes(data); line+=f"WTK_B011_READBACK|134217728|{self.flash_bytes}|halted\n"
        if self.scenario!="missing-complete": line+="WTK_B011_COMPLETE\n"
        (Path(folder)/"openocd.txt").write_text("SIMULATED toolkit contract\n"+line,encoding="utf-8")
        if "WTK_B011_COMPLETE" not in line: raise ValueError("SIMULATED zero exit without completion: UNKNOWN")
        return target,dict(version="SIMULATED toolkit contract; no OpenOCD/USB executed"),plan


def evidence(path):
    path=Path(path); return dict(path=str(path.resolve()),sha256=sha(read(path)),value=load(path))


def debug_authorization(session_path,board,operator,supply,acks):
    if not session_path or not all(isinstance(v,str) and v.strip() and v not in ("UNKNOWN","NOT_TESTED") for v in (board,operator,supply)):
        raise ValueError("real probe/verify require inspected B01 --session, --board, --operator and --supply")
    if not acks.get("safe_outputs") or not acks.get("debug_halt_watchdog"):
        raise ValueError("real probe/verify require explicit SAFE outputs and debug-halt/watchdog acknowledgement")
    imported=evidence(session_path); doc=imported["value"]; session=b01.validate_session(doc.get("session",doc))
    if doc.get("SIMULATED") or doc.get("OFFLINE_REPLAY") or any(e["simulated"] for e in session["manual"].values()): raise ValueError("synthetic evidence cannot authorize real debug connection")
    if board!=session["board"]["fabrication_identity"] or session["board"]["revision"]!="Rev1": raise ValueError("inspected board mismatch")
    for key in ("assembly","R02","R05","power","j_pwr_5v","5v_a","3v3","modules"):
        if session["manual"][key]["status"]!="PASS": raise ValueError("missing debug prerequisite: "+key)
    if any(e["status"]=="STOP" for e in session["manual"].values()) or any(c.get("status") in ("STOP","UNKNOWN") for c in doc.get("checks",[])):
        raise ValueError("unresolved B01 STOP/UNKNOWN; debug stage not approved")
    result=dict(operator=operator,board=board,supply=supply,utc=utc(),b01=imported,acknowledgements=acks,SIMULATED=False,permission="RESET_HALT_READ_ONLY_NO_PROGRAM")
    result["seal_sha256"]=seal(result); return result


def authorize(image,session_path,board,operator,supply,previous,uid,accept,acks,simulated,installation=None,cal_readback=None,resource=None):
    ntc_factory.require_target(image.get("ntc_factory_profile"), uid)
    if image["source"]["dirty"] and not simulated: raise ValueError("dirty-source image cannot be physically deployed")
    if not re.fullmatch("[0-9a-f]{24}",uid): raise ValueError("explicit inspected target UID required")
    for name,value in (("board",board),("operator",operator),("independent supply arrangement",supply)):
        if not isinstance(value,str) or not value.strip() or value in ("UNKNOWN","NOT_TESTED"): raise ValueError("record "+name)
    if previous not in ("FACTORY_BLANK","BRINGUP","BRINGUP_CAL","FACTORY_PRODUCT","RECOVERY_REVIEWED"): raise ValueError("declare previous programming stage")
    if set(acks)!={"safe_outputs","debug_halt_watchdog","no_app_bootloader","internal_flash"} or not all(acks.values()): raise ValueError("explicit programming-stage acknowledgements required")
    challenge=f'PROGRAM {image["profile"]} {image["files"]["firmware.bin"]["sha256"][:12]} {uid}'
    if accept!=challenge: raise ValueError("explicit internal-Flash approval required: "+challenge)
    imported=evidence(session_path); doc=imported["value"]; session=b01.validate_session(doc.get("session",doc))
    if board!=session["board"]["fabrication_identity"] or session["board"]["revision"]!="Rev1": raise ValueError("board identity differs from inspected B01 evidence")
    synthetic=doc.get("SIMULATED",False) or doc.get("OFFLINE_REPLAY",False) or any(e["simulated"] for e in session["manual"].values())
    if synthetic and not simulated: raise ValueError("synthetic/replayed evidence cannot authorize physical programming")
    for key,entry in session["manual"].items():
        if entry["status"]=="STOP": raise ValueError("unresolved B01 STOP: "+key)
    gates=("assembly","R02","R05","power","j_pwr_5v","5v_a","3v3","modules")
    if image["profile"]!="BRINGUP": gates+=("safe_boot",)
    for key in gates:
        entry=session["manual"][key]
        if entry["status"]!="PASS" or (entry["simulated"] and not simulated): raise ValueError("missing physical prerequisite: "+key)
    for field in ("population_notes","dnp_verification"):
        if session["board"].get(field) in (None,"","UNKNOWN","NOT_TESTED"): raise ValueError("record assembly "+field)
    if not session["board"]["photographs"]: raise ValueError("board photograph evidence missing")
    for category in ("modules","instruments"):
        if any(v in (None,"","UNKNOWN") for v in session[category].values()): raise ValueError("identify modules/instruments before programming")
    for check in doc.get("checks",[]):
        if check.get("status") in ("STOP","UNKNOWN"): raise ValueError("unresolved B01 diagnostic "+str(check.get("check")))
    result=dict(operator=operator,utc=utc(),board=board,supply=supply,previous_profile=previous,next_profile=image["profile"],
        expected_uid=uid,approval=accept,acknowledgements=acks,b01=imported,SIMULATED=simulated,
        image_sha256=image["files"]["firmware.bin"]["sha256"],manifest_seal=image["seal_sha256"],
        analog_clamp_risk=session["manual"]["bat54s_conduction"]["status"],resource_pack=dict(status="UNKNOWN",reference=None))
    if image["profile"].startswith("FACTORY_PRODUCT"):
        if previous not in ("BRINGUP_CAL","FACTORY_PRODUCT","RECOVERY_REVIEWED"): raise ValueError("factory PRODUCT requires reviewed BRINGUP_CAL provisioning stage")
        if not installation or not cal_readback: raise ValueError("separate OSL installation report AND canonical readback required")
        observed=evidence(installation); report=observed["value"]; frame=read(cal_readback,2760); summary=validate_candidate(frame)
        status=report.get("status",{}); device=report.get("device",{})
        if (report.get("verified") is not True or report.get("candidate_sha256")!=sha(frame) or report.get("readback_sha256")!=sha(frame) or
            status.get("state")!=4 or status.get("error")!=0 or status.get("active_sequence")!=summary.sequence or not status.get("active_valid") or device.get("uid")!=uid):
            raise ValueError("OSL installation/readback/device identity mismatch")
        if device.get("synthetic") and not simulated: raise ValueError("synthetic installation cannot authorize physical PRODUCT")
        result["osl"]=dict(report=observed,readback_path=str(Path(cal_readback).resolve()),sha256=sha(frame),schema=2,model=5,sequence=summary.sequence,
            limitation="Hash/report consistency, not a fresh W25Q read or physical accuracy certificate")
    if resource:
        result["resource_pack"]["reference"]=evidence(resource)
        # No guessed PRODUCT/resource diagnostic protocol. Preserve an external report,
        # without inferring readiness from its presence or from a generic JSON PASS.
        result["resource_pack"]["status"]="EXTERNAL_REFERENCE_REQUIRES_OPERATOR_REVIEW"
    if len(json.dumps(result,allow_nan=False))>512*1024: raise ValueError("authorization evidence exceeds 512 KiB; use compact B01 session with raw references")
    result["seal_sha256"]=seal(result)
    return result


def deployment(action,manifest_path,backend,out,authorization=None,uid="",dry_run=False):
    image=inspect(manifest_path) if manifest_path else None
    capacity=image["memory"]["flash_bytes"] if image else getattr(backend,"flash_bytes",FLASH)
    if capacity != getattr(backend,"flash_bytes",FLASH): raise ValueError("image/backend Flash geometry mismatch")
    if image: image=dict(image,manifest_path=str(Path(manifest_path).resolve()))
    if action=="program" and image:
        ntc_factory.require_target(image.get("ntc_factory_profile"), uid)
    out=Path(out).resolve()
    if dry_run:
        return dict(dry_run=True,target_access=False,commands=backend.plan(action,out,image,uid),
                    warning=WARNING,approval_required=action=="program",image=image)
    if action=="program" and authorization is None: raise ValueError("programming requires explicit reviewed authorization")
    if action in ("probe","verify") and not backend.simulated:
        if authorization is None or checked(authorization).get("permission")!="RESET_HALT_READ_ONLY_NO_PROGRAM" or authorization.get("SIMULATED") is not False:
            raise ValueError("real debug connection requires reviewed non-programming stage authorization")
    if action in ("program","verify") and image is None: raise ValueError("verified image manifest required")
    if action=="program":
        checked(authorization)
        if (authorization["image_sha256"],authorization["manifest_seal"],authorization["next_profile"],authorization["expected_uid"],authorization["SIMULATED"])!=(image["files"]["firmware.bin"]["sha256"],image["seal_sha256"],image["profile"],uid,backend.simulated):
            raise ValueError("authorization cannot be replayed for another image/profile/UID/backend")
    out.mkdir(parents=True,exist_ok=False)  # Atomic reservation prevents duplicate/colliding requests.
    expected_binary=None
    if image:
        expected_binary=read(Path(manifest_path).parent/"firmware.bin",capacity)
        if sha(expected_binary)!=image["files"]["firmware.bin"]["sha256"]: raise ValueError("image changed before staging")
        (out/"staged-image.bin").write_bytes(expected_binary)
        image["staged_binary"]=str(out/"staged-image.bin")
    record=dict(schema="WTK_DEPLOYMENT_1",transaction=str(uuid.uuid4()),operation=action,started_utc=utc(),SIMULATED=backend.simulated,
        image=image,authorization=authorization,programming="NOT_STARTED" if action=="program" else "NOT_TESTED",internal_readback="NOT_TESTED",boot="NOT_TESTED",
        uart_identity="NOT_TESTED",application="NOT_TESTED",external_w25q_access=False,physically_validated=False,
        outcome="IN_PROGRESS",error=None,transcripts=[],warning=WARNING)
    def persist():
        record["seal_sha256"]=seal(record); save(out/"deployment.json",record)
    persist()
    try:
        if action=="program":
            # Fresh non-programming preflight also validates reported voltage BEFORE writes.
            pre=out/"preflight"; pre.mkdir()
            target,provenance,plan=backend.execute("probe",pre,None,uid)
            validate_target(target,uid,capacity)
            record["preflight"]=dict(target=target,programmer=provenance,plan=plan)
            record["transcripts"].append(dict(path="preflight/openocd.txt",sha256=sha(read(pre/"openocd.txt"))))
            persist()
        if action=="program": record["programming"]="REQUESTED_OUTCOME_UNKNOWN"; persist()
        if image and sha(read(out/"staged-image.bin",capacity))!=image["files"]["firmware.bin"]["sha256"]: raise ValueError("staged image changed; STOP")
        target,provenance,plan=backend.execute(action,out,image,uid)
        record.update(target=target,programmer=provenance,plan=plan)
        record["transcripts"].append(dict(path="openocd.txt",sha256=sha(read(out/"openocd.txt"))))
        if action=="program": record["programming"]="CONFIRMED"
        if action!="probe":
            full=read(out/"full-readback.bin",capacity)
            if len(full)!=capacity: raise ValueError("incomplete internal-Flash readback")
            binary=expected_binary
            prefix=full[:len(binary)]; record["readback"]=dict(start=BASE,end_exclusive=BASE+len(binary),bytes=len(prefix),
                expected_sha256=sha(binary),actual_sha256=sha(prefix),full_flash_sha256=sha(full),full_flash_bytes=capacity)
            (out/"image-readback.bin").write_bytes(prefix)
            if prefix!=binary: record["internal_readback"]="FAIL"; raise ValueError("programmed byte-for-byte readback mismatch")
            if action=="program":
                before=read(out/"before.bin",capacity); end=math.ceil(len(binary)/1024)*1024
                if len(before)!=capacity or before[end:]!=full[end:]: raise ValueError("bytes outside approved internal pages changed")
                record["unaffected_internal_pages_preserved"]=True
            record["internal_readback"]="PASS"
        record["outcome"]="SIMULATED_VERIFIED" if backend.simulated else "TARGET_IDENTIFIED" if action=="probe" else "INTERNAL_FLASH_VERIFIED"
        record["next_step"]=("Start pc_bringup.py listener before separately authorized manual boot." if image and image["profile"]=="BRINGUP" else
            "Separately boot BRINGUP_CAL, identify/status via pc_cal_capture.py, then the OSL workbench; no automatic capture/install." if image and image["profile"]=="BRINGUP_CAL" else
            "Separately boot PRODUCT; limited SAFE_BOOT UART banner cannot prove SHA/calibration/resources/readiness. Inspect UI and physical checklist.")
    except (ValueError,OSError,TimeoutError,subprocess.SubprocessError) as exc:
        record.update(outcome="FAILED_OR_UNKNOWN",error=str(exc))
        if action=="program" and record["programming"]=="REQUESTED_OUTCOME_UNKNOWN": record["programming"]="UNKNOWN_MAY_BE_PARTIAL"
        if record["internal_readback"]=="NOT_TESTED": record["internal_readback"]="UNKNOWN"
    finally:
        record["completed_utc"]=utc(); persist()
        write_report(out/"deployment.json",out/"deployment.md")
    return record


def write_report(path,out):
    record=checked(load(path)); label="SIMULATED" if record["SIMULATED"] else "REAL_BACKEND / NOT PHYSICALLY QUALIFIED"
    Path(out).write_text("# Firmware deployment — "+label+"\n\n"+WARNING+"\n\n"+
        f'Outcome: {record["outcome"]}\n\nProgramming: {record["programming"]}; internal readback: {record["internal_readback"]}; boot: {record["boot"]}; UART: {record["uart_identity"]}; application: {record["application"]}.\n\n'+
        "External W25Q was not accessed. OSL/Resource Pack remain independently accepted prerequisites.\n\n```json\n"+json.dumps(record,indent=2)+"\n```\n",encoding="utf-8")
    return record


def doctor(openocd=None,scripts=None):
    tools={}
    for exe,args in (("git",["--version"]),("cmake",["--version"]),("ninja",["--version"]),("arm-none-eabi-gcc",["--version"])):
        path=shutil.which(exe)
        tools[exe]=dict(path=path,status="AVAILABLE" if path else "MISSING",version=command([path,*args]).splitlines()[0] if path else None)
    if openocd and scripts:
        try: tools["openocd"]=dict(status="AVAILABLE",**OpenOCD(openocd,scripts,"DOCTOR").provenance())
        except (ValueError,OSError,subprocess.SubprocessError) as exc: tools["openocd"]=dict(status="UNSUPPORTED_OR_MISSING",error=str(exc))
    else: tools["openocd"]=dict(status="MISSING_OR_UNSELECTED",instruction="select pinned xPack 0.12.0-6 executable and scripts; no adapter is opened by doctor")
    return dict(tools=tools,target_access=False,warning=WARNING)


def observe(deployment_path,evidence_path,out):
    """Import a separate actual observation; never query a guessed PRODUCT protocol."""
    record=checked(load(deployment_path)); observed=evidence(evidence_path); value=observed["value"]
    if not record["SIMULATED"] and (value.get("SIMULATED") or value.get("OFFLINE_REPLAY") or value.get("synthetic")): raise ValueError("synthetic observation cannot confirm physical boot")
    profile=record["image"]["profile"]
    identity=value.get("identity",value)
    if profile=="BRINGUP" and record["image"]["expected_boot"]["interface"]=="LIMITED_UART_UI_REQUIRED":
        status="UNKNOWN: minimal diagnostic banner requires independent SWD identity and bench acceptance"
    elif profile=="BRINGUP":
        if identity.get("profile")!="BRINGUP" or identity.get("git")!=record["image"]["source"]["git_sha"][:12]: raise ValueError("BRINGUP banner identity mismatch")
        status="OBSERVED_MATCHING_BRINGUP_BANNER"
    elif profile=="BRINGUP_CAL":
        if identity.get("profile")!=1 or identity.get("git")!=record["image"]["source"]["git_sha"][:8] or identity.get("uid")!=record["target"]["uid"] or not identity.get("capabilities",0)&16: raise ValueError("PLC1 identity/capability mismatch")
        status="OBSERVED_MATCHING_PLC1_IDENTITY"
    else:
        status="UNKNOWN: PRODUCT only has limited SAFE_BOOT banner; UI/bench evidence required"
    result=dict(deployment_reference=dict(path=str(Path(deployment_path).resolve()),sha256=sha(read(deployment_path))),observation=observed,
        identity_status=status,boot="REQUIRES_INDEPENDENT_REVIEW",application="NOT_TESTED",SIMULATED=record["SIMULATED"],physically_validated=False)
    if Path(out).exists(): raise ValueError("observation report destination already exists")
    save(out,result); return result


def import_transcript(deployment_path,transcript,out):
    record=checked(load(deployment_path)); raw=read(transcript,1024*1024)
    try: target=parse_target(raw.decode("utf-8")); interpretation="TOOLKIT_PREFLIGHT_PRESENT / no programming or boot inference"
    except (ValueError,UnicodeError): target=None; interpretation="UNKNOWN native/partial log; not guessed from unverified output grammar"
    result=dict(deployment_path=str(Path(deployment_path).resolve()),deployment_sha256=sha(read(deployment_path)),
        transcript=dict(path=str(Path(transcript).resolve()),sha256=sha(raw),bytes=len(raw)),target=target,interpretation=interpretation,
        internal_readback="NOT_TESTED",boot="NOT_TESTED",application="NOT_TESTED",SIMULATED=record["SIMULATED"],physically_validated=False)
    if Path(out).exists(): raise ValueError("transcript sidecar destination already exists")
    save(out,result); return result


def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("action",choices=("doctor","build","inspect","probe","program","verify","report"))
    p.add_argument("--profile",choices=tuple(PROFILES)); p.add_argument("--manifest",type=Path); p.add_argument("--out",type=Path)
    p.add_argument("--dry-run",action="store_true"); p.add_argument("--backend",choices=("openocd","fake"),default="openocd")
    p.add_argument("--k1-bench-test",action="store_true",help="build the optional BRINGUP empty-fixture relay test variant")
    p.add_argument("--bench-supply",action="store_true",help="build the optional BRINGUP external-5V bench variant")
    p.add_argument("--peripheral-test",action="store_true",help="build standalone BRINGUP display/buttons/buzzer diagnostic")
    p.add_argument("--ntc-factory-profile",type=Path,help="build with an experimental NTC profile bound to one MCU UID")
    p.add_argument("--openocd",type=Path); p.add_argument("--scripts",type=Path); p.add_argument("--adapter-serial")
    p.add_argument("--fake-device",type=Path); p.add_argument("--fake-scenario",choices=("normal","missing-adapter","unpowered","wrong-id","geometry","rdp","timeout","interrupted","readback-mismatch","missing-complete","unexpected-reset"),default="normal")
    p.add_argument("--session",type=Path); p.add_argument("--board"); p.add_argument("--operator"); p.add_argument("--supply")
    p.add_argument("--previous",choices=("FACTORY_BLANK","BRINGUP","BRINGUP_CAL","FACTORY_PRODUCT","RECOVERY_REVIEWED"))
    p.add_argument("--expected-uid",default=""); p.add_argument("--accept-internal-flash")
    for flag in ("safe-outputs","debug-halt-watchdog","no-app-bootloader","internal-flash"): p.add_argument("--ack-"+flag,action="store_true")
    p.add_argument("--installation",type=Path); p.add_argument("--cal-readback",type=Path); p.add_argument("--resource-evidence",type=Path)
    p.add_argument("--deployment",type=Path); p.add_argument("--observed",type=Path); p.add_argument("--programmer-transcript",type=Path)
    args=p.parse_args(argv)
    try:
        if args.k1_bench_test and args.action!="build": raise ValueError("--k1-bench-test is a build option; inspect/program use manifest identity")
        if args.bench_supply and args.action!="build": raise ValueError("--bench-supply is a build option; inspect/program use manifest identity")
        if args.peripheral_test and args.action!="build": raise ValueError("--peripheral-test is a build option; inspect/program use manifest identity")
        if args.ntc_factory_profile and args.action!="build": raise ValueError("--ntc-factory-profile is a build option; program uses sealed manifest identity")
        if args.action=="doctor": result=doctor(args.openocd,args.scripts) if not args.dry_run else dict(dry_run=True,target_access=False,commands=["git --version","cmake --version","ninja --version","arm-none-eabi-gcc --version","selected openocd --version"])
        elif args.action=="build":
            if not args.profile or not args.out: raise ValueError("build needs --profile and new --out")
            result=build(args.profile,args.out,args.dry_run,args.k1_bench_test,args.bench_supply,args.peripheral_test,args.ntc_factory_profile)
        elif args.action=="inspect":
            if not args.manifest: raise ValueError("inspect needs --manifest")
            result=inspect(args.manifest,args.profile)
        elif args.action=="report":
            if not args.deployment or not args.out: raise ValueError("report needs --deployment and new --out")
            if args.out.exists(): raise ValueError("report destination already exists")
            if args.observed and args.programmer_transcript: raise ValueError("import one independently identified evidence source per sidecar")
            result=observe(args.deployment,args.observed,args.out) if args.observed else import_transcript(args.deployment,args.programmer_transcript,args.out) if args.programmer_transcript else write_report(args.deployment,args.out)
        else:
            if not args.out: raise ValueError("probe/program/verify need unique --out")
            if args.action!="probe":
                if not args.manifest: raise ValueError("program/verify require a manifest even for dry-run")
                inspect(args.manifest,args.profile)
            capacity=profile_flash_bytes(inspect(args.manifest)["profile"] if args.manifest else args.profile)
            if args.backend=="fake":
                if not args.fake_device: raise ValueError("fake backend needs --fake-device (always SIMULATED)")
                backend=FakeSWD(args.fake_device,args.fake_scenario,capacity)
            else:
                if not args.openocd or not args.scripts or not args.adapter_serial: raise ValueError("select pinned --openocd, --scripts and --adapter-serial; no automatic adapter selection")
                backend=OpenOCD(args.openocd,args.scripts,args.adapter_serial,capacity)
            approval=None
            acks=dict(safe_outputs=args.ack_safe_outputs,debug_halt_watchdog=args.ack_debug_halt_watchdog,no_app_bootloader=args.ack_no_app_bootloader,internal_flash=args.ack_internal_flash)
            if args.action in ("probe","verify") and not backend.simulated and not args.dry_run:
                approval=debug_authorization(args.session,args.board,args.operator,args.supply,acks)
            if args.action=="program" and not args.dry_run:
                if not args.profile or not args.manifest or not args.session: raise ValueError("program needs selected --profile, --manifest, --session and explicit stage authorization")
                image=inspect(args.manifest,args.profile)
                approval=authorize(image,args.session,args.board,args.operator,args.supply,args.previous,args.expected_uid,args.accept_internal_flash,acks,backend.simulated,args.installation,args.cal_readback,args.resource_evidence)
            result=deployment(args.action,args.manifest,backend,args.out,approval,args.expected_uid,args.dry_run)
        print(json.dumps(result,indent=2,allow_nan=False))
        return 1 if result.get("outcome")=="FAILED_OR_UNKNOWN" else 0
    except (ValueError,KeyError,OSError,subprocess.SubprocessError) as exc:
        p.error(str(exc))


if __name__=="__main__": raise SystemExit(main())

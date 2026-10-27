#!/usr/bin/env python3
"""Build canonical ARM profiles and collect A01 evidence without editing a checkout.

--baseline SHA builds archived tracked firmware at that SHA with the current pinned
submodules. Raw logs/maps stay in the ignored output directory; sizes.csv is compact.
"""
import argparse
import csv
import io
import json
import os
import re
from pathlib import Path
import subprocess
import sys
import tarfile


def run(argv, cwd, log, env=None):
    with log.open('w', encoding='utf-8') as stream:
        subprocess.run([str(x) for x in argv], cwd=cwd, stdout=stream,
                       stderr=subprocess.STDOUT, check=True, env=env)


def verify_configuration(cache, factory, curves):
    """Fail before linking if flags were passed literally or a stale cache won."""
    for key, expected in (('WTK_PRODUCT_FACTORY_PROVISIONED', factory),
                          ('WTK_ENABLE_SUPPLEMENTARY_CURVES', curves)):
        match = re.search(r'^' + key + r':[^=]+=(.*)$', cache, re.MULTILINE)
        if match is None or match[1].strip() != expected:
            raise ValueError(f'{key} must be exactly {expected}')


def pin_archived_banner(text, sha):
    """An archive beneath a checkout must not inherit the checkout's HEAD banner."""
    if re.fullmatch(r'[0-9a-f]{40}', sha) is None:
        raise ValueError('baseline must resolve to a full commit SHA')
    command = 'COMMAND ${GIT_EXECUTABLE} rev-parse --short=12 HEAD'
    if text.count(command) != 1:
        raise ValueError('unsupported archived Git banner detector')
    return text.replace(command, f'COMMAND ${{CMAKE_COMMAND}} -E echo {sha[:12]}')


def host_matrix(firmware, out):
    for kind in ('debug', 'release'):
        config = kind.capitalize()
        for factory in ('OFF', 'ON'):
            for curves in ('OFF', 'ON'):
                name = f'host-{kind}-factory-{factory}-curves-{curves}'
                build = out / name
                run(['cmake', '--preset', 'host-' + kind, '-B', build,
                     f'-DWTK_PRODUCT_FACTORY_PROVISIONED={factory}',
                     f'-DWTK_ENABLE_SUPPLEMENTARY_CURVES={curves}'], firmware, out / (name+'-configure.log'))
                verify_configuration((build / 'CMakeCache.txt').read_text(), factory, curves)
                run(['cmake', '--build', build, '--config', config, '-j', '6'], firmware, out / (name+'-build.log'))
                run(['ctest', '--test-dir', build, '-C', config, '--output-on-failure'], firmware, out / (name+'-ctest.log'))
                bridge = build / 'tests' / config / 'wtk_pc_cal_capture_bridge.exe'
                if not bridge.exists():
                    bridge = build / 'tests/wtk_pc_cal_capture_bridge'
                if not bridge.exists():
                    raise FileNotFoundError('C fixture is required')
                env = dict(os.environ, WTK_CAPTURE_BRIDGE=str(bridge))
                run([sys.executable, '-m', 'unittest', 'discover', '-s', 'tests/tools', '-v'],
                    firmware, out / (name+'-python.log'), env)
                print(name, 'CTest and Python passed', flush=True)


def current_matrix(firmware, out):
    """Current deployable unit/recovery profiles and an explicit generic budget stop."""
    ntc=firmware/'config/units/54ff70068370525760380567-ntc-engineering.json'
    cases=[('unit-release','stm32-unit-release',[],False),
           ('unit-debug','stm32-unit-release',['-DCMAKE_BUILD_TYPE=Debug','-DCMAKE_C_FLAGS_DEBUG=-g3'],False),
           ('unit-no-serial','stm32-unit-release',['-DWTK_ENABLE_SERIAL_UI=OFF'],False),
           ('bringup','stm32-bringup',[],False),
           ('bringup-cal','stm32-bringup-cal',[],False),
           ('bringup-k1','stm32-bringup',['-DWTK_BRINGUP_K1_TEST=ON'],False),
           ('bringup-bench','stm32-bringup',['-DWTK_BRINGUP_BENCH_SUPPLY=ON'],False),
           ('bringup-peripheral','stm32-bringup',['-DWTK_BRINGUP_PERIPHERAL_TEST=ON'],False),
           ('generic-product-budget','stm32-factory-release',[],True)]
    rows=[]
    for name,preset,extra,budget_stop in cases:
        build=out/name
        run(['cmake','--preset',preset,'-B',build,'-DWTK_FLASH_FORENSICS=ON',
             '-DWTK_ENABLE_SUPPLEMENTARY_CURVES=OFF',f'-DWTK_NTC_FACTORY_PROFILE={ntc}',*extra],
            firmware,out/(name+'-configure.log'))
        log=out/(name+'-build.log')
        with log.open('w',encoding='utf-8') as stream:
            result=subprocess.run(['cmake','--build',str(build),'-j','6'],cwd=firmware,
                                  stdout=stream,stderr=subprocess.STDOUT)
        if budget_stop:
            text=log.read_text(encoding='utf-8',errors='replace')
            if result.returncode==0 or not any(token in text for token in ('overflowed','will not fit','project size budget exceeded')):
                raise RuntimeError('generic 64-KiB stop must be a proven memory gate, not a compile error')
            row=dict(profile=name,outcome='EXPECTED_64K_BUDGET_STOP')
        else:
            if result.returncode:raise RuntimeError('profile failed: '+name)
            size=json.loads((build/'WTK.RLCMeter.size.json').read_text())
            row=dict(profile=name,outcome='PASS',flash=size['flash_bytes'],ram=size['ram_accounted_bytes'],
                     stack=size['reserved_stack_bytes'],flash_capacity=size['budgets']['flash_silicon_bytes'])
        rows.append(row);(out/'current-matrix.json').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8')
        print(name,row['outcome'],flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--baseline')
    parser.add_argument('--host', action='store_true', help='Run both PRODUCT modes, curves and host build types')
    args = parser.parse_args()
    firmware = Path(__file__).resolve().parents[1]
    repo = firmware.parent
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    if args.host:
        if args.baseline:
            parser.error('--host cannot be combined with --baseline')
        host_matrix(firmware, out)
        return
    if not args.baseline:
        current_matrix(firmware,out)
        return
    sha = subprocess.check_output(['git', 'rev-parse', '--verify',
                                   (args.baseline or 'HEAD') + '^{commit}'], cwd=repo, text=True).strip()
    source = firmware
    roots = []
    if args.baseline:
        archive = subprocess.check_output(['git', 'archive', sha, 'Firmware'], cwd=repo)
        archive_root = out / 'source'
        archive_root.mkdir(exist_ok=True)
        with tarfile.open(fileobj=io.BytesIO(archive)) as package:
            for member in package.getmembers():
                target = (archive_root / member.name).resolve()
                if archive_root.resolve() not in target.parents or member.issym() or member.islnk():
                    raise ValueError('unsafe archive member')
            package.extractall(archive_root)
        source = archive_root / 'Firmware'
        # Metadata only: the archived firmware's banner records its own source SHA.
        options = source / 'cmake/modules/BuildOptions.cmake'
        options.write_text(pin_archived_banner(options.read_text(), sha))
        for key, name in (('WTK_STM32_CMSIS_CORE_ROOT', 'cmsis_core'),
                          ('WTK_STM32_CMSIS_DEVICE_F1_ROOT', 'cmsis_device_f1'),
                          ('WTK_STM32F1_HAL_DRIVER_ROOT', 'stm32f1xx_hal_driver')):
            dependency = firmware / 'third_party/st' / name
            expected = subprocess.check_output(['git', 'rev-parse',
                f'{sha}:Firmware/third_party/st/{name}'], cwd=repo, text=True).strip()
            installed = subprocess.check_output(['git', 'rev-parse', 'HEAD'],
                cwd=dependency, text=True).strip()
            if installed != expected:
                raise ValueError(f'baseline submodule mismatch: {name}; use a matching isolated checkout')
            roots.append(f'-D{key}={dependency}')
    rows = []
    for preset in ('stm32-debug', 'stm32-release', 'stm32-bringup', 'stm32-bringup-cal'):
        modes = ('legacy', 'factory') if not args.baseline and preset in ('stm32-debug', 'stm32-release') else ('legacy',)
        for mode in modes:
            for curves in ('OFF', 'ON'):
                name = f'{preset}-{mode}-{curves}'
                build = out / name
                run(['cmake', '--preset', preset, '-B', build, '-DWTK_FLASH_FORENSICS=ON',
                     f'-DWTK_ENABLE_SUPPLEMENTARY_CURVES={curves}',
                     f'-DWTK_PRODUCT_FACTORY_PROVISIONED={"ON" if mode == "factory" else "OFF"}', *roots],
                    source, out / f'{name}-configure.log')
                verify_configuration((build / 'CMakeCache.txt').read_text(), 'ON' if mode == 'factory' else 'OFF', curves)
                run(['cmake', '--build', build, '-j', '6'], source, out / f'{name}-build.log')
                run([sys.executable, source / 'tools/collect_flash_evidence.py', build,
                     '--profile', name, '--git-sha', sha], source, out / f'{name}-evidence.log')
                size = json.loads((build / 'WTK.RLCMeter.size.json').read_text())
                rows.append(dict(profile=preset, mode=mode, curves=curves, flash=size['flash_bytes'],
                                 ram=size['ram_accounted_bytes'], stack=size['reserved_stack_bytes'],
                                 flash_margin=65536-size['flash_bytes']))
                print(name, rows[-1], flush=True)
    with (out / 'sizes.csv').open('w', newline='', encoding='utf-8') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader(); writer.writerows(rows)


if __name__ == '__main__':
    main()

"""Isolated linker experiments, NEVER deployable firmware.

Archive tracked HEAD into a new directory. Public wizard calls become fail-closed
stubs; runtime coefficient application is never stubbed. Vendor roots stay pinned
to the initialized checkout. This measures link reachability, not a finished port.
"""
import argparse
import io
import json
import pathlib
import re
import subprocess
import tarfile


def replace_body(text, name, body):
    match = re.search(r'\b'+re.escape(name)+r'\s*\([^;]*?\)\s*\{', text, re.S)
    if not match:
        raise ValueError('missing probe function '+name)
    start = match.end()-1
    depth, end = 1, start+1
    while depth:
        depth += (text[end] == '{')-(text[end] == '}')
        end += 1
    return text[:start]+'{\n'+body+'\n}'+text[end:]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', required=True, type=pathlib.Path)
    args = parser.parse_args()
    repo = pathlib.Path(__file__).resolve().parents[2]
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)  # Never replace or clean existing work.
    sha = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=repo, text=True).strip()
    archive = subprocess.check_output(['git', 'archive', 'HEAD', 'Firmware'], cwd=repo)
    results = {'source_sha': sha, 'warning': 'LINKER_EXPERIMENT_ONLY_NEVER_FLASH', 'variants': {}}
    for variant in ('baseline', 'solver_stub', 'wizard_stub', 'wizard_stub_capture_retained',
                    'wizard_stub_capture_install_retained'):
        root = out/variant
        root.mkdir()
        with tarfile.open(fileobj=io.BytesIO(archive)) as bundle:
            bundle.extractall(root)
        firmware = root/'Firmware'
        solver = firmware/'src/measurement/measurement_calibration_solver.c'
        wizard = firmware/'src/app/app_calibration_wizard.c'
        if variant == 'solver_stub':
            solver.write_text(replace_body(solver.read_text(), 'measurement_cal_solver_solve',
                '(void)input; (void)solution; return MEASUREMENT_CAL_SOLVER_UNSUPPORTED_MODEL;'))
        if variant.startswith('wizard_stub'):
            text = wizard.read_text()
            for name, body in {
                'app_calibration_wizard_start': '(void)wizard; (void)mode; (void)sequence; (void)temperature_mC; (void)temperature_valid; return BSP_STATUS_ERROR;',
                'app_calibration_wizard_confirm': '(void)wizard;',
                'app_calibration_wizard_cancel': '(void)wizard; return BSP_STATUS_OK;',
                'app_calibration_wizard_step': '(void)wizard; (void)safety; (void)clock_summary; (void)clock_status; (void)temperature_mC; (void)temperature_valid; (void)now_ms;',
                'app_calibration_wizard_active': '(void)wizard; return false;',
                'app_calibration_wizard_terminal': '(void)wizard; return false;',
                'app_calibration_wizard_snapshot': '(void)wizard; if (snapshot != NULL) { *snapshot = (app_cal_wizard_snapshot_t){.state = APP_CAL_WIZARD_FAILED}; }',
            }.items():
                text = replace_body(text, name, body)
            if variant.endswith('_retained'):
                names = ('init', 'start', 'step', 'cancel', 'active', 'evidence')
                text += '\nvoid (* const a06_capture_anchors[])(void) = {\n'+',\n'.join(
                    '(void (*)(void))app_calibration_session_'+name for name in names)+'\n};\n'
                if variant.endswith('install_retained'):
                    services = ('candidate_begin', 'candidate_discard', 'candidate_set',
                                'candidate_validity', 'candidate_insert_record', 'candidate_commit_start', 'step')
                    text += '\nvoid (* const a06_install_anchors[])(void) = {\n'+',\n'.join(
                        '(void (*)(void))app_calibration_service_'+name for name in services)+'\n};\n'
            wizard.write_text(text)
        command = ['cmake', '-S', str(firmware), '-B', str(root/'build'), '-G', 'Ninja',
                   '-DCMAKE_BUILD_TYPE=Release', '-DWTK_TARGET_PLATFORM=stm32',
                   '-DCMAKE_C_FLAGS_RELEASE=-g0 -DNDEBUG',
                   '-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON',
                   '-DWTK_FIRMWARE_PROFILE=PRODUCT', '-DWTK_FLASH_FORENSICS=ON',
                   '-DWTK_ENABLE_SUPPLEMENTARY_CURVES=OFF',
                   '-DCMAKE_TOOLCHAIN_FILE='+str(firmware/'cmake/toolchains/arm-none-eabi-gcc.cmake')]
        for option, directory in (('WTK_STM32_CMSIS_CORE_ROOT', 'cmsis_core'),
                                  ('WTK_STM32_CMSIS_DEVICE_F1_ROOT', 'cmsis_device_f1'),
                                  ('WTK_STM32F1_HAL_DRIVER_ROOT', 'stm32f1xx_hal_driver')):
            command.append('-D'+option+'='+str(repo/'Firmware/third_party/st'/directory))
        if variant.endswith('_retained'):
            flags = '-Wl,--undefined=a06_capture_anchors'
            if variant.endswith('install_retained'):
                flags += ' -Wl,--undefined=a06_install_anchors'
            command.append('-DCMAKE_EXE_LINKER_FLAGS='+flags)
        with (root/'configure.log').open('w') as log:
            subprocess.run(command, check=True, stdout=log, stderr=subprocess.STDOUT)
        with (root/'build.log').open('w') as log:
            subprocess.run(['cmake', '--build', str(root/'build')], check=True, stdout=log, stderr=subprocess.STDOUT)
        data = json.loads((root/'build/WTK.RLCMeter.size.json').read_text())
        results['variants'][variant] = {key: data[key] for key in ('flash_bytes', 'ram_accounted_bytes')}
        print(variant, results['variants'][variant], flush=True)
    (out/'results.json').write_text(json.dumps(results, indent=2)+'\n')


if __name__ == '__main__':
    main()

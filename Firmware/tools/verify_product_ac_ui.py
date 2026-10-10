#!/usr/bin/env python3
"""Render actual C PRODUCT-controller observations, including emergency fallback.

Snapshots are ephemeral native host structs exchanged only within one build.
No measurement equations, validity rules or PRODUCT state transitions live here.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

from resource_pack_format import build_pack


def check(controller, preview, directory):
    directory.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(controller), '--ac-snapshots', str(directory)], check=True)
    pack = directory / 'product.wrp2'
    pack.write_bytes(build_pack(Path(__file__).resolve().parents[1] / 'assets/resource_manifest.json'))
    snapshots = sorted(directory.glob('*.view'))
    if len(snapshots) != 19:
        raise ValueError(f'expected 19 controller snapshots, received {len(snapshots)}')
    count = 0
    for snapshot in snapshots:
        for language in ('en', 'pt-BR'):
            for source, suffix in ((str(pack), 'resources'), ('-', 'fallback')):
                output = directory / f'{snapshot.stem}-{language}-{suffix}.ppm'
                result = subprocess.run([str(preview), source, language, '@' + str(snapshot), str(output)],
                                        capture_output=True, text=True)
                if result.returncode:
                    raise RuntimeError(result.stdout + result.stderr)
                image = output.read_bytes()
                if not image.startswith(b'P6\n240 320\n255\n') or len(image) != 15 + 240 * 320 * 3:
                    raise ValueError(f'invalid rendered image: {output}')
                count += 1
    for source in (str(pack), '-'):
        for scenario in ('result-updated', 'result-refresh'):
            subprocess.run([str(preview), source, 'en', scenario, str(directory / (scenario + '.ppm'))],
                           check=True, stdout=subprocess.DEVNULL)
        if (directory / 'result-updated.ppm').read_bytes() != (directory / 'result-refresh.ppm').read_bytes():
            raise ValueError('same-page refresh leaves stale pixels')
    print(f'SYNTHETIC: {len(snapshots)} real controller views; {count} bilingual/fallback renders; '
          'refresh matches fresh draw. REQUIRES_BENCH_VALIDATION')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--controller', type=Path, required=True)
    parser.add_argument('--preview', type=Path, required=True)
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    if args.out:
        check(args.controller.resolve(), args.preview.resolve(), args.out.resolve())
    else:
        with tempfile.TemporaryDirectory(prefix='wtk-ac-ui-') as temporary:
            check(args.controller.resolve(), args.preview.resolve(), Path(temporary))


if __name__ == '__main__':
    main()

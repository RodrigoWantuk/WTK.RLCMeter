"""CTest driver using the actual host-built C solver, codec and store."""
import argparse
import copy
import pathlib
import re
import struct
import subprocess
import tempfile

from pc_osl import build_candidate, c_solver_input
from pc_osl_synthetic import Fixture, campaign
from pc_osl_provision import transfer_frames


def run(bridge, *args):
    return subprocess.run([str(bridge), *map(str, args)], check=True, capture_output=True, text=True).stdout


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--bridge', required=True, type=pathlib.Path)
    bridge = parser.parse_args().bridge
    maximum = 0.0
    with tempfile.TemporaryDirectory() as directory:
        root = pathlib.Path(directory)
        source, output = root/'input.bin', root/'output.bin'
        documents = [campaign(fixture) for fixture in
                     (Fixture(ideal=True), Fixture(), Fixture(noise_v=0.0002, dc_offset_v=0.025))]
        # Normalized HG mathematical vectors, independent of board headroom qualification.
        hg_doc = copy.deepcopy(documents[0])
        for capture in hg_doc['captures']:
            t = {'OPEN': 0.05, 'SHORT': 0.001, 'LOAD': 0.02}[capture['standard']]
            vs = complex(*capture['phasors']['vexc_1'])
            capture['phasors']['ret_1x'] = [vs.real*t, vs.imag*t]
            raw = vs*t*complex(14.8, 0.2)
            capture['phasors']['ret_hg'] = [raw.real, raw.imag]
            capture['quality'] = {'ret_1x_valid': capture['standard'] != 'OPEN',
                                  'ret_hg_valid': True, 'hg_observed': capture['standard'] == 'LOAD'}
        documents.append(hg_doc)
        for doc in documents:
            candidate, _ = build_candidate(doc, 7)
            source.write_bytes(c_solver_input(doc, 7))
            run(bridge, 'solve', source, output)
            c_frame = output.read_bytes()
            assert len(c_frame) == len(candidate)
            for i in range(33):
                offset = 120+i*80+22
                pc = struct.unpack_from('<12f', candidate, offset)
                c = struct.unpack_from('<12f', c_frame, offset)
                for j in range(0, 12, 2):
                    a, b = complex(*pc[j:j+2]), complex(*c[j:j+2])
                    error = abs(a-b)/max(1.0, abs(a), abs(b))
                    maximum = max(maximum, error)
                    assert error < 2e-5, (i, a, b, error)
            source.write_bytes(candidate)
            run(bridge, 'roundtrip', source, output)
            assert output.read_bytes() == candidate
            for packet in transfer_frames(candidate):
                source.write_bytes(packet)
                run(bridge, 'wire', source, output)
                assert output.read_bytes() == packet
        doc = campaign(Fixture(ideal=True))
        old = build_candidate(doc, 1)[0]
        new = build_candidate(doc, 2)[0]
        source.write_bytes(new)
        slots, installed = root/'slots.bin', root/'installed.bin'
        slots.write_bytes(old+b'\xff'*(8192-len(old)))
        seen = set()
        for cut in range(45):
            result = run(bridge, 'store', source, slots, installed, cut)
            sequence = int(re.search(r'active_sequence=(\d+)', result)[1])
            assert sequence in (1, 2), result
            seen.add(sequence)
            if sequence == 2:
                assert installed.read_bytes()[4096:4096+len(new)] == new
        assert seen == {1, 2}
        damaged = bytearray(old+b'\xff'*(4096-len(old))+new+b'\xff'*(4096-len(new)))
        damaged[4096+200] ^= 1
        slots.write_bytes(damaged)
        assert 'active_sequence=1' in run(bridge, 'store', source, slots, installed, 0)
        corrupt = bytearray(new)
        corrupt[200] ^= 1
        source.write_bytes(corrupt)
        rejected = subprocess.run([str(bridge), 'store', str(source), str(slots), str(installed), '100'],
                                  capture_output=True)
        assert rejected.returncode == 2
        assert slots.read_bytes() == damaged
        source.write_bytes(new)
        slots.write_bytes(b'\xff'*8192)
        result = run(bridge, 'store', source, slots, installed, 100)
        assert 'active_sequence=2' in result  # Blank C store preserves a nonzero supplied sequence.
    print(f'132 condition fits (including 33 HG); 100 PLC1 vectors; 4 exact frame round trips; '
          f'45 C store interruption states + blank install; max normalized coefficient error={maximum:.9g}')


if __name__ == '__main__':
    main()

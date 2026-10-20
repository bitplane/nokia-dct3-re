"""Verify own NHM-9 upload execution stops at its explicit missing-code CALL."""

import argparse
from pathlib import Path
import re
import struct
import subprocess

try:
    from tools.extract_nsm3_verifier import extract, extract_program_fragment
except ModuleNotFoundError:
    from extract_nsm3_verifier import extract, extract_program_fragment


def check(image, output, trace, returncode):
    program = extract(image, '3350')
    extract_program_fragment(image, '3350')
    if struct.unpack_from('>HH', program, 0x1ae) != (0xf074, 0x861f):
        raise ValueError('own verifier does not CALL 861f at 0fd7')
    if returncode != 1:
        raise ValueError('native run did not terminate at a controlled boundary')
    if not re.search(r'release entry=0f00 words=247 prom_input=0006 clock=13000000 stage=verifier', trace):
        raise ValueError('missing own 247-word verifier release')
    if 'readonly_program_write address=ff87 data=0006 pc=0f12' not in trace:
        raise ValueError('missing immutable fragment probe')
    if 'unavailable_program address=861f pc=8620 stage=verifier' not in trace:
        raise ValueError('missing explicit unavailable-code boundary')
    if 'address=861f pc=8620 stage=verifier' not in output:
        raise ValueError('termination did not identify the same missing word')
    if any(marker in trace for marker in ('staged_dsp: publication', 'runtime_hle_handoff', 'loader2_verified')):
        raise ValueError('unexpected completion beyond missing resident code')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('rom_root', type=Path)
    parser.add_argument('run_dir', type=Path)
    args = parser.parse_args()
    binary, root, run = args.binary.resolve(), args.rom_root.resolve(), args.run_dir.resolve()
    try:
        if run.exists():
            raise ValueError('use a fresh run directory for initially erased storage')
        image = (root / 'noki3350/3350f530a.fls').read_bytes()
        extract(image, '3350')
        run.mkdir(parents=True)
        result = subprocess.run([
            str(binary), 'nhm9stage', '-rompath', str(root), '-noreadconfig',
            '-video', 'none', '-sound', 'none', '-nothrottle',
            '-seconds_to_run', '2', '-log'], cwd=run, capture_output=True,
            text=True, timeout=30)
        output = result.stdout + result.stderr
        (run / 'native_output.log').write_text(output)
        check(image, output, (run / 'error.log').read_text(), result.returncode)
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'NHM-9 native boundary failed: {error}\n')
    print('NHM-9 native boundary PASS: own verifier CALL 861f; resident code unavailable; no completion claimed')


if __name__ == '__main__':
    main()

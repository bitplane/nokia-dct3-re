"""Check own NHM-9 key tables and physical matrix controller conformance."""

import argparse
from pathlib import Path
import struct
import subprocess

try:
    from tools.extract_nsm3_verifier import extract
except ModuleNotFoundError:
    from extract_nsm3_verifier import extract


def check_tables(image):
    extract(image, '3350')  # Pin the whole own-image source before table reads.
    if image[0x25bd94:0x25bdad] != bytes.fromhex('3e1a0a18193e0f01020e053e0605043e3e0908073e030b170c'):
        raise ValueError('unexpected NHM-9 normal key table')
    if image[0x25bdb0:0x25bdb5] != bytes.fromhex('3e0d3e3e3e'):
        raise ValueError('unexpected NHM-9 Power key table')
    if struct.unpack_from('>3I', image, 0x1a956c) != (0x45bd94, 0x1244fe, 0x45bdb0):
        raise ValueError('unexpected NHM-9 decoder table pointers')


def check_trace(trace, returncode):
    if returncode or trace.count('nhm9_keypad: PASS matrix_keys=18 scans=90 power_mask=02 ui_delivery=unvalidated') != 1:
        raise ValueError('missing complete NHM-9 controller conformance')
    if '[LUA ERROR]' in trace or 'assertion failed' in trace:
        raise ValueError('NHM-9 Lua conformance failed')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('rom_root', type=Path)
    parser.add_argument('run_dir', type=Path)
    args = parser.parse_args()
    try:
        binary, root, run = args.binary.resolve(), args.rom_root.resolve(), args.run_dir.resolve()
        check_tables((root / 'noki3350/3350f530a.fls').read_bytes())
        if run.exists():
            raise ValueError('use a fresh run directory')
        run.mkdir(parents=True)
        result = subprocess.run([
            str(binary), 'noki3350', '-rompath', str(root), '-noreadconfig',
            '-video', 'none', '-sound', 'none', '-nothrottle', '-seconds_to_run', '1',
            '-log', '-autoboot_delay', '0', '-autoboot_script',
            str(Path(__file__).with_name('nhm9_keypad_conformance.lua').resolve())],
            cwd=run, capture_output=True, text=True, timeout=30)
        output = result.stdout + result.stderr
        (run / 'keypad_output.log').write_text(output)
        check_trace((run / 'error.log').read_text() + output, result.returncode)
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'NHM-9 keypad failed: {error}\n')
    print('NHM-9 keypad PASS: own tables, 18 contacts/90 scans and Power; UI delivery unvalidated')


if __name__ == '__main__':
    main()

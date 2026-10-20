"""Run own NSB-1 against explicitly unproved NSE-1 resident-ROM compatibility inputs."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

INPUTS = (
    ('noki5190/5190f671a.fls', '27b8f0cb8640e5b423960b206e9aee065f32d726'),
    ('noki5110/nse1_rom4_dsp_program.bin', 'a05a1e96a8c36ec5a47e1ea059d15afa54ca5739'),
    ('noki5110/nse1_rom4_dsp_data.bin', '024c7f970f4ef754d3e90471de48a167515f930d'),
)


def checksum_control(source):
    """Own decoded integrity fields only; never claim erased contents are calibrated."""
    if len(source) != 8192:
        raise ValueError('checksum control requires own 8 KiB external storage')
    result = bytearray(source)
    security_sum = sum(source[:0x11c]) & 0xffff
    calibration_sum = (sum(source[0x120:0x1fe]) - source[0x154] - source[0x155]) & 0xffff
    struct.pack_into('>I', result, 0x11c, security_sum)
    struct.pack_into('>H', result, 0x1fe, calibration_sum)
    return bytes(result)


def check_checksum_control(trace):
    flags = re.findall(r'nsb1_presentation: t=([0-9.]+) selftest_flags=([0-9a-f]{2}) observer_writes=0', trace)
    if len(flags) != 5 or [value for _, value in flags] != ['00', 'c0', '40', '40', '40']:
        raise ValueError('checksum control did not retain the observed self-test bit 40')
    if re.search(r'nsb1_selftest_flags: value=[0-9a-f]{2} pc=0023a018 ', trace):
        raise ValueError('checksum-failure branch still executed in checksum control')


def check_trace(trace, program3, program4, returncode):
    if returncode or trace.count('Soft reset') != 1 or '[LUA ERROR]' in trace:
        raise ValueError('native comparison reset, crashed or failed observation')
    programs = re.findall(r'nsb1_release_program: release=(\d+) words=186 data=([0-9a-f]+) observer_writes=0', trace)
    if len(program3) != 372 or len(program4) != 372 or programs != [('1', program3.hex()), ('2', program4.hex())]:
        raise ValueError('ROM3/ROM4 retries did not execute the two own uploads')
    releases = re.findall(r'nsb1_dsp_release: command=01 arguments=([0-9a-f]+) pc=([0-9a-f]+)', trace)
    if releases != [('0100030000008000000100010200', '0028f9c8')] * 2 + [
            ('fd00ff8002440500007800000000', '0028fc50')]:
        raise ValueError('unexpected verifier/loader release lifecycle')
    reads = re.findall(r'nsb1_dsp_result_read: address=00010004 data=([0-9a-f]{8}) mask=([0-9a-f]{8}) pc=([0-9a-f]{8})', trace)
    expected = [('00040000', 'ffff0000', '0028f9dc'),
                ('00040000', 'ffff0000', '0028fa08'),
                ('00000003', '0000ffff', '0028fa0c'),
                ('00040000', 'ffff0000', '0028f9dc'),
                ('00040000', 'ffff0000', '0028fa08'),
                ('00000004', '0000ffff', '0028fa0c')]
    if reads[:6] != expected:
        raise ValueError('missing resident-version mismatch then organic ROM4 acceptance')
    completions = re.findall(r'nsb1_dsp_result_read: address=00010000 data=([0-9a-f]{8}) mask=0000ffff pc=0028fa96', trace)
    if completions != ['0000ffff', '00000004']:
        raise ValueError('missing native final-result transition from pending to version 4')
    states = re.findall(r'nsb1_dsp_state: t=([0-9.]+) .*?init_success=01 saved_version=0004 handoffs=63 releases=3 observer_writes=0', trace)
    if len(states) != 4 or any(abs(float(actual) - wanted) > 0.02 for actual, wanted in zip(states, (0.5, 1, 2, 5))):
        raise ValueError('missing completed 63-handoff native continuation observations')
    for marker in ('rom4_port_write: port=02 data=ffff pc=0d8f',
                   'rom4_port_write: port=36 data=0080 pc=8d98'):
        if marker not in trace:
            raise ValueError('missing native loader/resident execution evidence')


def check_lcd(trace, snapshots):
    from PIL import Image

    commands = re.findall(r'nsb1_lcd_command: byte=([0-9a-f]{2}) pc=', trace)
    expected = '24 40 80 41 80 42 80 43 80 44 80 45 80 20 21 05 14 ce 20 0c'.split()
    if commands[:20] != expected:
        raise ValueError('missing own six-bank LCD initialization')
    summaries = re.findall(r'nsb1_lcd_summary: t=([0-9.]+) commands=(\d+) data_bytes=(\d+) nonzero=(\d+) observer_writes=0', trace)
    counts = [(int(commands), int(data), int(nonzero)) for _, commands, data, nonzero in summaries]
    if counts != [(0, 0, 0), (32, 1008, 504), (32, 1008, 504), (68, 2016, 504), (68, 2016, 504)]:
        raise ValueError('LCD traffic does not reproduce clear/fill/clear self-test')
    for name, value in (('nsb1_native_1s.png', 0), ('nsb1_native_5s.png', 255)):
        with Image.open(snapshots / name) as source:
            image = source.convert('RGB')
            if image.size != (84, 48) or image.getextrema() != ((value, value),) * 3:
                raise ValueError(f'LCD self-test pixels mismatch: {name}')


def prepare(root, run, checksum_from=None):
    if run.exists():
        raise ValueError('use a fresh comparison directory; no prior EEPROM is admitted')
    qualified = []
    for name, expected in INPUTS:
        data = (root / name).read_bytes()
        if hashlib.sha1(data).hexdigest() != expected:
            raise ValueError(f'unqualified comparison input: {name}')
        qualified.append((name, data, expected))
    fixture = None
    if checksum_from:
        previous = json.loads((checksum_from / 'input_manifest.json').read_text())
        if (previous.get('inputs') != [{'source': name, 'sha1': expected} for name, expected in INPUTS]
                or previous.get('external_eeprom') != 'erased 8 KiB; no donor storage'):
            raise ValueError('checksum source must be a qualified own erased-storage comparison')
        if hashlib.sha1((checksum_from / 'nvram/nsb1r4t/flash').read_bytes()).hexdigest() != INPUTS[0][1]:
            raise ValueError('checksum source executable differs')
        original = (checksum_from / 'nvram/nsb1r4t/eeprom').read_bytes()
        fixture = checksum_control(original)
    target = run / 'inputs/nsb1r4t'
    target.mkdir(parents=True)
    for name, data, expected in qualified:
        (target / Path(name).name).write_bytes(data)
    manifest = {'inputs': [{'source': name, 'sha1': expected} for name, _, expected in qualified],
                'external_eeprom': 'erased 8 KiB; no donor storage',
                'resident_mask': 'NSE-1 compatibility candidate; NSB-1 fitted identity unproved',
                'dsp_clock_hz': 52000000, 'physical_dsp_clock_proved': False,
                'boot_claim': False}
    if fixture is not None:
        storage = run / 'nvram/nsb1r4t'
        storage.mkdir(parents=True)
        (storage / 'eeprom').write_bytes(fixture)
        manifest['external_eeprom'] = 'diagnostic checksum-only control; calibration/identity not validated'
        manifest['checksum_source_run'] = str(checksum_from.resolve())
        manifest['checksum_source_sha1'] = hashlib.sha1(original).hexdigest()
        manifest['checksum_fixture_sha1'] = hashlib.sha1(fixture).hexdigest()
        manifest['changed_offsets'] = [hex(i) for i, (a, b) in enumerate(zip(original, fixture)) if a != b]
    (run / 'input_manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return run / 'inputs'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('rom_root', type=Path)
    parser.add_argument('run_dir', type=Path)
    parser.add_argument('--seconds', type=int, default=6, choices=range(1, 61))
    parser.add_argument('--verbose', action='store_true', help='include the larger native opcode inventory')
    parser.add_argument('--checksum-control-from', type=Path,
                        help='diagnostic only: recompute six checksum bytes from a qualified own erased-storage run')
    args = parser.parse_args()
    try:
        binary, root, run = args.binary.resolve(), args.rom_root.resolve(), args.run_dir.resolve()
        rompath = prepare(root, run, args.checksum_control_from)
        with (run / 'mame_output.log').open('w') as output:
            result = subprocess.run([
                str(binary), 'nsb1r4t', '-rompath', str(rompath), '-noreadconfig',
                '-video', 'none', '-sound', 'none', '-nothrottle', '-log',
                *(['-verbose'] if args.verbose else []),
                '-seconds_to_run', str(args.seconds), '-autoboot_delay', '0',
                '-autoboot_script', str(Path(__file__).with_name('nsb1_boot_observe.lua').resolve())],
                cwd=run, stdout=output, stderr=subprocess.STDOUT, timeout=180)
        (run / 'exit_status.json').write_text(json.dumps({'returncode': result.returncode}) + '\n')
        if args.seconds >= 6:
            image = (rompath / 'nsb1r4t/5190f671a.fls').read_bytes()
            check_trace((run / 'error.log').read_text(), image[0xb9350:0xb9350 + 372],
                        image[0xbe570:0xbe570 + 372], result.returncode)
            if args.checksum_control_from is None:
                check_lcd((run / 'error.log').read_text(), run / 'snap')
            else:
                check_checksum_control((run / 'error.log').read_text())
            if (run / 'nvram/nsb1r4t/flash').read_bytes() != image:
                raise ValueError('own executable flash changed')
            if len((run / 'nvram/nsb1r4t/eeprom').read_bytes()) != 8192:
                raise ValueError('external EEPROM capacity changed')
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'NSB-1 comparison failed: {error}\n')
    status = 'native continuation PASS' if args.seconds >= 6 else f'run recorded: returncode={result.returncode}'
    qualifier = 'checksum diagnostic; presentation not accepted; ' if args.checksum_control_from else ''
    print(f'NSB-1 compatibility {status}; {qualifier}fitted mask/phone boot not promoted; evidence={run}')


if __name__ == '__main__':
    main()

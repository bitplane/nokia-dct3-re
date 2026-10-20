"""Run own NSB-1 against explicitly unproved NSE-1 resident-ROM compatibility inputs."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

INPUTS = (
    ('noki5190/5190f671a.fls', '27b8f0cb8640e5b423960b206e9aee065f32d726'),
    ('noki5110/nse1_rom4_dsp_program.bin', 'a05a1e96a8c36ec5a47e1ea059d15afa54ca5739'),
    ('noki5110/nse1_rom4_dsp_data.bin', '024c7f970f4ef754d3e90471de48a167515f930d'),
)


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


def prepare(root, run):
    if run.exists():
        raise ValueError('use a fresh comparison directory; no prior EEPROM is admitted')
    qualified = []
    for name, expected in INPUTS:
        data = (root / name).read_bytes()
        if hashlib.sha1(data).hexdigest() != expected:
            raise ValueError(f'unqualified comparison input: {name}')
        qualified.append((name, data, expected))
    target = run / 'inputs/nsb1r4t'
    target.mkdir(parents=True)
    for name, data, expected in qualified:
        (target / Path(name).name).write_bytes(data)
    manifest = {'inputs': [{'source': name, 'sha1': expected} for name, _, expected in qualified],
                'external_eeprom': 'erased 8 KiB; no donor storage',
                'resident_mask': 'NSE-1 compatibility candidate; NSB-1 fitted identity unproved',
                'dsp_clock_hz': 52000000, 'physical_dsp_clock_proved': False,
                'boot_claim': False}
    (run / 'input_manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return run / 'inputs'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('rom_root', type=Path)
    parser.add_argument('run_dir', type=Path)
    parser.add_argument('--seconds', type=int, default=6, choices=range(1, 61))
    parser.add_argument('--verbose', action='store_true', help='include the larger native opcode inventory')
    args = parser.parse_args()
    try:
        binary, root, run = args.binary.resolve(), args.rom_root.resolve(), args.run_dir.resolve()
        rompath = prepare(root, run)
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
            if (run / 'nvram/nsb1r4t/flash').read_bytes() != image:
                raise ValueError('own executable flash changed')
            if len((run / 'nvram/nsb1r4t/eeprom').read_bytes()) != 8192:
                raise ValueError('external EEPROM capacity changed')
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'NSB-1 comparison failed: {error}\n')
    status = 'native continuation PASS' if args.seconds >= 6 else f'run recorded: returncode={result.returncode}'
    print(f'NSB-1 compatibility {status}; fitted mask/phone boot not promoted; evidence={run}')


if __name__ == '__main__':
    main()

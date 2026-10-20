"""Bounded own NSK-1 / NSE-1 ROM4 compatibility observation, not fitted-mask proof."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsk1_static_check import inspect


INPUTS = (
    ('noki5130/5130f561a.fls', 'a2a99ec6c52104eebcbae281cf8e845bdc8ea1d9'),
    ('noki5130/5130f561a-2915.fls', '64068df6b7950b47f34ac141d0ef9c51dc666cca'),
    ('noki5110/nse1_rom4_dsp_program.bin', 'a05a1e96a8c36ec5a47e1ea059d15afa54ca5739'),
    ('noki5110/nse1_rom4_dsp_data.bin', '024c7f970f4ef754d3e90471de48a167515f930d'),
)


def prepare(root, run):
    if run.exists():
        raise ValueError('NSK-1 observation requires a fresh run directory')
    sources = [(root / name, digest) for name, digest in INPUTS]
    for source, digest in sources:
        if hashlib.sha1(source.read_bytes()).hexdigest() != digest:
            raise ValueError(f'NSK-1 compatibility input hash differs: {source.name}')
    for index, variant in enumerate(('standard', '2915')):
        inspect(sources[index][0].read_bytes(), variant)
    target = run / 'inputs/nsk1r4t'
    target.mkdir(parents=True)
    for source, _ in sources:
        shutil.copyfile(source, target / source.name)
    (run / 'input_manifest.json').write_text(json.dumps({
        'inputs': [{'source': name, 'sha1': digest} for name, digest in INPUTS],
        'external_eeprom': 'own erased 2048 bytes; no donor provisioning',
        'resident_mask_and_52mhz_clock': 'NSE-1 compatibility inputs, unproved on NSK-1',
        'peer_completions': False, 'physical_keys': False,
    }, indent=2)+'\n')


def summarize(trace, image, variant):
    samples = re.findall(r'nsk1_boot: t=([0-9.]+) pc=([0-9a-f]+).*illegal=([01])', trace)
    if '[LUA ERROR]' in trace or len(samples) != 5 or float(samples[-1][0]) < 5:
        raise ValueError('missing complete passive NSK-1 observation')
    releases = re.findall(r'nsk1_release: index=(\d+) command=01 pc=([0-9a-f]+) args=([0-9a-f]+) program=([0-9a-f]+)', trace)
    descriptor = 0x29c2e4 if variant == 'standard' else 0x29d044
    first_payload = image[descriptor-0x200000+12:descriptor-0x200000+12+372].hex()
    first_pc = '00273504' if variant == 'standard' else '002734f4'
    if not releases or releases[0] != ('1', first_pc, '0100030000008000000100010200', first_payload):
        raise ValueError('first DSP release does not contain own slot-25 payload/arguments')
    for index, _, arguments, _ in releases[1:]:
        slot0 = 0x29728c if variant == 'standard' else 0x29727c
        expected = image[slot0-0x200000:slot0-0x200000+10].hex()
        if arguments[:20] != expected:
            raise ValueError(f'release {index} is not own descriptor-0 transfer')
    return {'variant': variant, 'samples': len(samples),
            'last_mcu_pc': samples[-1][1],
            'native_illegal_observed': any(row[2] != '0' for row in samples),
            'releases': len(releases), 'own_slot25_payload_observed': True,
            'descriptor0_release_observed': len(releases) > 1,
            'graphical_or_fitted_hardware_proved': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('roms', type=Path)
    parser.add_argument('run', type=Path)
    parser.add_argument('--variant', choices=('standard', '2915'), default='standard')
    args = parser.parse_args()
    run = args.run.resolve()
    try:
        prepare(args.roms.resolve(), run)
        script = Path(__file__).with_name('nsk1_boot_observe.lua').resolve()
        with (run / 'mame_output.log').open('w') as output:
            result = subprocess.run([
                str(args.binary.resolve()), 'nsk1r4t', '-bios',
                '561a' if args.variant == 'standard' else '561a2915',
                '-rompath', str(run / 'inputs'), '-noreadconfig', '-video', 'none',
                '-sound', 'none', '-nothrottle', '-log', '-seconds_to_run', '6',
                '-autoboot_delay', '0', '-autoboot_script', str(script),
            ], cwd=run, stdout=output, stderr=subprocess.STDOUT, timeout=180)
        (run / 'exit_status.json').write_text(json.dumps({'returncode': result.returncode})+'\n')
        if result.returncode:
            raise ValueError(f'MAME exited {result.returncode}; inspect mame_output.log')
        trace = (run / 'error.log').read_text()
        storage = run / 'nvram' / ('nsk1r4t' if args.variant == 'standard' else 'nsk1r4t_1')
        image_index = 0 if args.variant == 'standard' else 1
        image = (run/'inputs/nsk1r4t'/Path(INPUTS[image_index][0]).name).read_bytes()
        if hashlib.sha1((storage/'flash').read_bytes()).hexdigest() != INPUTS[image_index][1]:
            raise ValueError('own MCU/PPM changed during observation')
        if len((storage/'eeprom').read_bytes()) != 2048:
            raise ValueError('own EEPROM storage capacity changed')
        summary = summarize(trace,image,args.variant)
        summary['unchanged_flash'] = True
        (run / 'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
        print(json.dumps(summary, indent=2))
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'NSK-1 observation failed: {error}\n')


if __name__ == '__main__':
    main()

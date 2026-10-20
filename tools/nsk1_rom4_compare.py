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
from tools.nsk1_record_exchange_check import check as check_records
from tools.nsk1_record_profile import build_from_capture,check_observation


INPUTS = (
    ('noki5130/5130f561a.fls', 'a2a99ec6c52104eebcbae281cf8e845bdc8ea1d9'),
    ('noki5130/5130f561a-2915.fls', '64068df6b7950b47f34ac141d0ef9c51dc666cca'),
    ('noki5110/nse1_rom4_dsp_program.bin', 'a05a1e96a8c36ec5a47e1ea059d15afa54ca5739'),
    ('noki5110/nse1_rom4_dsp_data.bin', '024c7f970f4ef754d3e90471de48a167515f930d'),
)


def prepare(root, run, record_profile_from=None):
    if run.exists():
        raise ValueError('NSK-1 observation requires a fresh run directory')
    sources = [(root / name, digest) for name, digest in INPUTS]
    for source, digest in sources:
        if hashlib.sha1(source.read_bytes()).hexdigest() != digest:
            raise ValueError(f'NSK-1 compatibility input hash differs: {source.name}')
    for index, variant in enumerate(('standard', '2915')):
        inspect(sources[index][0].read_bytes(), variant)
    initial_eeprom,profile_metadata = bytes([255])*2048,None
    if record_profile_from is not None:
        initial_eeprom,profile_metadata = build_from_capture(record_profile_from,INPUTS)
    target = run / 'inputs/nsk1r4t'
    target.mkdir(parents=True)
    for source, _ in sources:
        shutil.copyfile(source, target / source.name)
    if profile_metadata is not None:
        storage = run/'nvram/nsk1r4t'
        storage.mkdir(parents=True)
        (storage/'eeprom').write_bytes(initial_eeprom)
    (run / 'input_manifest.json').write_text(json.dumps({
        'inputs': [{'source': name, 'sha1': digest} for name, digest in INPUTS],
        'external_eeprom': ('diagnostic published lock-record example; identity/calibration erased'
                           if profile_metadata else 'own erased 2048 bytes; no donor provisioning'),
        'record_profile':profile_metadata,
        'resident_mask_and_52mhz_clock': 'NSE-1 compatibility inputs, unproved on NSK-1',
        'peer_completions': False, 'physical_keys': False,
    }, indent=2)+'\n')
    return initial_eeprom


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
    presentation = re.findall(r'nsk1_presentation: t=([0-9.]+) commands=(\d+) data=(\d+) nonzero=(\d+) startup_reason=([0-9a-f]{2}) idle_flag=([0-9a-f]{2}) observer_writes=0', trace)
    if len(presentation) != 5 or [row[0] for row in presentation] != [row[0] for row in samples]:
        raise ValueError('missing synchronized LCD/startup observations')
    last = presentation[-1]
    decisions = re.findall(r'nsk1_record_decision: pointer=([0-9a-f]{8}) state=([0-9a-f]{8}) valid=([0-9a-f]{8}) observer_writes=0', trace)
    accepted = re.findall(r'nsk1_record_accept: context=([0-9a-f]{8}) valid=00000001 copied=([0-9a-f]{48}) observer_writes=0',trace)
    late = re.findall(r'nsk1_late: t=([0-9.]+) pc=([0-9a-f]{8}) dsp_pc=([0-9a-f]{4}) illegal=([01]) startup_reason=([0-9a-f]{2})',trace)
    return {'variant': variant, 'samples': len(samples),
            'last_mcu_pc': samples[-1][1],
            'native_illegal_observed': any(row[2] != '0' for row in samples) or any(row[3] != '0' for row in late),
            'releases': len(releases), 'own_slot25_payload_observed': True,
            'descriptor0_release_observed': len(releases) > 1,
            'lcd_commands': int(last[1]), 'lcd_data_bytes': int(last[2]),
            'lcd_nonzero_bytes': int(last[3]),
            'startup_reasons': [row[4] for row in presentation],
            'record_decisions': [{'pointer': pointer, 'state': int(state,16), 'valid': int(valid,16)}
                                 for pointer,state,valid in decisions],
            'record_accepts':[{'context':context,'copied':copied} for context,copied in accepted],
            'late_samples':[{'time':float(time),'mcu_pc':pc,'dsp_pc':dsp_pc,'startup_reason':reason}
                            for time,pc,dsp_pc,_,reason in late],
            'graphical_or_fitted_hardware_proved': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('roms', type=Path)
    parser.add_argument('run', type=Path)
    parser.add_argument('--variant', choices=('standard', '2915'), default='standard')
    parser.add_argument('--debug-startup', action='store_true', help='bounded read-only own startup entry traces')
    parser.add_argument('--record-profile-from',type=Path,help='diagnostic external record example keyed only from a qualified own erased capture')
    parser.add_argument('--seconds',type=int,choices=range(6,62),default=6)
    args = parser.parse_args()
    run = args.run.resolve()
    if args.record_profile_from and args.variant != 'standard':
        parser.error('record-profile experiment is not established for the 2915 family')
    if args.record_profile_from and not args.debug_startup:
        parser.error('record-profile experiment requires --debug-startup for own accepted-copy evidence')
    try:
        initial_eeprom = prepare(args.roms.resolve(), run,args.record_profile_from)
        script = Path(__file__).with_name('nsk1_boot_observe.lua').resolve()
        with (run / 'mame_output.log').open('w') as output:
            result = subprocess.run([
                str(args.binary.resolve()), 'nsk1r4t', '-bios',
                '561a' if args.variant == 'standard' else '561a2915',
                '-rompath', str(run / 'inputs'), '-noreadconfig', '-video', 'none',
                '-sound', 'none', '-nothrottle', '-log', '-seconds_to_run', str(args.seconds),
                *(['-debug', '-debugger', 'none'] if args.debug_startup else []),
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
        required_late = [time for time in (10,20,30,50,59) if time <= args.seconds-1]
        allowed_late = [time for time in (10,20,30,50,59) if time <= args.seconds]
        if not len(required_late) <= len(summary['late_samples']) <= len(allowed_late) or any(
                not expected <= sample['time'] < expected+0.1
                for expected,sample in zip(allowed_late,summary['late_samples'])):
            raise ValueError('missing or mistimed late native observation')
        summary['unchanged_flash'] = True
        summary['record_arithmetic'] = (check_records(trace,minimum=1 if args.record_profile_from else 2,initial_eeprom=initial_eeprom)
                                        if args.variant == 'standard' else {'comparison_performed':False,'reason':'2915 record lifecycle not qualified'})
        if args.record_profile_from:
            check_observation(summary,initial_eeprom,(storage/'eeprom').read_bytes())
            summary['synthetic_record_conformance'] = True
        (run / 'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
        print(json.dumps(summary, indent=2))
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'NSK-1 observation failed: {error}\n')


if __name__ == '__main__':
    main()

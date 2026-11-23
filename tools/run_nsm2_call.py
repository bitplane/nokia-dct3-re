#!/usr/bin/env python3
"""The native 8850's lab call: registration, incoming call, speech and clearing.

nsm2ap5 runs the 8850's own MCU and DSP uploads on the acquired 8210 ROM6 with
the 6210's laboratory cell (direct ADC, untuned). With the synthetic record
fixture (make_nsm2_record_profile.py) it must register, ring with the caller's
number, answer with Send, carry lab tones both ways on TCH/F and clear with
End. --acquired-records is the negative control: the donor's records fail the
DSP's own validation and the handset cannot register.
"""
import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
if __package__ in (None, ''):
    sys.path.insert(0, str(ROOT))
MODEL = 'nsm2ap5'
CALLER = '5551234'
FRAMES = {  # reviewed LCD pixels
    'nsm2_idle.png': 'fd9895b3ed26863e51addaf05d56293ff78d752fcaadfb7399e40879c5f72887',
    'nsm2_ringing.png': '5e45fce4598de194cdc1a2a5ceb3feb728bec391040a6ad52b28839f56213389',
}


def prepare(base, provision=True):
    from tools.make_nsm2_record_profile import make_profile
    flash = (ROOT / 'roms/noki8850/8850v531.fls').read_bytes()
    pmm = (ROOT / 'roms/noki8850/8850 virgin eeprom 003d0000.fls').read_bytes()
    profile = make_profile(flash, pmm, (ROOT / 'roms/nsm3r6/nsm3_rom6_data.be16').read_bytes())
    (base / 'nvram' / MODEL).mkdir(parents=True)
    (base / 'nvram' / MODEL / 'flash').write_bytes(flash.ljust(0x1d0000, b'\xff') + (profile if provision else pmm))
    (base / 'roms').mkdir()
    (base / 'roms' / MODEL).symlink_to(ROOT / 'roms/nsm3r6', target_is_directory=True)


def check_registration(text):
    """Own SABM/LU, its UA and Accept, the RR acknowledgement and release, in order."""
    at = -1
    for pattern in (r'npe3_raw_agch: ', r'npe3_bts_uplink: start_frame=\d+ octets=013f490508',
                    r'npe3_raw_ua: ', r'npe3_raw_lu_accept: ', r'npe3_bts_uplink: start_frame=\d+ octets=032101',
                    r'npe3_raw_release: ', r'npe3_raw_disc_ua: ', r'npe3_bts_page_access: ',
                    r'npe3_raw_call_setup: ', r'npe3_raw_assignment: '):
        match = re.compile(pattern).search(text, at + 1)
        if not match:
            raise ValueError(f'native 8850 exchange is missing or out of order: {pattern}')
        at = match.start()


def check_inputs(text):
    keys = re.findall(r'nsm2_call_input: key=(.+) t=([\d.]+)', text)
    expected = ['Keypad 1', 'Keypad 2', 'Keypad 3', 'Keypad 4', 'Keypad 5', 'Menu',
                'Laboratory BTS page', 'Call / Send', 'End']
    if [key for key, _ in keys] != expected:
        raise ValueError('native 8850 call inputs did not complete')


def main():
    from tools.rom4_lab_registration_check import check_traffic_call
    from tools.run_npe3_native import configure_cell_power
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--mame', type=Path, default=ROOT / 'mame/mame')
    parser.add_argument('--acquired-records', action='store_true', help='negative control: keep the donor records')
    parser.add_argument('--keep', action='store_true')
    args = parser.parse_args()
    args.mame = args.mame.resolve()  # MAME runs from inside the run directory
    (ROOT / 'tmp').mkdir(exist_ok=True)
    base = Path(tempfile.mkdtemp(prefix='nsm2-call.', dir=ROOT / 'tmp'))
    try:
        prepare(base, not args.acquired_records)
        run = base / 'run'
        run.mkdir()
        configure_cell_power(run, MODEL, 'handset33', incoming_call=True, traffic_assignment=True,
                             call_ack=True, clear_call=True, sacch=True, speech=True)
        command = [str(args.mame), MODEL, '-rompath', f'{base / "roms"};{ROOT / "roms"}',
                   '-nvram_directory', str(base / 'nvram'), '-cfg_directory', 'cfg',
                   '-snapshot_directory', 'snap', '-noreadconfig', '-video', 'none', '-sound', 'none',
                   '-nothrottle', '-log', '-autoboot_delay', '0',
                   '-autoboot_script', str(ROOT / 'tools/nsm2_call.lua'),
                   '-seconds_to_run', '64', '-nonvram_save']
        environment = {k: v for k, v in os.environ.items() if not k.startswith('NOKIA_DCT3_')}
        with (run / 'console.log').open('w') as output:
            subprocess.run(command, cwd=run, env=environment, stdout=output, stderr=subprocess.STDOUT,
                           check=True, timeout=1800)
        text = (run / 'error.log').read_text(errors='replace')
        if '[LUA ERROR]' in (run / 'console.log').read_text(errors='replace') or 'LUA ERROR' in text:
            raise ValueError('call script failed')
        check_inputs(text)
        check_registration(text)
        result = check_traffic_call(text)
        from PIL import Image
        for name, digest in FRAMES.items():
            with Image.open(run / 'snap' / name) as image:
                actual = hashlib.sha256(image.convert('L').tobytes()).hexdigest()
            if actual != digest:
                raise ValueError(f'{name} differs from its reviewed frame: {actual}')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'Native 8850 lab call failed: {error}; evidence in {base}\n')
    print(' '.join(f'{key}={value}' for key, value in result.items()))
    print('Native 8850 lab call: PASS registration, ringing, Answer, speech both ways and End')
    if args.keep:
        print(f'evidence retained: {base}')
    else:
        shutil.rmtree(base)


if __name__ == '__main__':
    main()

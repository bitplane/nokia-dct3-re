#!/usr/bin/env python3
"""Native lab calls on the products trialled with the acquired 8210 ROM6.

Each machine runs its own MCU and DSP uploads with the 6210's laboratory cell
(direct ADC, untuned). With the synthetic record fixture
(make_rom6_record_profile.py) it must register, ring with the caller's number,
answer with Send, carry lab tones both ways on TCH/F and clear with End.
--acquired-records is the negative control: the donor's records fail the
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
from tools.make_rom6_record_profile import PRODUCTS, make_profile  # noqa: E402

CALLER = '5551234'
MODELS = {  # page time, security code, Send field and reviewed LCD pixels
    'nsm2': dict(name='8850', page=44, code=True, send='Call / Send', frames={
        'idle.png': '08737c08772c99df2b41fc8560587820c2c209f5f74c12893a657bd6f623e555',
        'ringing.png': '5e45fce4598de194cdc1a2a5ceb3feb728bec391040a6ad52b28839f56213389'}),
    'nhm3': dict(name='6250', page=24, code=False, send='Send', frames={
        'idle.png': 'f130f1115e9561b5786c95fc6e8a47ff44b57156640b4b88a0510b170c7efa66',
        'ringing.png': '7d5d7b6fc9f9562faf52cfa1a7f3b9bfcaf56b1611d0fba626e976174225535d'}),
    'nsb6': dict(name='8890', page=44, code=True, send='Call / Send', frames={
        'idle.png': 'f3ee5f33a5efdb8212916964122cd2d0c0ce39ee3feae569ccb409ca92ab5106',
        'ringing.png': '0066a696ab3d221b3926bfb91ba6827adef9e2655eb6b2eca89268dd5ae58190'}),
}


def prepare(base, key, provision=True):
    product = PRODUCTS[key]
    flash = (ROOT / 'roms' / product.mcu).read_bytes()
    pmm = (ROOT / 'roms' / product.pmm).read_bytes()
    profile = make_profile(product, flash, pmm, (ROOT / 'roms/nsm3r6/nsm3_rom6_data.be16').read_bytes())
    machine = f'{key}ap5'
    (base / 'nvram' / machine).mkdir(parents=True)
    (base / 'nvram' / machine / 'flash').write_bytes(
        flash.ljust(product.flash, b'\xff') + (profile if provision else pmm))
    (base / 'roms').mkdir()
    (base / 'roms' / machine).symlink_to(ROOT / 'roms/nsm3r6', target_is_directory=True)
    return machine


def check_registration(text):
    """Own SABM/LU, its UA and Accept, the RR acknowledgement and release, in order."""
    at = -1
    for pattern in (r'npe3_raw_agch: ', r'npe3_bts_uplink: start_frame=\d+ octets=013f490508',
                    r'npe3_raw_ua: ', r'npe3_raw_lu_accept: ', r'npe3_bts_uplink: start_frame=\d+ octets=032101',
                    r'npe3_raw_release: ', r'npe3_raw_disc_ua: ', r'npe3_bts_page_access: ',
                    r'npe3_raw_call_setup: ', r'npe3_raw_assignment: '):
        match = re.compile(pattern).search(text, at + 1)
        if not match:
            raise ValueError(f'native exchange is missing or out of order: {pattern}')
        at = match.start()


def check_inputs(text, model):
    keys = re.findall(r'rom6_call_input: key=(.+) t=[\d.]+', text)
    code = ['Keypad 1', 'Keypad 2', 'Keypad 3', 'Keypad 4', 'Keypad 5', 'Menu'] if model['code'] else []
    if keys != code + ['Laboratory BTS page', model['send'], 'End']:
        raise ValueError('call inputs did not complete')


def main():
    from tools.rom4_lab_registration_check import check_traffic_call
    from tools.run_npe3_native import configure_cell_power
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('model', choices=MODELS)
    parser.add_argument('--mame', type=Path, default=ROOT / 'mame/mame')
    parser.add_argument('--acquired-records', action='store_true', help='negative control: keep the donor records')
    parser.add_argument('--keep', action='store_true')
    args = parser.parse_args()
    model = MODELS[args.model]
    args.mame = args.mame.resolve()  # MAME runs from inside the run directory
    (ROOT / 'tmp').mkdir(exist_ok=True)
    base = Path(tempfile.mkdtemp(prefix=f'{args.model}-call.', dir=ROOT / 'tmp'))
    try:
        machine = prepare(base, args.model, not args.acquired_records)
        run = base / 'run'
        run.mkdir()
        configure_cell_power(run, machine, 'handset33', incoming_call=True, traffic_assignment=True,
                             call_ack=True, clear_call=True, sacch=True, speech=True)
        command = [str(args.mame), machine, '-rompath', f'{base / "roms"};{ROOT / "roms"}',
                   '-nvram_directory', str(base / 'nvram'), '-cfg_directory', 'cfg',
                   '-snapshot_directory', 'snap', '-noreadconfig', '-video', 'none', '-sound', 'none',
                   '-nothrottle', '-log', '-autoboot_delay', '0',
                   '-autoboot_script', str(ROOT / 'tools/rom6_lab_call.lua'),
                   '-seconds_to_run', str(model['page'] + 20), '-nonvram_save']
        environment = {k: v for k, v in os.environ.items() if not k.startswith(('NOKIA_DCT3_', 'ROM6_LAB_'))}
        environment.update(ROM6_LAB_PAGE=str(model['page']), ROM6_LAB_SEND=model['send'],
                           ROM6_LAB_CODE='1' if model['code'] else '0')
        with (run / 'console.log').open('w') as output:
            subprocess.run(command, cwd=run, env=environment, stdout=output, stderr=subprocess.STDOUT,
                           check=True, timeout=1800)
        text = (run / 'error.log').read_text(errors='replace')
        if '[LUA ERROR]' in (run / 'console.log').read_text(errors='replace') or 'LUA ERROR' in text:
            raise ValueError('call script failed')
        check_inputs(text, model)
        check_registration(text)
        result = check_traffic_call(text)
        from PIL import Image
        for name, digest in model['frames'].items():
            with Image.open(run / 'snap' / name) as image:
                actual = hashlib.sha256(image.convert('L').tobytes()).hexdigest()
            if actual != digest:
                raise ValueError(f'{name} differs from its reviewed frame: {actual}')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'Native {model["name"]} lab call failed: {error}; evidence in {base}\n')
    print(' '.join(f'{key}={value}' for key, value in result.items()))
    print(f'Native {model["name"]} lab call: PASS registration, ringing, Answer, speech both ways and End')
    if args.keep:
        print(f'evidence retained: {base}')
    else:
        shutil.rmtree(base)


if __name__ == '__main__':
    main()

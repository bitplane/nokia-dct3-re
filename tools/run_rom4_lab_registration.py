#!/usr/bin/env python3
"""Run native ROM4 Location Updating on a laboratory BTS machine and check it."""
import argparse
import os
import shutil
from pathlib import Path
import sys
import tempfile

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.make_5110_eeprom_profile import build_profile
from tools.process_runner import run_emulator
from tools.rom4_lab_registration_check import (NSE1_LCD_SHA256, check, check_incoming_call, check_lcd,
                                              check_lcd_sequence, check_roundtrip, check_traffic_call)
from tools.run_nse2_native import prepare_native

HEADLESS = ['-noreadconfig', '-midiprovider', 'none', '-keyboardprovider', 'none',
            '-mouseprovider', 'none', '-lightgunprovider', 'none', '-joystickprovider', 'none',
            '-video', 'none', '-sound', 'none', '-log', '-verbose', '-skip_gameinfo', '-nothrottle']
# Laboratory BTS options: incoming SETUP, RR acknowledgement of call control,
# network clearing, and SACCH.
CALL_BTS_BITS = (4, 16, 32, 64)
# The same call moved to TCH/F on timeslot 1 after CALL CONFIRMED, with
# speech: a 1 kHz downlink tone, and a 500 Hz microphone tone from the lab.
TRAFFIC_BTS_BITS = (4, 8, 16, 32, 64, 2048)
# The page follows Location Updating; Answer and End are physical left-softkey presses.
CALL_INPUTS = {'NPE3_NATIVE_PAGE_AT': '20', 'NOKIA_DCT3_POST_READY_KEYS': 'menu,menu',
               'NOKIA_DCT3_POST_READY_KEY_DELAY_MS': '23000', 'NOKIA_DCT3_POST_READY_KEY_DURATION_MS': '220',
               'NOKIA_DCT3_POST_READY_KEY_GAP_MS': '4000'}
RINGING_LCD_SHA256 = '12fbdc9400c3ca1fc46e4769c9eac25eb40f97ec79f766bc3c2383515a00952c'
CALL_LCD_SHA256 = 'd74c752eb952af734ca6ab0243080daf1bc1a86b370d852da7205e3c611313ff'
# A mid-call save, 200 ms of recorded DSP I/O, then a load that must replay it.
ROUNDTRIP_REPLAY_MS = 200


def write_bts_config(run, machine, bits):
    ports = ''.join(f'<port tag=":LAB_BTS" type="CONFIG" mask="{bit}" defvalue="0" value="{bit}" />' for bit in bits)
    (run / 'cfg').mkdir()
    (run / 'cfg' / f'{machine}.cfg').write_text(
        f'<?xml version="1.0"?>\n<mameconfig version="10"><system name="{machine}"><input>{ports}'
        '</input></system></mameconfig>\n')


def command(root, run, machine, mame, call=False, traffic=False, roundtrip=None):
    if machine == 'nse1ap5':
        nvram = run / 'nvram/nse1ap5'
        nvram.mkdir(parents=True)
        (run / 'snap').mkdir()
        nvram.joinpath('eeprom').write_bytes(build_profile(
            (root / 'roms/noki5110/nse-1.bin').read_bytes(),
            (root / 'roms/noki5110/5110f530.fls').read_bytes()))
        if call:
            write_bts_config(run, machine, TRAFFIC_BTS_BITS if traffic else CALL_BTS_BITS)
            os.environ.update(CALL_INPUTS)
        if roundtrip is not None:
            # Lua key timers are outside the machine image: only Answer is
            # scheduled before the save, and the restored branch presses End
            # at the same emulated time as the ordinary run.
            end = 23 + 0.22 + 4
            if not 23.5 <= roundtrip <= end - 1:
                raise ValueError('the round trip must fall between Answer and End')
            os.environ.update({'NOKIA_DCT3_POST_READY_KEYS': 'menu',
                               'NOKIA_DCT3_STATE_ROUNDTRIP_AT': str(roundtrip),
                               'NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS': str(ROUNDTRIP_REPLAY_MS),
                               'NOKIA_DCT3_STATE_ROUNDTRIP_END_KEY': 'menu',
                               'NOKIA_DCT3_STATE_ROUNDTRIP_END_DELAY_MS':
                                   str(round((end - roundtrip) * 1000) - ROUNDTRIP_REPLAY_MS)})
        # The staged menu script adds the LCD mirror and the staged-CPU end marker.
        script = root / ('tools/rom4_lab_call_restore.lua' if roundtrip is not None else
                         'tools/rom4_lab_call_input.lua' if call else 'tools/c54x_rom4_staged_menu.lua')
        return [str(mame), machine, '-rompath', str(root / 'mame/roms'), '-nvram_directory', str(run / 'nvram'),
                '-cfg_directory', str(run / 'cfg'), '-seconds_to_run', '36' if call else '22', *HEADLESS,
                '-autoboot_script', str(script)]
    # The 5110i runs past its former 62-second reason-68 reset.
    prepare_native(root, run, record_checksum=True, nominal_vbat=True, insert_sim=True,
                   phone_retry_zero=True, security_records=True, lab=True)
    return [str(mame), machine, '-rompath', str(run / 'roms'), '-nvram_directory', str(run / 'nvram'),
            '-cfg_directory', str(run / 'cfg'), '-seconds_to_run', '70', *HEADLESS]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('machine', choices=('nse1ap5', 'nse2ap5'))
    parser.add_argument('--mame', type=Path)
    parser.add_argument('--incoming-call', action='store_true',
                        help='page after registration, answer, then end the call (nse1ap5)')
    parser.add_argument('--traffic-channel', action='store_true',
                        help='the incoming call, assigned to TCH/F on timeslot 1 (nse1ap5)')
    parser.add_argument('--state-roundtrip', type=float, metavar='SECONDS',
                        help='save, replay and reload the traffic-channel call at this time (nse1ap5)')
    parser.add_argument('--keep', action='store_true', help='retain the run directory after a pass')
    args = parser.parse_args()
    args.traffic_channel |= args.state_roundtrip is not None
    args.incoming_call |= args.traffic_channel
    if args.incoming_call and args.machine != 'nse1ap5':
        parser.error('the incoming-call scenario is defined for nse1ap5')
    root = Path(__file__).resolve().parents[1]
    scratch = root / 'tmp/native-gates'
    scratch.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix=f'{args.machine}-registration.', dir=scratch))
    os.rmdir(run)  # prepare_native requires a new directory
    try:
        cmd = command(root, run, args.machine, (args.mame or root / 'mame/mame').resolve(), args.incoming_call,
                      args.traffic_channel, args.state_roundtrip)
        run.mkdir(exist_ok=True)
        os.environ['NOKIA_DCT3_SNAPSHOT_DIR'] = str(run / 'snap')
        with open(run / 'console.log', 'wb') as output:
            status = run_emulator(cmd, run, output, timeout=1200)
        if status:
            raise ValueError(f'emulator exited {status}')
        text = (run / 'error.log').read_text(errors='replace')
        if args.incoming_call:
            page = text.find('npe3_bts_page_request:')
            result = check(text[:page] if page >= 0 else text)
            result.update(check_traffic_call(text) if args.traffic_channel else check_incoming_call(text))
            check_lcd_sequence(run / 'snap', (RINGING_LCD_SHA256, CALL_LCD_SHA256), NSE1_LCD_SHA256)
            if args.state_roundtrip is not None:
                result.update(check_roundtrip(text))
        else:
            result = check(text)
        if args.machine == 'nse1ap5':
            if 'rom4_staged_end:' not in text or 'illegal=0' not in text.split('rom4_staged_end:', 1)[1].split('\n', 1)[0]:
                raise ValueError('staged ROM4 execution did not end cleanly')
            if not args.incoming_call:
                check_lcd(run / 'snap', NSE1_LCD_SHA256)
    except Exception as error:
        raise SystemExit(f'FAIL - {error}; evidence retained: {run}')
    print(' '.join(f'{key}={value}' for key, value in result.items()))
    scenario = ('traffic-channel call with state round trip' if args.state_roundtrip is not None else
                'traffic-channel call' if args.traffic_channel else
                'incoming call' if args.incoming_call else 'Location Updating')
    print(f'NSE ROM4 {args.machine} native laboratory {scenario}: PASS')
    if args.keep:
        print(f'evidence retained: {run}')
    else:
        shutil.rmtree(run)


if __name__ == '__main__':
    main()

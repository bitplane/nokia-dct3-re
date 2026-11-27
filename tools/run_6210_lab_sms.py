#!/usr/bin/env python3
"""Native SMS on the 6210 laboratory cell, both directions.

The lab cell runs its SDCCH through the GSM session and LAPDm link
(LAB_BTS 0x4000): Location Updating, then either an incoming SMS (the lab page
is pressed at 5 s; the cell pages once registration completes, delivers the
network's queued message on SAPI 3 and the phone is read with its own keys) or
an outgoing SMS ("A" to 5551234, composed and sent with the phone's keys).

Checks the air dialogue from the cell's log (segmented CP-DATA, the handset's
CP-ACK and RP-ACK or its SMS-SUBMIT, the network's CP-ACK and RP-ACK, channel
release) and the pinned screens: the received "hello" and the sent report.
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
sys.path.insert(0, str(ROOT))
from tools.bench_6210_call import MODEL, prepare  # noqa: E402
from tools.run_npe3_native import configure_cell_power  # noqa: E402

SECONDS = {'incoming': 36, 'outgoing': 42}
SCREENS = {'incoming': 'sms_read_1', 'outgoing': 'sms_sent'}


def uplinks(text):
    return re.findall(r'npe3_session_uplink: result=\d+ sapi=(\d) layer3=([0-9a-f]*) ', text)


def downlinks(text):
    return re.findall(r'npe3_session_downlink: kind=(\S+) start_frame=\d+ octets=([0-9a-f]+) ', text)


def information(octets):
    """Layer 3 of a downlink I-frame (address, control, length, data)."""
    data = bytes.fromhex(octets)
    return data[3:3 + (data[2] >> 2)].hex()


def check(text, scenario):
    if text.count('npe3_session_released: registered=1') < 2:
        raise ValueError('registration and the SMS dialogue did not both release the channel')
    if text.count('native_dsp_reset_line:') != 3:
        raise ValueError('the DSP reset line moved after boot')
    sent = [information(octets) for kind, octets in downlinks(text) if kind.startswith('information')]
    heard = [layer3 for sapi, layer3 in uplinks(text) if sapi == '3']
    if scenario == 'incoming':
        # CP-DATA (TI 0) carrying RP-DATA network->MS, sent in segments.
        if not any(item.startswith('0901') for item in sent):
            raise ValueError('no CP-DATA delivery')
        if '8904' not in heard:
            raise ValueError('the handset did not acknowledge CP-DATA')
        if not any(re.fullmatch(r'89010202[0-9a-f]{2}', item) for item in heard):
            raise ValueError('the handset did not send RP-ACK')
        if '0904' not in sent:
            raise ValueError('the network did not acknowledge the RP-ACK')
    else:
        submit = [item for item in heard if item.startswith('3901') and item[6:8] == '00']
        # SMS-SUBMIT to 5551234 (international, 7 digits) with text "A".
        if not any('0781551532f4' in item and item.endswith('0141') for item in submit):
            raise ValueError('no SMS-SUBMIT of "A" to 5551234')
        if 'b904' not in sent or not any(re.fullmatch(r'b90102030[0-9a-f]', item) or
                                         re.fullmatch(r'b901020[0-9a-f]{3}', item) for item in sent):
            raise ValueError('the network did not send CP-ACK and RP-ACK')
        if '3904' not in heard:
            raise ValueError('the handset did not acknowledge the RP-ACK')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('scenario', choices=('incoming', 'outgoing'))
    parser.add_argument('--mame', type=Path, default=ROOT / 'mame/mame')
    parser.add_argument('--screen-sha256', help='expected SHA-256 of the final screen snapshot')
    parser.add_argument('--keep', action='store_true')
    args = parser.parse_args()
    scratch = ROOT / 'tmp/native-gates'
    scratch.mkdir(parents=True, exist_ok=True)
    base = Path(tempfile.mkdtemp(prefix=f'npe3-lab-sms-{args.scenario}.', dir=scratch))
    try:
        prepare(base)
        directory = base / 'handset'
        directory.mkdir()
        configure_cell_power(directory, MODEL, 'handset33', sacch=True, sms=True)
        environment = {k: v for k, v in os.environ.items() if not k.startswith('NOKIA_DCT3_')}
        environment['NPE3_LAB_SMS'] = args.scenario
        command = [str(args.mame.resolve()), MODEL, '-rompath', f'{base / "roms"};{ROOT / "roms"}',
                   '-nvram_directory', str(base / 'nvram'), '-cfg_directory', 'cfg',
                   '-snapshot_directory', 'snap', '-noreadconfig', '-skip_gameinfo', '-nonvram_save',
                   '-video', 'none', '-sound', 'none', '-nothrottle',
                   '-seconds_to_run', str(SECONDS[args.scenario]), '-log', '-autoboot_delay', '0',
                   '-autoboot_script', str(ROOT / 'tools/npe3_lab_sms_input.lua')]
        with (directory / 'console.log').open('wb') as console:
            subprocess.run(command, cwd=directory, env=environment, stdout=console,
                           stderr=subprocess.STDOUT, check=True)
        if b'LUA ERROR' in (directory / 'console.log').read_bytes():
            raise ValueError('the input script failed')
        check((directory / 'error.log').read_text(errors='replace'), args.scenario)
        screen = directory / 'snap' / f'{SCREENS[args.scenario]}.png'
        digest = hashlib.sha256(screen.read_bytes()).hexdigest()
        if args.screen_sha256 and digest != args.screen_sha256:
            raise ValueError(f'{screen.name} differs ({digest})')
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        raise SystemExit(f'FAIL - {error}; evidence retained: {base}')
    print(f'{SCREENS[args.scenario]}={digest[:16]}')
    print(f'NPE-3 native lab {args.scenario} SMS: PASS')
    if args.keep:
        print(f'evidence retained: {base}')
    else:
        shutil.rmtree(base)


if __name__ == '__main__':
    main()

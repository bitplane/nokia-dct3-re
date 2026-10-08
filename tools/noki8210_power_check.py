"""Physical NSM-3 rail-off/wake in the declared base-record HLE comparison."""
import argparse
import hashlib
from pathlib import Path
import re
import sys
from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.run_noki8210_acceptance import check_host_registration


IDLE_CROP_SHA256 = '59b772b8dd4715490911ec43c4969b76a4cb57708473d2345b31f8e22fd77b7b'


def verify(text, storage):
    if '[LUA ERROR]' in text:
        raise ValueError('NSM-3 observer failed')
    actions = list(re.finditer(r'8210_power_physical: action=(\w+)', text))
    if [event[1] for event in actions] != [
            'shutdown_press', 'shutdown_release', 'restart_press', 'restart_release']:
        raise ValueError('NSM-3 physical power sequence differs')
    off = list(re.finditer(r'ccont_power: event=off t=([0-9.]+)', text))
    wake = list(re.finditer(r'ccont_power: event=wake cause=(\w+) t=([0-9.]+)', text))
    if len(off) != 1 or len(wake) != 1 or wake[0][1] != '02':
        raise ValueError('NSM-3 requires one rail-off and physical PWRONX wake')
    off, wake = off[0], wake[0]
    if not actions[1].end() < off.start() < actions[2].start() < wake.start() < actions[3].start():
        raise ValueError('NSM-3 rail transitions do not follow physical inputs')
    if float(wake[2]) - float(off[1]) < 8:
        raise ValueError('NSM-3 off interval is too short')
    interval = text[off.end():wake.start()]
    ticks = re.findall(r'ccont_rtc: event=second time=12:00:(\d+) day=1[^\n]*t=([0-9.]+)', interval)
    if len(ticks) < 8 or any(int(second) != float(when) for second, when in ticks) or any(
            float(right[1]) - float(left[1]) != 1 for left, right in zip(ticks, ticks[1:])):
        raise ValueError('NSM-3 RTC did not keep ticking while off')
    if re.search(r'dspif_transport: (?:RX enqueue|FIQ0 notify|peer RAM W)|'
                 r'rom4_(?:timing_port|port_write):|staged_dsp: publication|'
                 r'radio_peer: LAPDm|dsp_hle: speech', interval):
        raise ValueError('NSM-3 DSP/radio generated activity while off')
    before, after = text[:off.start()], text[wake.end():]
    cause = re.search(r'ccont_power: event=cause_read data=(\w+)', after)
    if not cause or int(cause[1], 16) & 7 != 3:
        raise ValueError('NSM-3 did not consume ready/PWRONX without charger')
    if not re.search(r'ccont_rtc: event=counter_write reg=07 data=00\b', after):
        raise ValueError('NSM-3 firmware seconds reinitialization absent')
    for boot in (before, after):
        check_host_registration(boot, storage)
    read = re.search(r'read-binary fid=6f7e offset=0 length=11', after)
    request = re.search(r'TX packet type=1b .*data=0080013f4905087200f110000133080910101032547698', after)
    if not read or not request or read.end() >= request.start():
        raise ValueError('NSM-3 warm Location Updating did not use retained LAI')
    if len(re.findall(r'update-binary fid=6f7e offset=10 length=1', before)) != 1 or re.search(
            r'update-binary fid=6f7e offset=10 length=1', after):
        raise ValueError('NSM-3 warm boot unexpectedly rewrote location status')
    keys = list(re.finditer(r'8210_power_security: key=([^\n]+)', after))
    if [key[1] for key in keys] != ['Keypad 1', 'Keypad 2', 'Keypad 3', 'Keypad 4', 'Keypad 5', 'Menu']:
        raise ValueError('NSM-3 restart security input differs')
    for index, key in enumerate(keys):
        end = keys[index + 1].start() if index + 1 < len(keys) else len(after)
        decoded = 0x19 if index == 5 else index + 1
        if not re.search(rf'8210_keypad_decoded: key={decoded:02x}\b', after[key.end():end]):
            raise ValueError('NSM-3 restart security key did not decode')


def check_frames(directory):
    for name in ('8210_power_idle.png', '8210_power_after_security.png'):
        with Image.open(directory / name) as image:
            frame = image.convert('L')
            if frame.size != (84, 48) or hashlib.sha256(
                    frame.crop((15, 0, 69, 16)).tobytes()).hexdigest() != IDLE_CROP_SHA256:
                raise ValueError('NSM-3 reviewed operator pixels differ: ' + name)
    with Image.open(directory / '8210_power_off.png') as image:
        frame = image.convert('L')
        if frame.size != (84, 48) or frame.getextrema() != (255, 255):
            raise ValueError('NSM-3 rail-off screen is not blank')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('storage', type=Path)
    parser.add_argument('frames', type=Path)
    args = parser.parse_args()
    try:
        with args.log.open(errors='replace') as stream:
            text = ''.join(line for line in stream if not line.startswith('[opcov]'))
        verify(text, args.storage.read_bytes())
        check_frames(args.frames)
    except (OSError, ValueError) as error:
        parser.exit(1, f'8210 power lifecycle FAIL: {error}\n')
    print('8210 physical rail-off/wake, retained location and security continuation PASS; native speech unproved')


if __name__ == '__main__':
    main()

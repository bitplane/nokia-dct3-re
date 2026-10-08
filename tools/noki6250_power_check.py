"""Own-PMM NHM-3 research HLE physical rail-off and warm registration."""
import argparse
import hashlib
from pathlib import Path
import re
import sys
from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.noki6250_coherent_registration_check import verify as check_cold
from tools.noki6250_staged_check import check as check_uploads
from tools.power_domain_contract import require_endpoint_silence
from tools.radio_registration_trace_check import verify as check_registration

IDLE_HASH = '519c59eb68967255c5b2f7cd2b8ae2351cac00b03828c78cf86109e5496a6f49'


def verify(text, storage):
    if '[LUA ERROR]' in text or '6250_nv_sum_failure:' in text:
        raise ValueError('NHM-3 observer or NV validation failed')
    actions = list(re.finditer(r'6250_power_physical: step=(\d) pressed=(\d) t=([0-9.]+)', text))
    if [(m[1], m[2]) for m in actions] != [('1', '1'), ('2', '0'), ('3', '1'), ('4', '0')]:
        raise ValueError('NHM-3 physical power sequence differs')
    off = list(re.finditer(r'ccont_power: event=off t=([0-9.]+)', text))
    wake = list(re.finditer(r'ccont_power: event=wake cause=(\w+) t=([0-9.]+)', text))
    if len(off) != 1 or len(wake) != 1 or wake[0][1] != '02':
        raise ValueError('NHM-3 requires one rail-off and PWRONX wake')
    off, wake = off[0], wake[0]
    if not actions[1].end() < off.start() < actions[2].start() < wake.start() < actions[3].start():
        raise ValueError('NHM-3 rail ordering differs')
    if float(wake[2]) - float(off[1]) < 8:
        raise ValueError('NHM-3 off interval too short')
    interval = text[off.end():wake.start()]
    require_endpoint_silence(interval, 'NHM-3 powered endpoint active while off')
    # Day zero is firmware-written in this comparison, not a valid calendar claim.
    ticks = re.findall(r'ccont_rtc: event=second time=12:00:(\d+) day=0[^\n]*t=([0-9.]+)', interval)
    if len(ticks) < 8 or any(int(s) != float(t) for s, t in ticks) or any(
            float(b[1]) - float(a[1]) != 1 for a, b in zip(ticks, ticks[1:])):
        raise ValueError('NHM-3 RTC continuity differs')
    before, after = text[:off.start()], text[wake.end():]
    check_cold(before, storage)
    check_uploads(after, runtime=True)
    check_registration(after, 'nhm3', preserved=True, configured_carrier=True)
    if not re.search(r'gsm_call_adapter: network registered=1 arfcn=19\b', after):
        raise ValueError('NHM-3 warm host carrier differs')
    read = re.search(r'read-binary fid=6f7e offset=0 length=11', after)
    request = re.search(r'TX packet type=1b .*data=0080013f4905087200f110000133080910101032547698', after)
    if not read or not request or read.end() >= request.start():
        raise ValueError('NHM-3 warm request did not follow retained location read')
    cause = re.search(r'ccont_power: event=cause_read data=(\w+)', after)
    if not cause or int(cause[1], 16) & 7 != 3:
        raise ValueError('NHM-3 restart cause not consumed')
    endpoints = re.findall(r'6250_power_endpoint: phase=(restart|settled) faults=([0-9a-f]+)', after)
    if [p for p, _ in endpoints] != ['restart', 'settled'] or any(
            len(data) != 48 or bytes.fromhex(data)[12] != 0 for _, data in endpoints):
        raise ValueError('NHM-3 second-boot NV fault endpoints differ')


def check_frames(directory):
    for name in ('idle', 'settled'):
        with Image.open(directory / f'6250_power_{name}.png') as image:
            frame = image.convert('L')
            if frame.size != (96, 60) or hashlib.sha256(
                    frame.crop((0, 8, 96, 60)).tobytes()).hexdigest() != IDLE_HASH:
                raise ValueError('NHM-3 reviewed idle pixels differ')
    with Image.open(directory / '6250_power_off.png') as image:
        frame = image.convert('L')
        if frame.size != (96, 60) or frame.getextrema() != (255, 255):
            raise ValueError('NHM-3 off display not blank')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('storage', type=Path)
    parser.add_argument('frames', type=Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors='replace'), args.storage.read_bytes())
        check_frames(args.frames)
    except (OSError, ValueError) as error:
        parser.exit(1, f'6250 power FAIL: {error}\n')
    print('6250 physical off/wake and coherent warm registration PASS; native speech unproved')


if __name__ == '__main__':
    main()

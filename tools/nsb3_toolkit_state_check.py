"""Native save/load of a fetched NSB-3 DISPLAY TEXT awaiting physical input."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_toolkit_check import check_storage, DISPLAY, RESPONSE
from tools.nsb3_pin_check import check_trace as check_pin
from tools.nsb3_phonebook_check import wire
from tools.nse2_native_state_check import check_state
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.sim_security_trace_check import statuses

RETURNED_MENU = 'b2bde13413750ea3a6442a8a4d6c90362d4e8b16cde72bbf2f981a00089cecd0'


def check_replay_protocol(text):
    check_pin(text, True)
    if 'nsb3_board_reset_request:' in text or 'Error in' in text or 'LUA ERROR' in text:
        raise ValueError('reset or failed observer')
    commands = wire(text)
    toolkit = [row[:5] for row in commands if row[0] in (0x10, 0x12, 0x14, 0xc2)]
    response = (0x14, 0, 0, 12, RESPONSE)
    if toolkit != [(0x10, 0, 0, 5, bytes.fromhex('0b41977700')), (0x12, 0, 0, 22, b''), response, response]:
        raise ValueError('requires one fetched command and two exact replayed responses')
    if statuses(text, 0x10) != [(0x9000, 3, 10)] or statuses(text, 0x14) != [(0x9000, 3, 10)] * 2:
        raise ValueError('pending card transaction did not complete twice')
    if text.count('SIM completion ins=f2 sw=9116') != 1:
        raise ValueError('missing unique organic pending-command notification')
    if any(row[0] in (4, 0x44, 0xdc, 0xd6, 0x24, 0x26, 0x28, 0x2c, 0x32) for row in commands):
        raise ValueError('unrelated persistent card mutation')
    rx = bytes.fromhex(''.join(re.findall(r'nsb3_sim_wire: direction=RX data=([0-9a-f]{2}) ', text)))
    if rx.count(DISPLAY + b'\x90\x00') != 1:
        raise ValueError('exact initial card command missing or refetched')
    for phase, count in (('reference', 0), ('restored', 1)):
        marker = f'nsb3_state_contact: phase={phase} down=1'
        if text.count(marker) != 1:
            raise ValueError('missing physical replay contact')
        if sum(row[0] == 0x14 for row in wire(text.split(marker, 1)[0])) != count:
            raise ValueError('response preceded its physical replay contact')


def check(source, run):
    check_storage(source, run, 1)
    text = (run / 'error.log').read_text()
    check_replay_protocol(text)
    before = (run / 'initial_sim_card.bin').read_bytes()
    after = (run / 'nvram/nsb3r4t/sim_card').read_bytes()
    pixels = []
    for phase in ('reference', 'restored'):
        with Image.open(run / f'snap/nsb3_state_{phase}.png') as image:
            if image.size != (84, 48):
                raise ValueError('own frame geometry differs')
            pixels.append(image.convert('RGB').tobytes())
    report = check_state(text, before, after, *pixels, prefix='nsb3',
                         checkpoint='65.000000000', card_size=3570, menu_frame=RETURNED_MENU)
    frame_check(run / 'snap/nsb3_toolkit_state_prompt.png',
                '0b61ca941a012e9feffe2d13b834c720031b71295220fc32139decd034172b2c')
    state = run / 'sta/nsb3r4t/nsb3_native_ui.sta'
    if not state.is_file() or state.stat().st_size == 0:
        raise ValueError('native saved-state artifact missing')
    with state.open('rb') as stream:
        report['state_sha256'] = hashlib.file_digest(stream, 'sha256').hexdigest()
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('run', type=Path)
    args = parser.parse_args()
    try:
        report = check(args.source, args.run)
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f'NSB-3 Toolkit restoration: FAIL: {exc}\n')
    print(f"PASS: pending card/UI restoration, two physical completions and {report['io_replay_events']} exact I/O writes; no registration claim")


if __name__ == '__main__':
    main()

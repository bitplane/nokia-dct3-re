"""Own NSB-3 organic TERMINAL PROFILE/FETCH/physical DISPLAY TEXT completion."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.make_sim_card_profile import make_current_profile
from tools.nsb3_phonebook_check import wire
from tools.nsb3_pin_check import check_trace as check_pin
from tools.nsb3_puk_check import check_link
from tools.nsb3_nominal_input import IMAGE_SHA256
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.sim_security_trace_check import statuses

DISPLAY = bytes.fromhex('d0148103012180820281028d0904') + b'DCT3 SAT'
RESPONSE = bytes.fromhex('810301218002028281030100')


def check_trace(text):
    check_pin(text, True)
    if 'nsb3_board_reset_request:' in text or 'LUA ERROR' in text or 'Error in' in text:
        raise ValueError('reset or failed observer')
    commands = wire(text)
    toolkit = [row[:5] for row in commands if row[0] in (0x10, 0x12, 0x14, 0xc2)]
    if toolkit != [(0x10, 0, 0, 5, bytes.fromhex('0b41977700')),
                   (0x12, 0, 0, 22, b''), (0x14, 0, 0, 12, RESPONSE)]:
        raise ValueError('own Toolkit APDU sequence or result differs')
    if any(row[0] in (4, 0x44, 0xdc, 0xd6, 0x24, 0x26, 0x28, 0x2c, 0x32) for row in commands):
        raise ValueError('unrelated card mutation')
    if statuses(text, 0x10) != [(0x9000, 3, 10)] or statuses(text, 0x14) != [(0x9000, 3, 10)]:
        raise ValueError('Toolkit command completion status differs')
    if text.count('SIM completion ins=f2 sw=9116') != 1:
        raise ValueError('missing unique proactive-pending STATUS')
    rx = bytes.fromhex(''.join(re.findall(r'nsb3_sim_wire: direction=RX data=([0-9a-f]{2}) ', text)))
    if rx.count(DISPLAY + b'\x90\x00') != 1:
        raise ValueError('missing exact card-originated proactive payload')
    contacts = re.findall(r'nsb3_toolkit_physical: action=dismiss down=([01]) t=([0-9.]+)', text)
    if contacts != [('1', '68.000000000'), ('0', '68.150000000')]:
        raise ValueError('physical dismissal differs')
    if text.index('SIM completion ins=f2 sw=9116') > text.index('nsb3_toolkit_physical: action=dismiss down=1'):
        raise ValueError('dismissal precedes pending command')
    before = text.split('nsb3_toolkit_physical: action=dismiss down=1', 1)[0]
    if any(row[0] == 0x14 for row in wire(before)):
        raise ValueError('terminal response preceded physical dismissal')


def check(source, run):
    check_link(source, run)
    metadata = json.loads((run / 'cold_source.json').read_text())
    if metadata.get('external_toolkit_card') != dict(profile=1,
            description='Phase 2+ with one DISPLAY TEXT command',
            handset_storage_changed=False, persistent_card_changed=False):
        raise ValueError('external Toolkit profile provenance differs')
    storage = run / 'nvram/nsb3r4t'
    if hashlib.sha256((storage / 'flash').read_bytes()).hexdigest() != IMAGE_SHA256:
        raise ValueError('own executable changed')
    if (run / 'initial_sim_card.bin').read_bytes() != make_current_profile(True) or (storage / 'sim_card').read_bytes() != make_current_profile(True):
        raise ValueError('complete card changed')
    check_trace((run / 'error.log').read_text())
    frame_check(run / 'snap/nsb3_toolkit_display.png', '0b61ca941a012e9feffe2d13b834c720031b71295220fc32139decd034172b2c')
    frame_check(run / 'snap/nsb3_toolkit_dismissed.png', 'b2bde13413750ea3a6442a8a4d6c90362d4e8b16cde72bbf2f981a00089cecd0')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('run', type=Path)
    args = parser.parse_args()
    try:
        check(args.source, args.run)
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f'NSB-3 Toolkit: FAIL: {exc}\n')
    print('PASS: own physical DISPLAY TEXT lifecycle; native registration unproved')


if __name__ == '__main__':
    main()

"""Exact NSB-7 settled-idle restoration and physical post-load Menu."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb7_radio_check import contract
from tools.nsb7_security_check import check_trace as check_security
from tools.nsb7_netmonitor_check import IDLE_HASH, frame
from tools.nsb7_menu_check import FRAMES
from tools.radio_state_roundtrip import verify_roundtrip


def check(text):
    if '[LUA ERROR]' in text or 'nsb7_state: FAIL' in text:
        raise ValueError('idle state fixture failed')
    rows = re.findall(r'nsb7_state: event=(saved|restored) pc=([0-9a-f]{8}) '
                      r'sp=([0-9a-f]{8}) ram=([0-9a-f]{8}) cpu=([0-9a-f,]+) t=([0-9.]+)', text)
    if len(rows) != 2 or [row[0] for row in rows] != ['saved', 'restored']:
        raise ValueError('missing architectural snapshots')
    if any(not re.fullmatch(r'[0-9a-f]{8}(?:,[0-9a-f]{8}){36}', row[4]) for row in rows):
        raise ValueError('incomplete ARM/banked register inventory')
    if rows[0][1:] != rows[1][1:]:
        raise ValueError('CPU/RAM/time restoration differs')
    verify_roundtrip(text, ('TX packet', 'RX enqueue', 'GSM service', 'sim_device:'), 'NSB-7 idle')
    if not re.search(r'state_replay: phase=restored event=end[^\n]*\n[\s\S]*'
                     r'nsb7_state_physical: key=Menu state=1[\s\S]*'
                     r'nsb7_key_decode: key=19 observer_writes=0[\s\S]*'
                     r'nsb7_state_physical: key=Menu state=0', text):
        raise ValueError('missing post-load physical Menu decode')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('run', type=Path)
    parser.add_argument('source', type=Path, help='own input storage run')
    args = parser.parse_args()
    try:
        contract(args.image.read_bytes())
        text = (args.run / 'error.log').read_text()
        check_security(text)
        check(text)
        for name in ('reference', 'restored'):
            frame(args.run / f'snap/nsb7_state_idle_{name}.png', IDLE_HASH)
        frame(args.run / 'snap/nsb7_state_idle_menu.png', FRAMES['nsb7_menu_26.png'])
        if (args.run / 'nvram/nsb7hle/sim_card').read_bytes() != (args.source / 'nvram/nsb7hle/sim_card').read_bytes():
            raise ValueError('idle replay changed persistent SIM storage')
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-7 idle state: FAIL: {error}\n')
    print('PASS: exact ARM/RAM/time restore, radio replay, clean idle pixels and physical Menu after load')


if __name__ == '__main__':
    main()

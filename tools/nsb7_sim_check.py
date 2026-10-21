"""Check own NSB-7 SIMI byte transport and startup file reads, not unlock/RF."""
import argparse
import hashlib
from pathlib import Path
import re
import sys
from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb7_input_check import IMAGE_SHA1
from tools.nsk3_phonebook_check import transmitted
from tools.sim_fdn_protocol_check import validity_targets


def check_trace(text):
    if re.findall(r'nsb7_profile: machine=(\w+) observer_writes=0', text) != ['nsb7hle']:
        raise ValueError('requires own explicit HLE composition')
    for marker in ('nsb7_card_fixture: present=1 physical_contact=1',
                   'runtime_hle_handoff pc=2c75 native_suspended=1',
                   'nsb7_simi: kind=read offset=38 value=40 pc=002eeefe',
                   'nsb7_simi: kind=write offset=39 value=b3 pc=002eeb9c'):
        if marker not in text:
            raise ValueError('activation/receive boundary absent: ' + marker)
    if any(error in text for error in ('LUA ERROR', 'c54x_failure:', 'observer_writes=1')):
        raise ValueError('runtime observer failure')
    tx = transmitted(text, 4096, 'nsb7_sim_tx')
    # Shared decoder validates PPS/APDU framing through the entire TX capture.
    if validity_targets(tx, 0x44):
        raise ValueError('unexpected rehabilitation command')
    for command in ('ff00ff', 'a0a40000023f00', 'a0a40000022fe2',
                    'a0b000000a', 'a0a40000026fae', 'a0a40000026f07',
                    'a0b0000009', 'a0a40000026f3a', 'a0c000000f'):
        if bytes.fromhex(command) not in tx:
            raise ValueError('startup command absent: ' + command)
    for record in range(1, 51):
        if bytes((0xa0, 0xb2, record, 4, 32)) not in tx:
            raise ValueError('ADN record read absent: ' + str(record))
    if not tx.endswith(bytes.fromhex('a0f2000016')):
        raise ValueError('final presence-monitor request absent')
    rx = re.findall(r'nsb7_simi: kind=read offset=37 value=([0-9a-f]{2}) ', text)
    if len(rx) >= 8192 or bytes.fromhex('3b1005ff00ff') not in bytes(int(v, 16) for v in rx):
        raise ValueError('ATR/PPS receive evidence incomplete')
    if 'nsb7_key_decode: key=01 observer_writes=0' not in text:
        raise ValueError('physical digit not decoded')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('run', type=Path)
    args = parser.parse_args()
    try:
        if hashlib.sha1(args.image.read_bytes()).hexdigest() != IMAGE_SHA1:
            raise ValueError('requires own qualified image')
        check_trace((args.run / 'error.log').read_text())
        frame = Image.open(args.run / 'snap/nsb7_key_10.png').convert('RGB')
        if frame.size != (84, 48) or hashlib.sha256(frame.tobytes()).hexdigest() != 'af76996d4be7837d42a2c47226b5eb1ccf96c09f996cd1d97aaffd374ec32d83':
            raise ValueError('Security code editor frame differs')
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-7 SIM transport: FAIL: {error}\n')
    print('PASS: own SIMI ATR/PPS, startup file/ADN reads and presence monitor; phone unlock/RF unproved')


if __name__ == '__main__':
    main()

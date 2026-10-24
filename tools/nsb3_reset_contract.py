"""Own-image reset-policy census and passive reason-publication qualification."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_activation_census import callers
from tools.nsb3_nominal_input import IMAGE_SHA256


def inspect(image):
    if hashlib.sha256(image).hexdigest() != IMAGE_SHA256:
        raise ValueError('requires own qualified NSB-3 executable')
    for address, encoded in (
        (0x2b3b8e, '2cddd000e76888682800d100e760201d4903f7b8f967806f'),
        (0x2b3a5e, '2001f7f3ff21e084886830018068'),
        (0x2a7948, '1c20f001f8dcf001f903'),
        (0x2a8b06, '4922700846f7'),
        (0x23fe04, '4dcf4cd478282868d116'),
    ):
        expected = bytes.fromhex(encoded)
        if image[address - 0x200000:address - 0x200000 + len(expected)] != expected:
            raise ValueError('own reset/receive instruction contract differs')
    mcu = image[:0xd6d00]
    policy = callers(mcu, 0x2a78a6)
    if policy['calls'] != [{'site': '0x20738c', 'input': 2},
                           {'site': '0x2b3a60', 'input': 1}]:
        raise ValueError('own direct policy callers differ')
    return {'reason_setter': callers(mcu, 0x2a8b06), 'policy': policy,
            'reason_byte': '0x10ff80', 'receive_activity': '0x10c4ce',
            'scope': 'direct Thumb calls and own dispatch fragments; not indirect-call closure or a fitted-mask/RF contract'}


def check_trace(text):
    writes = re.findall(r'nsb3_reset_reason_write: value=68 pc=002a8b08 lr=002a794f '
                        r't=([0-9.]+) observer_writes=0', text)
    resets = re.findall(r'nsb3_board_reset_request: data=05 pc=002b4b9e lr=0023ff2f '
                        r'reason=00000068 count=00 inhibit=00 t=([0-9.]+) observer_writes=0', text)
    if len(writes) != 1 or len(resets) != 1 or not float(writes[0]) < float(resets[0]):
        raise ValueError('requires ordered own reason68 policy publication and software reset')
    if 'nsb3_reset_event_tail: reason=68 caller=002b16b5 observer_writes=0' not in text:
        raise ValueError('own reset event tail missing')


def check_windows(text):
    check_trace(text)
    windows = re.findall(r'nsb3_receive_window: activity=([0-9a-f]{4}) observer_writes=0', text)
    if windows != ['0002', '0000']:
        raise ValueError('requires observed active then empty receive windows')
    if re.findall(r'nsb3_reset_policy_entry: kind=([0-9a-f]{8}) caller=([0-9a-f]{8}) observer_writes=0', text) != [('00000001', '002b3a65')]:
        raise ValueError('requires own receive-loop argument1 policy caller')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('--trace', type=Path)
    parser.add_argument('--windows', action='store_true')
    args = parser.parse_args()
    try:
        result = inspect(args.image.read_bytes())
        if args.trace:
            (check_windows if args.windows else check_trace)(args.trace.read_text())
        print(json.dumps(result, indent=2))
    except (OSError, ValueError) as exc:
        parser.exit(1, f'NSB-3 reset contract: FAIL: {exc}\n')


if __name__ == '__main__':
    main()

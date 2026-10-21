"""Qualify physical Net monitor disable, own storage and clean cold idle."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb7_radio_check import contract
from tools.nsb7_security_check import check_trace as check_security
from tools.nsb7_storage_check import inspect

IDLE_HASH = '18af845e6ddde36b7e5ba8ecb09c7d6c6c5a67170d2e56e18d99327961e0f147'


def check_keys(text):
    if '[LUA ERROR]' in text:
        raise ValueError('physical fixture failed')
    steps = [(26, 'Left Softkey / Menu', 0x19), (29, 'Scroll Up', 0x17),
             (32, 'Left Softkey / Menu', 0x19), (35, 'Keypad 0', 0x0a),
             (36, 'Keypad 0', 0x0a), (38, 'Left Softkey / Menu', 0x19)]
    rows = list(re.finditer(r'nsb7_netmonitor_input: key=(.*?) state=([01]) t=([0-9.]+)', text))
    expected = [(key, state, f'{second}.{fraction}') for second, key, _ in steps
                for state, fraction in [('1', '000000000'), ('0', '200000000')]]
    if [(row[1], row[2], row[3]) for row in rows] != expected:
        raise ValueError('physical test-00 selection differs')
    for index, (_, _, code) in enumerate(steps):
        if f'nsb7_key_decode: key={code:02x} observer_writes=0' not in text[rows[2*index].end():rows[2*index+1].start()]:
            raise ValueError('physical contact did not decode')
    if 'nsb7_netmonitor_done: t=46.' not in text:
        raise ValueError('selection observation incomplete')


def check_storage(control, disabled, cold):
    if any(len(cache) != 0x8000 for cache in (control, disabled, cold)):
        raise ValueError('logical storage size differs')
    differences = [(i, a, b) for i, (a, b) in enumerate(zip(control, disabled)) if a != b]
    if differences != [(0x4f1, 0xff, 0x3d)] or cold[0x4f1] != 0x3d:
        raise ValueError('own test-00 setting transition/retention differs')


def frame(path, expected):
    from PIL import Image
    with Image.open(path) as image:
        if image.size != (84, 48) or hashlib.sha256(image.convert('RGB').tobytes()).hexdigest() != expected:
            raise ValueError('Net monitor/idle frame differs: ' + str(path))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('image', 'control', 'disabled', 'cold'):
        parser.add_argument(name, type=Path)
    parser.add_argument('--call', type=Path, help='also qualify clean outgoing-call presentation')
    args = parser.parse_args()
    try:
        contract(args.image.read_bytes())
        caches = []
        for run in (args.control, args.disabled, args.cold):
            check_security((run / 'error.log').read_text())
            caches.append(inspect((run / 'nvram/nsb7hle/flash').read_bytes())[0])
        check_keys((args.disabled / 'error.log').read_text())
        check_storage(*caches)
        for name, expected in {
            'nsb7_netmonitor_29.png': '8bc22a997848abf796726c946c9936d051263ff4f8301f6fbd59275e074b848a',
            'nsb7_netmonitor_32.png': 'f43b6aa0abdbd8b5d076df206a58b6acd05c38d2e1b07a3f2bf16959458dafb8',
            'nsb7_netmonitor_36.png': 'b49a25424579fc1e8432e03fb722d5a61677c5604bac74e0ba7063a444508e76',
            'nsb7_netmonitor_38.png': IDLE_HASH,
            'nsb7_netmonitor_final.png': IDLE_HASH,
        }.items():
            frame(args.disabled / 'snap' / name, expected)
        frame(args.cold / 'snap/nsb7_security_25.png', IDLE_HASH)
        if args.call:
            from tools.nsb7_outgoing_call_check import check as check_call
            check_call((args.call / 'error.log').read_text())
            frame(args.call / 'snap/nsb7_outgoing_call_26.png',
                  '223ffbdc6b19f01641f293b0a168a65b8715c37b71a97ecc5bb972b8d15baea1')
            frame(args.call / 'snap/nsb7_outgoing_call_28.png',
                  '067a6a8ee85b4989a4295780ab87d1fe2eccbab47271107e8550dacc02883091')
            frame(args.call / 'snap/nsb7_outgoing_call_final.png', IDLE_HASH)
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-7 Net monitor: FAIL: {error}\n')
    print('PASS: physical test 01 -> 00, one logical setting-byte change, clean idle and cold retention')


if __name__ == '__main__':
    main()

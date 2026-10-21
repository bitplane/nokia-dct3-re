"""Physical NSB-7 PIN-request Off/On and cold prompt restoration."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb7_radio_check import contract
from tools.nsb7_pin_check import check_trace as check_pin, check_frame, serialized_profile
from tools.nsb7_security_check import check_trace as check_phone
from tools.nsb7_sms_check import tx_bytes
from tools.sim_security_trace_check import statuses, ENABLED_OFFSET
from tools.nsb7_storage_check import inspect


def check_request(text, enable):
    if '[LUA ERROR]' in text or 'nsb7_request_input: complete' not in text:
        raise ValueError('request-setting fixture incomplete')
    steps = []
    if not enable:
        steps = [(16+i, 'Keypad '+digit) for i, digit in enumerate('12345')]
        steps.append((21, 'Left Softkey / Menu'))
    steps += [(26, 'Left Softkey / Menu'), (28, 'Keypad 4'), (30, 'Keypad 6'),
              (32, 'Keypad 1'), (35, 'Left Softkey / Menu'), (38, 'Keypad 1'),
              (39, 'Keypad 2'), (40, 'Keypad 3'), (41, 'Keypad 4'),
              (42, 'Left Softkey / Menu'), (45, 'Scroll Up' if enable else 'Scroll Down'),
              (48, 'Left Softkey / Menu')]
    rows = list(re.finditer(r'nsb7_request_input: enable=([01]) key=(.*?) state=([01]) t=([0-9.]+)', text))
    expected = [(str(int(enable)), key, state, f'{second}.{fraction}') for second, key in steps
                for state, fraction in [('1', '000000000'), ('0', '200000000')]]
    if [row.groups() for row in rows] != expected:
        raise ValueError('physical request-setting sequence differs')
    for index, (_, key) in enumerate(steps):
        code = int(key[-1]) if key.startswith('Keypad ') else {
            'Left Softkey / Menu': 0x19, 'Scroll Up': 0x17, 'Scroll Down': 0x18}[key]
        if f'nsb7_key_decode: key={code:02x} observer_writes=0' not in text[rows[2*index].end():rows[2*index+1].start()]:
            raise ValueError('request-setting contact did not decode')
    instruction = 0x28 if enable else 0x26
    if statuses(text, instruction) != [(0x9000, 3, 10)] or \
            statuses(text, 0x20) != ([] if enable else [(0x9000, 3, 10)]):
        raise ValueError('startup VERIFY or setting command differs')
    if any(statuses(text, ins) for ins in (0x24, 0x2c, 0x26 if enable else 0x28)):
        raise ValueError('unrelated credential mutation')
    command = bytes((0xa0, instruction, 0, 1, 8)) + b'1234\xff\xff\xff\xff'
    if tx_bytes(text).count(command) != 1 or command not in tx_bytes(text[rows[-2].end():]):
        raise ValueError('setting APDU must follow physical final confirmation')


def check_cards(pairs):
    enabled = serialized_profile()
    disabled = bytearray(enabled)
    disabled[ENABLED_OFFSET] = 0
    if pairs != [(enabled, bytes(disabled)), (bytes(disabled), enabled), (enabled, enabled)]:
        raise ValueError('only the PIN-enabled byte may change across the cold Off/On chain')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('image', 'off', 'on', 'cold'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    try:
        contract(args.image.read_bytes())
        off = (args.off / 'error.log').read_text()
        on = (args.on / 'error.log').read_text()
        check_request(off, False)
        check_pin(off.split('nsb7_request_input: enable=0 key=Left Softkey / Menu state=1 t=26.', 1)[0], True)
        check_request(on, True)
        check_phone(on)
        check_pin((args.cold / 'error.log').read_text(), True)
        pairs = []
        for run in (args.off, args.on, args.cold):
            inspect((run / 'nvram/nsb7hle/flash').read_bytes())
            pairs.append(((run / 'initial_sim_card.bin').read_bytes(),
                          (run / 'nvram/nsb7hle/sim_card').read_bytes()))
        check_cards(pairs)
        check_frame(args.off / 'snap/nsb7_request_final.png',
                    '752726c0d75f026701668b091a97205962a9e9567c4576243f75829e4e4573b1')
        check_frame(args.on / 'snap/nsb7_request_final.png',
                    '17df00783805c3c649739a946794c264f7eb3e826c0859b9f5e285c13417df43')
        check_frame(args.cold / 'snap/nsb7_pin_prompt.png',
                    'a12b3c3fdb4a3d1e8ffeb5fc8986c3bc008bb86b1f2e927440848fe0420ffc0c')
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-7 PIN request: FAIL: {error}\n')
    print('PASS: physical Off/On, only enabled-byte changes, cold PIN bypass and restored PIN prompt')


if __name__ == '__main__':
    main()

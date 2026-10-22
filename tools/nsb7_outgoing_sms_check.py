"""Qualify a physical NSB-7 Reply and one-shot incoming delivery."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb7_radio_check import contract


def check_inputs(text, delay=0):
    if not isinstance(delay, int) or delay < 0:
        raise ValueError('physical input delay must be a nonnegative integer')
    if '[LUA ERROR]' in text:
        raise ValueError('physical fixture failed')
    keys = re.findall(r'nsb7_sms_reply_input: key=(.*?) state=([01]) t=([0-9.]+)', text)
    steps = [(28, 'Left Softkey / Menu'), (30, 'Scroll Down'),
             (32, 'Left Softkey / Menu'), (34, 'Left Softkey / Menu'),
             (36, 'Keypad 2'), (38, 'Left Softkey / Menu'),
             (40, 'Left Softkey / Menu'), (42, 'Left Softkey / Menu')]
    expected = [(key, state, f'{second+delay}.{fraction}') for second, key in steps
                for state, fraction in [('1', '000000000'), ('0', '200000000')]]
    if keys != expected:
        raise ValueError('physical Reply input sequence differs')


def check(text, delay=0):
    check_inputs(text,delay)
    submit = ('gsm_sms_submit: cp=39 rp=01 smsc=1234567890 destination=5551234 '
              'alphabet=0 user_length=1 outcome=0 status_report=0')
    if text.count('gsm_sms_submit:') != 1 or submit not in text:
        raise ValueError('expected one accepted single-character submission to 5551234')
    wire = 'data=390118000100069121436587090d11000781551532f40000a70141'
    markers = [wire, 'GSM service downlink kind=17 sapi=3 pd=09 message=04',
               'GSM service downlink kind=18 sapi=3 pd=09 message=01',
               'GSM service uplink sapi=3 pd=09 message=04 length=2 data=3904',
               'LAPDm service Channel Release acknowledged nr=2']
    cursor = 0
    for marker in markers:
        position = text.find(marker, cursor)
        if position < 0:
            raise ValueError(f'missing ordered outgoing completion: {marker}')
        cursor = position + len(marker)
    incoming = re.findall(r'GSM service uplink sapi=3 pd=09 message=01 length=5 data=8901020240', text)
    if len(incoming) != 1:
        raise ValueError('incoming SMS must complete exactly once, without fixture replay')
    if f'nsb7_sms_reply_done: t={70+delay}.' not in text:
        raise ValueError('one-shot check requires the complete 70-second observation')


def check_frame(path):
    from PIL import Image
    with Image.open(path) as image:
        if image.size != (84, 48) or hashlib.sha256(image.convert('RGB').tobytes()).hexdigest() != \
                '9ac0ef8ceeb981079a93cde45249a30256763d25119e5fcfae1218f05cf5c0b7':
            raise ValueError('Message sent frame differs')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('run', type=Path)
    parser.add_argument('--delay', type=int, default=0, help='physical input delay, not emulation timing')
    args = parser.parse_args()
    try:
        contract(args.image.read_bytes())
        check((args.run / 'error.log').read_text(), delay=args.delay)
        check_frame(args.run / f'snap/nsb7_sms_reply_final_{44.3+args.delay}.png')
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-7 outgoing SMS: FAIL: {error}\n')
    print('PASS: physical Reply, accepted submit, CP/RP ACK, release, Message sent and no incoming replay')


if __name__ == '__main__':
    main()

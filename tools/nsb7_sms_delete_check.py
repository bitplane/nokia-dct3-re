"""Physical NSB-7 SMS deletion, cancellation and cold persistence."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb7_radio_check import contract
from tools.nsb7_sms_check import check_cold, check_read_frame, tx_bytes
from tools.radio_sms_acceptance_common import FIRST_SMS_DELIVER_BODY, SMS_NVRAM_OFFSET, sms_record


def check_delete(text, card, source, cancel=False):
    record = sms_record(source)
    if '[LUA ERROR]' in text or record[0] != 1 or not record[1:].startswith(FIRST_SMS_DELIVER_BODY):
        raise ValueError('deletion needs a valid read hello message and a successful fixture')
    expected = bytearray(source)
    if not cancel:
        expected[SMS_NVRAM_OFFSET] = 0
    if card != bytes(expected):
        raise ValueError('erase/cancel changed unexpected card bytes')
    final_key = 'Back' if cancel else 'Menu'
    keys = re.findall(r'nsb7_sms_delete_input: key=(Menu|Back) state=([01]) t=([0-9.]+)', text)
    expected_keys = [(key, state, f'{time}.{fraction}')
                     for key, time in [('Menu', 28), ('Menu', 30), (final_key, 32)]
                     for state, fraction in [('1', '000000000'), ('0', '200000000')]]
    if keys != expected_keys:
        raise ValueError('erase confirmation/cancellation physical keys differ')
    writes = re.findall(r'sim_device: update fid=6f3c record=1 length=176 t=([0-9.]+)', text)
    if len(writes) != (2 if cancel else 3):
        raise ValueError('unexpected delivery/read/delete record-write count')
    if not (float(writes[0]) < 23 and 25 < float(writes[1]) < 28):
        raise ValueError('delivery/read writes must precede the erase dialogue')
    logical_delete = bytearray(record)
    logical_delete[0] = 0
    update = bytes.fromhex('a0dc0104b0') + bytes(logical_delete)
    wire = tx_bytes(text)
    if cancel and update in wire:
        raise ValueError('Back must not transmit a logical-delete write')
    if not cancel:
        if wire.count(update) != 1 or not 32 < float(writes[-1]) < 38:
            raise ValueError('logical deletion must be a physical SIM write after OK')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('read', type=Path)
    parser.add_argument('deleted', type=Path)
    parser.add_argument('cancelled', type=Path)
    parser.add_argument('cold', type=Path)
    args = parser.parse_args()
    try:
        contract(args.image.read_bytes())
        def card(run):
            return (run / 'nvram/nsb7hle/sim_card').read_bytes()
        source = card(args.read)
        deleted = card(args.deleted)
        check_read_frame(args.read / 'snap/nsb7_sms_read_25.png')
        check_delete((args.deleted / 'error.log').read_text(), deleted, source)
        check_delete((args.cancelled / 'error.log').read_text(), card(args.cancelled), source, cancel=True)
        check_cold((args.cold / 'error.log').read_text(), card(args.cold), deleted)
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-7 SMS deletion: FAIL: {error}\n')
    print('PASS: physical OK logically deletes; Back preserves; cold MCU reads unchanged deleted card')


if __name__ == '__main__':
    main()

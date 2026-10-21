"""Own NSB-7 incoming SMS delivery and cold SIM readback, not UI rendering."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb7_radio_check import PACKET, contract, check_location_update
from tools.radio_sms_acceptance_common import FIRST_SMS_DELIVER_BODY, SMS_NVRAM_OFFSET, sms_record

READ_FRAME_SHA256 = '73f302b41033281ec674b38bfb011a95857ddbd0b533b94a5520a821c9b36b4f'


def tx_bytes(text):
    return bytes(int(value, 16) for value in re.findall(
        r'nsb7_sim_tx: index=\d+ value=([0-9a-f]{2})', text))


def check_delivery(text, card):
    if '[LUA ERROR]' in text:
        raise ValueError('delivery fixture error')
    check_location_update(text.split('nsb7_radio_tx: index=46 ', 1)[0])
    rows = PACKET.findall(text)
    if [int(row[0]) for row in rows] != list(range(1, 84)):
        raise ValueError('own SMS TX inventory differs')
    def block(prefix):
        return prefix + '2b'*(25-len(bytes.fromhex(prefix))) + '000000'
    expected = {
        65: ('1b', block('00800f7301')),
        66: ('1b', block('00800f2101')),
        67: ('1b', block('00800f4101')),
        70: ('1b', block('00800d40098904')),
        71: ('1b', block('00800d42158901020240')),
        73: ('1b', block('0080036101')),
        74: ('02', '041202000000001a600002580000000f00000000'),
        78: ('57', '03050000'),
    }
    if any((rows[index-1][1], rows[index-1][3]) != value for index, value in expected.items()):
        raise ValueError('handset SAPI3/CP/RP acknowledgement or release differs')
    record = b'\x03' + FIRST_SMS_DELIVER_BODY
    record += b'\xff'*(176-len(record))
    if sms_record(card) != record:
        raise ValueError('unread hello SMS record differs')
    if card[1600:1611] != bytes.fromhex('ffffffff00f1100001ff00'):
        raise ValueError('accepted laboratory location was not retained')
    for number in range(2, 11):
        if sms_record(card, number) != b'\x00' + b'\xff'*175:
            raise ValueError('delivery overwrote another SMS record')
    wire = tx_bytes(text)
    if wire.count(bytes.fromhex('a0dc0104b0') + record) != 1:
        raise ValueError('expected one exact physical UPDATE RECORD for the delivered SMS')
    if 'GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2 ' not in text:
        raise ValueError('network CP-ACK is missing')


def check_cold(text, card, delivered):
    if '[LUA ERROR]' in text or card != delivered:
        raise ValueError('cold fixture failed or modified the delivered card')
    wire = tx_bytes(text)
    if b'\xa0\xdc' in wire or b'\xa0\xb2\x01\x04\xb0' not in wire:
        raise ValueError('cold SMS fixture must read record 1 without UPDATE RECORD')
    received = bytes(int(value, 16) for value in re.findall(
        r'nsb7_simi: kind=read offset=37 value=([0-9a-f]{2}) ', text))
    if sms_record(delivered) not in received:
        raise ValueError('MCU did not read the complete retained SMS record through SIMI')


def check_read(text, card, delivered, read_times=(23,25)):
    if '[LUA ERROR]' in text or sms_record(delivered)[0] != 3:
        raise ValueError('reading requires a successful fixture and an unread source record')
    expected = bytearray(delivered)
    expected[SMS_NVRAM_OFFSET] = 1
    if card != bytes(expected):
        raise ValueError('physical reading must change only the delivered record status to read')
    keys = re.findall(r'nsb7_sms_input: key=Menu state=([01]) t=([0-9.]+)', text)
    if keys != [(state, f'{second}.{fraction}') for second in read_times
                for state, fraction in [('1', '000000000'), ('0', '200000000')]]:
        raise ValueError('physical message-reading keys differ')
    wire = tx_bytes(text)
    unread = bytes.fromhex('a0dc0104b0') + sms_record(delivered)
    read = bytes.fromhex('a0dc0104b0') + sms_record(card)
    if wire.count(unread) != 1 or wire.count(read) != 1 or wire.index(unread) >= wire.index(read):
        raise ValueError('delivery then read-status UPDATE RECORD exchange differs')
    writes = re.findall(r'sim_device: update fid=6f3c record=1 length=176 t=([0-9.]+)', text)
    if len(writes) != 2 or not (float(writes[0]) < read_times[0] and read_times[-1] < float(writes[1]) < read_times[-1]+10):
        raise ValueError('read-status write must follow the physical reading keys')


def check_read_frame(path):
    from PIL import Image
    with Image.open(path) as source:
        pixels = source.convert('RGB')
        if pixels.size != (84, 48) or hashlib.sha256(pixels.tobytes()).hexdigest() != READ_FRAME_SHA256:
            raise ValueError('SMS hello/Options/Back pixel oracle differs')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('delivery', type=Path)
    parser.add_argument('cold', type=Path)
    parser.add_argument('--read', type=Path)
    parser.add_argument('--read-cold', type=Path)
    args = parser.parse_args()
    if bool(args.read) != bool(args.read_cold):
        parser.error('--read and --read-cold must be supplied together')
    try:
        contract(args.image.read_bytes())
        saved = (args.delivery / 'nvram/nsb7hle/sim_card').read_bytes()
        check_delivery((args.delivery / 'error.log').read_text(), saved)
        check_cold((args.cold / 'error.log').read_text(),
                   (args.cold / 'nvram/nsb7hle/sim_card').read_bytes(), saved)
        if args.read:
            read = (args.read / 'nvram/nsb7hle/sim_card').read_bytes()
            check_read((args.read / 'error.log').read_text(), read, saved)
            check_read_frame(args.read / 'snap/nsb7_sms_read_25.png')
            check_cold((args.read_cold / 'error.log').read_text(),
                       (args.read_cold / 'nvram/nsb7hle/sim_card').read_bytes(), read)
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-7 SMS: FAIL: {error}\n')
    print('PASS: handset CP/RP ACK, exact SIM SMS write and cold MCU readback' +
          ('; physical read/status persistence and hello pixel oracle' if args.read else '; no UI claim'))


if __name__ == '__main__':
    main()

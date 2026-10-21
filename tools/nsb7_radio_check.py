"""Check the bounded own NSB-7 no-radio-peer TX inventory, not RF completion."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb7_input_check import IMAGE_SHA1
from tools.nsb7_security_check import check_trace as check_security

PACKET = re.compile(r'nsb7_radio_tx: index=(\d+) type=([0-9a-f]{2}) length=(\d+) data=([0-9a-f]+) '
                    r'cursor=([0-9a-f]{2}) producer=([0-9a-f]{2}) pc=([0-9a-f]{8}) t=([0-9.]+) observer_writes=0')
TYPES = (5, 5, 5, 0x70, 0x70, 0x70, 0x70, 0x70, 0x3c,
         0x0d, 0x0d, 0x0d, 0x0d, 5, 0x0d, 0x0d, 0x0d, 0x0d, 0x70, 5,
         0x4c, 0x51, 0x51, 0x51, 0x51, 0x51, 0x51, 0x51, 0x55)
LENGTHS = (10, 8, 10, 6, 14, 22, 26, 2, 158, 66, 66, 66, 66, 10,
           66, 66, 66, 66, 2, 10, 4, 80, 80, 80, 80, 80, 80, 28, 4)


def channel_class(channel, features):
    """Decoded 2c7f48 branch order; class numbers are not physical band names."""
    if not 0 <= channel <= 0xffff:
        raise ValueError('requires a 16-bit channel')
    if features & (1 << 31) and 1 <= channel <= 124:
        return 1
    if features & (1 << 30) and (975 <= channel < 1024 or channel == 0):
        return 1
    if features & (1 << 29) and 955 <= channel <= 974:
        return 1
    if features & (1 << 28) and 512 <= channel < 886:
        return 2
    if features & (1 << 27) and 512 <= channel <= 810:
        return 3
    return 0


def contract(image):
    if hashlib.sha1(image).hexdigest() != IMAGE_SHA1:
        raise ValueError('requires own qualified image')
    def word(address):
        return int.from_bytes(image[address-0x200000:address-0x200000+4], 'big')
    if word(0x309f60 + 8*4) != 0x309fb4 or image[0xe526c:0xe526e] != bytes.fromhex('200d'):
        raise ValueError('own 8b dispatch/task-13 contract differs')
    if image[0xcb208:0xcb20a] != bytes.fromhex('3404'):
        raise ValueError('own RX payload prefix differs')
    if (word(0x2c8268), word(0x2c826c), word(0x2c8270), word(0x2c8378)) != (955, 974, 886, 810):
        raise ValueError('own channel thresholds differ')
    ranges = []
    start = 0
    previous = channel_class(0, 0x98000000)
    for channel in range(1, 65537):
        value = channel_class(channel, 0x98000000) if channel < 65536 else None
        if value != previous:
            ranges.append({'first': start, 'last': channel-1, 'class': previous})
            start, previous = channel, value
    return {'image_sha1': IMAGE_SHA1, 'rx_8b_table_entry': '309fb4', 'forwarder': '2e5258',
            'task': 13, 'payload_prefix_bytes': 4, 'feature_word': '98000000',
            'classified_inputs': 65536, 'ranges': ranges,
            'scope': 'decoded software classes, not fitted RF-band proof'}


def check_trace(text, measurement=False):
    check_security(text)
    rows = PACKET.findall(text)
    if len(rows) != text.count('nsb7_radio_tx:') or not rows or len(rows) >= 256:
        raise ValueError('TX capture malformed, absent or capped')
    if 'nsb7_radio_capture: invalid_extent' in text:
        raise ValueError('observer ring interpretation failed')
    cursor = 0
    inventory = []
    for index, row in enumerate(rows, 1):
        number, kind, length, payload, start, producer, pc, time = row
        size = int(length)
        if int(number) != index or len(payload) != size*2 or int(start, 16) != cursor or pc != '002cb0ec':
            raise ValueError('ordered packet geometry/publication differs')
        cursor = (cursor+(size+3)//2) % 0x52
        if int(producer, 16) != cursor:
            raise ValueError('packet publication extent differs')
        inventory.append((int(kind, 16), bytes.fromhex(payload)))
    expected_types = TYPES + ((0x56,) if measurement else ())
    expected_lengths = LENGTHS + ((160,) if measurement else ())
    if tuple(kind for kind, _ in inventory) != expected_types or tuple(len(payload) for _, payload in inventory) != expected_lengths:
        raise ValueError('bounded packet inventory differs')
    if inventory[28] != (0x55, bytes.fromhex('03050000')):
        raise ValueError('initial own scan-control request differs')
    if measurement:
        if inventory[29][1] != bytes.fromhex('0258' + 'ff'*158):
            raise ValueError('organic candidate request differs from the single-cell candidate list')
        objects = re.findall(r'nsb7_candidate_object: index=1 type=56 length=160 words=([0-9a-f,]+) ', text)
        if len(objects) != 1 or objects[0].split(',') != ['0258'] + ['ffff']*79:
            raise ValueError('pre-transport candidate object is missing or differs')
        replies = re.findall(r'nsb7_radio_rx: index=(\d+) type=([0-9a-f]{2}) length=(\d+) ', text)
        # Two existing external-service 8e frames precede the sole RF reply.
        if replies != [('1', '8e', '10'), ('2', '8e', '10'), ('3', '8b', '166')]:
            raise ValueError('measurement-only RX scope differs')
        consumers = re.findall(r'nsb7_measurement: pc=([0-9a-f]{8}) task=(\d+) type=([0-9a-f]{2}) first=([0-9a-f]{4}) '
                               r'second=([0-9a-f]{4}) rssi=(-?\d+) ', text)
        if consumers != [('002e5258', '4', '8b', '0258', 'ffff', '-60'),
                         ('002a3704', '13', '8b', '0258', 'ffff', '-60')]:
            raise ValueError('PCS measurement did not reach the own broker and task-13 consumer')
        if 'nsb7_measurement_class: channel=0258 class=2 ' not in text:
            raise ValueError('own carrier classifier did not run')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('run', type=Path)
    parser.add_argument('--contract-json', type=Path)
    parser.add_argument('--measurement', action='store_true')
    args = parser.parse_args()
    try:
        report = contract(args.image.read_bytes())
        text = (args.run / 'error.log').read_text()
        check_trace(text, args.measurement)
        states = re.findall(r'nsb7_radio_state: bands=([0-9a-f]{8}) measurement_route=([0-9a-f]{2}) t=([0-9.]+)', text)
        if states != [('98000000', '01', f'{time:.9f}') for time in (3.5, 6, 14, 24)]:
            raise ValueError('live feature/forwarding predicate differs')
        if args.contract_json:
            args.contract_json.write_text(json.dumps(report, indent=2) + '\n')
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-7 radio inventory: FAIL: {error}\n')
    print('PASS: ' + ('PCS RSSI consumed by task 13; organic 56 request left unanswered' if args.measurement else
                     'own 29-packet no-peer TX baseline; final 55/03050000') + '; no RF completion claim')


if __name__ == '__main__':
    main()

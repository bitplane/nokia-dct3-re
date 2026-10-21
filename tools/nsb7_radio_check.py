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

PACKET = re.compile(r'nsb7_radio_tx: index=(\d+) type=([0-9a-f]{2}) length=(\d+) data=([0-9a-f]*) '
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
    dispatch = [word(0x309f60 + index*4) for index in range(13)]
    expected_dispatch = [0x309fec, 0x309fe4, 0x30a006, 0x309fdc,
                         0x309fd4, 0x309fcc, 0x309fc4, 0x309fbc,
                         0x309fb4, 0x309fac, 0x30a006, 0x30a006, 0x309fa4]
    if dispatch != expected_dispatch:
        raise ValueError('own complete 83..8f RX dispatch differs')
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
            'rx_dispatch': {f'{0x83+index:02x}': f'{target:08x}'
                            for index, target in enumerate(dispatch)},
            'rx_8a': {'forwarder': '002e4fdc', 'gate': '00130784 == 1', 'task': 13},
            'rx_80': {'dispatcher': '002e4ef4', 'gate': '00130784 == 1',
                      'subtype_payload_offset': 0, 'status_payload_offset': 2,
                      'frame_payload_offsets': [3, 4, 5],
                      'subtype40_gate_address': '00130784',
                      'candidate_parser': '00287574',
                      'candidate_channel_payload_offsets': [6, 7],
                      'candidate_update': '0028734c',
                      'success_record_state': 4,
                      'sch_bsic_payload_offset': 10,
                      'bcch_forwarder': '002e4e96', 'bcch_task': 17,
                      'bcch_tag': '07ff', 'bcch_parser': '0029d42c',
                      'bcch_block_payload_offset': 10, 'bcch_block_bytes': 24,
                      'si_state': '0012d790', 'complete_si_bitmap': 15,
                      'subtype_handlers': {'40': '002e4f8a', '50': '002e4e96',
                                           '60': '002e4d24', '70': '002e4b86',
                                           '80': '002e49e6', 'a0': '002e4f52',
                                           'b0': '002e4f4c'}},
            'task': 13, 'payload_prefix_bytes': 4, 'feature_word': '98000000',
            'classified_inputs': 65536, 'ranges': ranges,
            'scope': 'decoded software classes, not fitted RF-band proof'}


def check_trace(text, measurement=False, synchronization=False, channel=False, bcch=False):
    channel = channel or bcch
    synchronization = synchronization or channel
    measurement = measurement or synchronization
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
    if synchronization:
        expected_types += (0x02,)
        expected_lengths += (20,)
    if bcch:
        expected_types += (0x02,)
        expected_lengths += (20,)
    elif channel:
        expected_types += (0x03, 0x55)
        expected_lengths += (0, 4)
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
        contexts = re.findall(r'nsb7_candidate_context: route=01 root08=00000000 root0c=([0-9a-f]{8}) '
                              r'root18=([0-9a-f]{8}) root28=([0-9a-f]{8}) ', text)
        records = re.findall(r'nsb7_candidate_record: address=([0-9a-f]{8}) channel=0258 flags=a2 ', text)
        if (len(contexts) != 1 or len(records) != 1 or
                contexts[0] != (records[0],)*3 or not 0x100000 <= int(records[0], 16) < 0x17fff0):
            raise ValueError('candidate context does not contain the sole measured carrier')
        replies = re.findall(r'nsb7_radio_rx: index=(\d+) type=([0-9a-f]{2}) length=(\d+) ', text)
        # Two existing external-service 8e frames precede the sole RF reply.
        expected_replies = [('1', '8e', '10'), ('2', '8e', '10'), ('3', '8b', '166')]
        if synchronization:
            expected_replies.append(('4', '80', '14'))
        if channel:
            expected_replies.append(('5', '89', '8'))
        if bcch:
            expected_replies.extend((str(index), '80', '34') for index in range(6, 14))
        if replies != expected_replies:
            raise ValueError('qualified acquisition RX scope differs')
        consumers = re.findall(r'nsb7_measurement: pc=([0-9a-f]{8}) task=(\d+) type=([0-9a-f]{2}) first=([0-9a-f]{4}) '
                               r'second=([0-9a-f]{4}) rssi=(-?\d+) ', text)
        if consumers != [('002e5258', '4', '8b', '0258', 'ffff', '-60'),
                         ('002a3704', '13', '8b', '0258', 'ffff', '-60')]:
            raise ValueError('PCS measurement did not reach the own broker and task-13 consumer')
        if 'nsb7_measurement_class: channel=0258 class=2 ' not in text:
            raise ValueError('own carrier classifier did not run')
    if synchronization:
        if inventory[30][1][:12] != bytes.fromhex('041202000000005050000258'):
            raise ValueError('organic channel request does not select the synchronized carrier')
        sch = re.findall(r'nsb7_sch: pc=([0-9a-f]{8}) task=(\d+) selected=([0-9a-f]{8}) flags=([0-9a-f]{2}) '
                         r'payload=([0-9a-f]+) ', text)
        if len(sch) != 3 or [(row[0], row[1]) for row in sch] != [
                ('002e4ef4', '4'), ('00287574', '13'), ('0028734c', '13')]:
            raise ValueError('SCH did not traverse the own broker, lookup and candidate update')
        payloads = [bytes.fromhex(row[4]) for row in sch]
        if (len(set(payloads)) != 1 or len(payloads[0]) != 14 or
                payloads[0][:3] != bytes.fromhex('401200') or
                payloads[0][6:8] != bytes.fromhex('0258') or payloads[0][10] >> 2 != 0x12):
            raise ValueError('SCH payload channel, status or decoded BSIC differs')
        if 'nsb7_sch_candidate_commit: channel=0258 flags=a4 bsic=12 ' not in text:
            raise ValueError('firmware did not settle the synchronized candidate record')
    if channel:
        if not bcch and inventory[31:] != [(0x03, b''), (0x55, bytes.fromhex('04080000'))]:
            raise ValueError('unanswered BCCH timeout tail differs')
        if 'nsb7_channel_cached: descriptor=00000000 data=045000000000120002580000 live_channel=0000 live_mode=00 ' not in text:
            raise ValueError('own receiver configuration was not cached before acknowledgement')
        if not re.search(r'nsb7_channel_ack: pc=002e5054 task=4 type=89 bit=00 cached=00[1-7][0-9a-f]{5} channel=0000 mode=00 bsic=00 ', text):
            raise ValueError('channel acknowledgement did not reach the own pending-config consumer')
        if 'nsb7_channel_state: cached=00000000 channel=0258 mode=50 bsic=12 t=6.000000000 ' not in text:
            raise ValueError('channel acknowledgement did not commit the own receiver state')
    if bcch:
        if inventory[31][1][:12] != bytes.fromhex('041202090000001060000258'):
            raise ValueError('complete SI did not yield the own CCCH receiver request')
        rows = re.findall(r'nsb7_bcch: pc=([0-9a-f]{8}) task=(\d+) tag=([0-9a-f]{4}) '
                          r'mode=50 error=00 channel=0258 block=([0-9a-f]{48}) flags=[0-9a-f]{2} ', text)
        broker = [row[3] for row in rows if row[:2] == ('002e4e96', '4')]
        consumer = [row[3] for row in rows if row[:3] == ('0029d42c', '17', '07ff')]
        if len(rows) != 16 or len(broker) != 8 or broker != consumer:
            raise ValueError('BCCH blocks did not traverse the own task-17 consumer unchanged')
        blocks = [bytes.fromhex(block) for block in consumer]
        if any(block[1] != 6 for block in blocks) or {block[2] for block in blocks} != {0x19, 0x1a, 0x1b, 0x1c}:
            raise ValueError('own consumer did not receive the complete RR system-information family')
        if any(block[3:10] != bytes.fromhex('000100f1100001') for block in blocks if block[2] == 0x1b):
            raise ValueError('SI3 laboratory cell identity or LAI differs')
        if 'nsb7_bcch_flags: flags=0f pc=0029d5ae ' not in text:
            raise ValueError('own SI parser did not complete its bitmap')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('run', type=Path)
    parser.add_argument('--contract-json', type=Path)
    parser.add_argument('--measurement', action='store_true')
    parser.add_argument('--synchronization', action='store_true')
    parser.add_argument('--channel', action='store_true')
    parser.add_argument('--bcch', action='store_true')
    args = parser.parse_args()
    try:
        report = contract(args.image.read_bytes())
        text = (args.run / 'error.log').read_text()
        check_trace(text, args.measurement, args.synchronization, args.channel, args.bcch)
        states = re.findall(r'nsb7_radio_state: bands=([0-9a-f]{8}) measurement_route=([0-9a-f]{2}) t=([0-9.]+)', text)
        if states != [('98000000', '01', f'{time:.9f}') for time in (3.5, 6, 14, 24)]:
            raise ValueError('live feature/forwarding predicate differs')
        if args.contract_json:
            args.contract_json.write_text(json.dumps(report, indent=2) + '\n')
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-7 radio inventory: FAIL: {error}\n')
    print('PASS: ' + ('own SI bitmap complete; organic CCCH request left unanswered' if args.bcch else
                     'own channel acknowledgement commits receiver; no BCCH replies' if args.channel else
                     'own SCH accepted; organic channel request left unanswered' if args.synchronization else
                     'PCS RSSI consumed by task 13; organic 56 request left unanswered' if args.measurement else
                     'own 29-packet no-peer TX baseline; final 55/03050000') + '; no RF completion claim')


if __name__ == '__main__':
    main()

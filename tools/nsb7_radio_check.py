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
    ccch_endings = list(image[0x143fb4:0x143fbd])
    if ccch_endings != [9, 15, 19, 25, 29, 35, 39, 45, 49] or word(0x2e4d04) != 0x623:
        raise ValueError('own AGCH tag/block-ending contract differs')
    if word(0x2e52a0) != 0x409 or image[0xe506e:0xe5072] != bytes.fromhex('4291d02d'):
        raise ValueError('own 0409 receiver-cache bypass contract differs')
    if (image[0xb6a1e:0xb6a24] != bytes.fromhex('b5701c0d1c06') or
            image[0xb6a3c:0xb6a40] != bytes.fromhex('205770e0') or
            image[0xb6a4c:0xb6a50] != bytes.fromhex('f7fffdd3')):
        raise ValueError('own background request constructor/selector encoder differs')
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
            'rx_84': {'forwarder': '002e4fa4', 'task': 13, 'parser': '002f5274',
                      'ra_payload_offset': 0, 'frame_payload_offsets': [1, 2, 3],
                      'request_reference_tag': '07f9', 'request_reference_task': 17},
            'rx_89': {'handler': '002e5054', 'dedicated_bit': 1,
                      'queued_descriptor_tag': '0402', 'armed_clear_pc': '002e5086'},
            'rx_86': {'dispatcher': '002e5228', 'dedicated_handler': '002e513e',
                      'dedicated_uplink_queue': '00130434', 'selector': '80'},
            'tx_57': {'constructor': '002b6a1e', 'selector_encoder': '002b65f6',
                      'payload_bytes': 4, 'qualified_request': '03050000',
                      'response_type': '8b', 'response_bytes': 166},
            'contention': {'handler': '002664be', 'state_offset': 0x163,
                           'pending_length_offset': 0x12e, 'pending_pointer_offset': 0x130,
                           'received_information_offset': 0x1a, 'state_write': '002665c0',
                           'before': 2, 'after': 4},
            'tx_0c': {'constructor': '002b6e58', 'payload_bytes': 8,
                      'timing_mode_offset': 1, 'ra_offset': 2, 'slot_delay_offset': 3,
                      'scheduled_base_offsets': [4, 5], 'coverage': 'non-combined CCCH only'},
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
                      'agch_forwarder': '002e49e6', 'agch_task': 14,
                      'agch_tag': '0623', 'agch_decoder': '002645ec',
                      'agch_decoded_result': '061a',
                      'ccch_endings_table': '00343fb4', 'ccch_block_endings': ccch_endings,
                      'subtype_handlers': {'40': '002e4f8a', '50': '002e4e96',
                                           '60': '002e4d24', '70': '002e4b86',
                                           '80': '002e49e6', 'a0': '002e4f52',
                                           'b0': '002e4f4c'}},
            'task': 13, 'payload_prefix_bytes': 4, 'feature_word': '98000000',
            'classified_inputs': 65536, 'ranges': ranges,
            'scope': 'decoded software classes, not fitted RF-band proof'}


def check_rach(text, inventory, assignment=False):
    requests = [payload for kind, payload in inventory if kind == 0x0c]
    deadlines = re.findall(r'access_deadline ra=([0-9a-f]{2}) mode=(\d+) frame=(\d+) anchor=(\d+) '
                           r'base=(\d+) slots=(\d+) deadline=(\d+) ', text)
    received = re.findall(r'nsb7_rach: pc=([0-9a-f]{8}) task=(\d+) type=84 payload=([0-9a-f]{16}) ', text)
    broker = [bytes.fromhex(payload) for pc, task, payload in received if (pc, task) == ('002e4fa4', '4')]
    consumer = [bytes.fromhex(payload) for pc, task, payload in received if (pc, task) == ('002f5274', '13')]
    references = re.findall(r'nsb7_rach_reference: index=(\d+) task=13 target=17 tag=07f9 '
                            r't1=([0-9a-f]{2}) t3=([0-9a-f]{2}) t2=([0-9a-f]{2}) ra=([0-9a-f]{2}) ', text)
    count = 1 if assignment else 4
    request_count = 2 if assignment else 4
    if len(requests) != request_count or len(deadlines) != count or len(received) != count*2 or broker != consumer or len(broker) != count or len(references) != count:
        raise ValueError('random-access request/completion/reference inventory differs')
    previous_frame = None
    for index, (request, scheduling, completion, reference) in enumerate(zip(requests, deadlines, consumer, references), 1):
        ra, mode, frame, anchor, base, slots, deadline = scheduling
        mode, frame, anchor, base, slots, deadline = map(int, (mode, frame, anchor, base, slots, deadline))
        if (int(ra, 16), mode, base, slots) != (request[2], request[1], int.from_bytes(request[4:6], 'big'), request[3]):
            raise ValueError('access deadline was not derived from the own request')
        if mode == 0:
            expected = frame + slots
            if anchor != frame:
                raise ValueError('relative access delay has wrong anchor')
        elif mode == 1 and previous_frame is not None:
            delta = (base - (previous_frame % 2715648 & 0xffff)) & 0xffff
            expected = previous_frame + delta + slots
            if anchor != previous_frame or delta >= 0x8000:
                raise ValueError('scheduled retry lost its previous-access anchor')
        else:
            raise ValueError('unsupported access timing mode')
        if deadline != expected:
            raise ValueError('access deadline arithmetic differs')
        actual = int.from_bytes(completion[1:4], 'big')
        if completion[0] != request[2] or completion[4:] != bytes(4) or not 0 <= actual-deadline <= 2:
            raise ValueError('access completion is premature, uncorrelated or outside the HLE polling allowance')
        expected_reference = (index, (actual//1326)&31, actual%51, actual%26, request[2])
        got_reference = (int(reference[0]), *(int(value, 16) for value in reference[1:]))
        if got_reference != expected_reference:
            raise ValueError('firmware task-17 reference does not match the completed access frame')
        previous_frame = actual


def check_assignment(text, inventory, dedicated=False):
    check_rach(text, inventory, assignment=True)
    forward = re.findall(r'nsb7_agch_forward: index=1 task=4 mode=60 error=00 frame=(\d+) block=([0-9a-f]{48}) ', text)
    consumer = re.findall(r'nsb7_agch_consumer: index=1 task=14 tag=0623 mode=60 block=([0-9a-f]{48}) ', text)
    result = re.findall(r'nsb7_rr_decoder_result: index=1 task=14 code=061a block=([0-9a-f]{48}) ', text)
    if len(forward) != 1 or consumer != [forward[0][1]] or result != consumer:
        raise ValueError('own AGCH route did not preserve the block through task-14 decoder result 061a')
    block = bytes.fromhex(consumer[0])
    if block[:7] != bytes.fromhex('2d063f00200258'):
        raise ValueError('Immediate Assignment does not select the lab SDCCH carrier')
    completed = re.findall(r'nsb7_rach: pc=002f5274 task=13 type=84 payload=([0-9a-f]{16}) ', text)
    payload = bytes.fromhex(completed[0])
    access_frame = int.from_bytes(payload[1:4], 'big')
    reference = (block[7], block[8] >> 3, ((block[8]&7)<<3) | (block[9]>>5), block[9]&31)
    if reference != (payload[0], (access_frame//1326)&31, access_frame%51, access_frame%26):
        raise ValueError('assignment reference does not identify the completed access')
    frame = int(forward[0][0])
    endings = (9, 15, 19, 25, 29, 35, 39, 45, 49)
    expected = next(number for number in range(access_frame+4, access_frame+15) if number%51 in endings)
    if frame != expected:
        raise ValueError('AGCH metadata is not the next eligible decoded block after RACH')
    if f'assignment_deadline access_frame={access_frame} deadline={frame}' not in text:
        raise ValueError('assignment scheduling evidence missing')
    if inventory[39:41] != [(0x1f, bytes.fromhex('00090000')),
                           (0x02, bytes.fromhex('0412000000000040800002584000000000000000'))]:
        raise ValueError('own assignment did not produce the dedicated-channel request')
    if not re.search(r'nsb7_channel_pending: address=00[1-7][0-9a-f]{5} tag=0402 bit=01 armed=01 ', text):
        raise ValueError('dedicated request did not arm its own bit-1 descriptor')
    if not dedicated and 'nsb7_channel_state: cached=00000000 channel=0258 mode=1a bsic=12 t=24.000000000 ' not in text:
        raise ValueError('unacknowledged dedicated request unexpectedly committed receiver state')


def check_dedicated(text, inventory, uplink=False):
    descriptor = re.search(r'nsb7_channel_pending: address=([0-9a-f]{8}) tag=0402 bit=01 armed=01 ', text)
    cleared = re.search(r'nsb7_dedicated_armed: address=([0-9a-f]{8}) value=00 pc=002e5086 task=4 ', text)
    if not descriptor or not cleared or descriptor[1] != cleared[1]:
        raise ValueError('dedicated acknowledgement did not clear the own armed descriptor')
    if not re.search(r'nsb7_channel_ack: pc=002e5054 task=4 type=89 bit=01 cached=00000000 channel=0258 mode=1a bsic=12 ', text):
        raise ValueError('dedicated bit-1 reply did not reach the own handler')
    for time in (14, 24):
        if f'nsb7_channel_state: cached=00000000 channel=0258 mode=40 bsic=12 t={time}.000000000 ' not in text:
            raise ValueError('dedicated receiver mode 40 did not settle')
    if not uplink:
        return
    rows = re.findall(r'nsb7_block_request: pc=([0-9a-f]{8}) task=4 selector=([0-9a-f]{2}) queue=([0-9a-f]{8}) ', text)
    if (len(rows) != 2 or [(row[0], row[1]) for row in rows] != [('002e5228', '80'), ('002e513e', '00')] or
            rows[0][2] != rows[1][2] or not 0x100000 <= int(rows[0][2], 16) < 0x17ff00):
        raise ValueError('block request did not select the own nonempty dedicated queue')
    frame = inventory[41][1]
    if frame[:5] != bytes.fromhex('0080013f49') or frame[4] >> 2 != 18:
        raise ValueError('own uplink is not an eighteen-byte LAPDm SABM establishment')
    information = frame[5:23]
    if information != bytes.fromhex('05087000f000fffe20080910101032547698'):
        raise ValueError('own Location Updating Request, classmark or subscriber identity differs')
    if frame[23:] != bytes.fromhex('2b2b000000'):
        raise ValueError('own SEND_BLOCK padding/trailer differs')


def check_contention(text, inventory):
    information = inventory[41][1][5:23]
    expected = bytes.fromhex('017349') + information + bytes.fromhex('2b2b2b')
    rows = re.findall(r'nsb7_lapdm_downlink: pc=([0-9a-f]{8}) task=(\d+) selector=80 block=([0-9a-f]{48}) ', text)
    if ([(row[0], row[1]) for row in rows] != [('002e49e6', '4'), ('002645ec', '14')] or
            any(bytes.fromhex(row[2]) != expected for row in rows)):
        raise ValueError('contention UA did not echo the own SABM through both native consumers')
    compare = re.findall(r'nsb7_ua_compare: context=([0-9a-f]{8}) state=2 length=18 sent=([0-9a-f]{36}) '
                         r'received=([0-9a-f]{36}) ', text)
    states = re.findall(r'nsb7_lapdm_state: context=([0-9a-f]{8}) state=(\d+) pc=([0-9a-f]{8}) task=(\d+) ', text)
    if (len(compare) != 1 or not 0x100000 <= int(compare[0][0], 16) < 0x17fe00 or
            states != [(compare[0][0], '4', '002665c0', '14')] or
            compare[0][1:] != (information.hex(), information.hex())):
        raise ValueError('firmware did not compare contention data and advance link state 2 to 4')
    result = re.findall(r'nsb7_rr_decoder_result: index=3 task=14 code=0614 block=([0-9a-f]{48}) ', text)
    if result != [expected.hex()]:
        raise ValueError('UA decoder result differs')


def check_location_update(text, first_ack=43, request=None):
    """Qualify the post-UA exchange only; this is not a registration oracle."""
    blocks = re.findall(r'nsb7_lapdm_downlink: pc=([0-9a-f]{8}) task=(\d+) selector=80 block=([0-9a-f]{48}) ', text)
    request = request if request is not None else bytes.fromhex('05087000f000fffe20080910101032547698')
    if len(request) != 18 or request[:2] != bytes.fromhex('0508'):
        raise ValueError('location-update request must have the own MM layout')
    expected = [
        (bytes.fromhex('017349') + request + b'\x2b'*3).hex(),
        '030045050200f1100001170809101010325476982b2b2b2b',
        '03020d060d002b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b',
    ]
    if blocks != [(pc, task, block) for block in expected
                  for pc, task in [('002e49e6', '4'), ('002645ec', '14')]]:
        raise ValueError('location-update/release blocks did not reach both MCU consumers')
    indices = '|'.join(str(first_ack+i) for i in range(3))
    transmissions = re.findall(r'nsb7_radio_tx: index=('+indices+r') type=([0-9a-f]{2}) length=(\d+) data=([0-9a-f]+) ', text)
    if transmissions != [
            (str(first_ack), '1b', '28', '0080032101' + '2b'*20 + '000000'),
            (str(first_ack+1), '1b', '28', '0080034101' + '2b'*20 + '000000'),
            (str(first_ack+2), '02', '20', '041202000000001a600002580000000f00000000')]:
        raise ValueError('firmware acknowledgement or receiver-return request differs')
    states = re.findall(r'nsb7_lapdm_state: context=([0-9a-f]{8}) state=(\d+) pc=([0-9a-f]{8}) task=(\d+) ', text)
    if (len(states) != 3 or len({row[0] for row in states}) != 1 or
            [row[1:] for row in states] != [('4', '002665c0', '14'),
                                           ('3', '0026644a', '14'),
                                           ('0', '00265674', '14')]):
        raise ValueError('firmware link release state sequence differs')


def check_receiver_return(text, card, paging=False):
    check_location_update(text.split('nsb7_radio_tx: index=46 ', 1)[0])
    packets = [(int(row[0]), int(row[1], 16), bytes.fromhex(row[3]))
               for row in PACKET.findall(text)]
    expected = [(46, 0x46, bytes.fromhex('3210321000010000')),
                (47, 2, bytes.fromhex('0412020200000016600002580000000001297000')),
                (48, 2, bytes.fromhex('0412020200000016600002580000000001297000')),
                (49, 0x57, bytes.fromhex('03050000'))]
    if packets[45:] != expected:
        raise ValueError('post-release receiver sequence differs')
    replies = re.findall(r'nsb7_radio_rx: index=(2[6-9]|[3-9][0-9]) type=([0-9a-f]{2}) length=(\d+) ', text)
    if replies[:3] != [('26', '89', '8'), ('27', '89', '8'), ('28', '89', '8')]:
        raise ValueError('receiver return needs exactly three confirmations, no paging payload')
    if not paging and len(replies) != 3:
        raise ValueError('unexpected traffic after bounded receiver return')
    if 'live_channel=0258 live_mode=1a ' not in text or 'live_channel=0258 live_mode=16 ' not in text:
        raise ValueError('common-control receiver was not committed')
    # Native card NVRAM stores 50 32-byte ADN records before the 11-byte LOCI.
    if len(card) < 1611 or card[1600:1611] != bytes.fromhex('ffffffff00f1100001ff00'):
        raise ValueError('card did not retain laboratory LAI and updated LOCI status')
    wire = bytes(int(value, 16) for value in re.findall(
        r'nsb7_sim_tx: index=\d+ value=([0-9a-f]{2})', text))
    if bytes.fromhex('a0a40000026f7ea0d600040500f1100001a0d6000a0100') not in wire:
        raise ValueError('LOCI update was not observed on the physical SIM TX path')
    if paging:
        check_paging(text)


def check_paging(text):
    empty = '1506210001f0' + '2b'*18
    forwarded = re.findall(r'nsb7_agch_forward: index=\d+ task=4 mode=60 error=00 frame=(\d+) block=([0-9a-f]{48}) ', text)
    consumed = re.findall(r'nsb7_agch_consumer: index=\d+ task=14 tag=0623 mode=60 block=([0-9a-f]{48}) ', text)
    # The earlier Immediate Assignment uses the same CCCH route, not PCH.
    forwarded = [row for row in forwarded if row[1][2:6] == '0621']
    consumed = [block for block in consumed if block[2:6] == '0621']
    if (len(forwarded) < 8 or consumed != [row[1] for row in forwarded] or
            any(row[1] != empty for row in forwarded)):
        raise ValueError('two cycles of empty paging blocks did not reach both MCU consumers')
    frames = [int(row[0]) for row in forwarded]
    # This proves the monitored group, not complete physical-time delivery:
    # calibrated HLE cadence can skip a two-multiframe receive opportunity.
    if any((later-earlier) % 2715648 == 0 or
           ((later-earlier) % 2715648) % 102 != 0
           for earlier, later in zip(frames, frames[1:])):
        raise ValueError('subscriber paging multiframe sequence differs')
    decoded = re.findall(r'nsb7_rr_decoder_result: index=\d+ task=14 code=061a block=([0-9a-f]{48}) ', text)
    if decoded.count(empty) != len(forwarded):
        raise ValueError('empty paging blocks did not pass the RR decoder')


def check_addressed_page(text):
    """Prove the own IMSI match and parser event, not a Paging Response."""
    page = '31062110080910101032547698' + '2b'*11
    if not re.search(r'nsb7_agch_consumer: index=\d+ task=14 tag=0623 mode=60 block=' + page + r' ', text):
        raise ValueError('addressed paging block did not reach the MCU consumer')
    if not re.search(r'nsb7_page_post: index=\d+ source=14 target=17 lr=00264591 data=05eb', text):
        raise ValueError('decoded page was not forwarded to task 17')
    checks = [
        r'pc=00274124 r0=00000017 r2=00000009 data=080910101032547698 ',
        r'pc=002741dc r0=00000001 r2=00000000 data=080910101032547698 stack=[0-9a-f]{8}080910101032547698 ',
        r'pc=002741f6 r0=00000000 ',
        r'pc=00274e82 r0=00000001 ',
        r'pc=002b3130 r0=0000083e ',
    ]
    positions = []
    for pattern in checks:
        match = re.search(r'nsb7_page_identity: ' + pattern, text)
        if match is None:
            raise ValueError('own paging identity comparison/parser result is missing')
        positions.append(match.start())
    if positions != sorted(positions):
        raise ValueError('paging identity evidence order differs')
    returned = re.findall(r'nsb7_page_event_return: event=083e caller=00260c2b record=([0-9a-f]{8}) ', text)
    dispatch = re.findall(r'nsb7_page_dispatch: pc=([0-9a-f]{8}) event=083e record=([0-9a-f]{8}) flag0b=([0-9a-f]{2}) saved=([0-9a-f]{8}) ', text)
    if (len(returned) != 1 or len(dispatch) != 2 or
            dispatch[0][0] != '00260c4c' or dispatch[1][0] != '00260c16' or
            dispatch[0][1] != dispatch[1][1] or
            dispatch[0][2] != '00' or dispatch[1][2:] != ('01', '00000000') or
            not 0x100000 <= int(dispatch[0][3], 16) < 0x180000):
        raise ValueError('explicit page-event continuation did not release its saved record')
    readiness = re.findall(r'nsb7_page_dispatch: pc=00260c(?:4c|16) [^\n]*flag03=([0-9a-f]{2}) flag06=([0-9a-f]{2}) api1=([0-9a-f]{2}) ', text)
    if readiness != [('00', '01', '01'), ('00', '01', '01')]:
        raise ValueError('own paging-readiness predicates differ')
    if (not re.search(r'nsb7_page_post: index=\d+ source=18 target=16 lr=0022e323 data=09c8', text) or
            not re.search(r'nsb7_page_post: index=\d+ source=16 target=17 lr=00200e39 data=07d4', text) or
            'nsb7_rr_flag06_trigger: event=07d4 report=03ec ' not in text or
            not re.search(r'nsb7_rr_flag06: value=01 pc=00260a48 task=17 ', text)):
        raise ValueError('firmware-owned readiness-request chain is missing')
    completions = re.findall(r'nsb7_9c8_completion: event=([0-9a-f]{4}) record=([0-9a-f]{8}) ', text)
    reports = re.findall(r'nsb7_service_completion: pending=([0-9a-f]{8}) tag=03ec ', text)
    submitted = re.findall(r'nsb7_page_post: index=\d+ source=17 target=13 lr=002b29a1 data=03ec', text)
    callers = re.findall(r'nsb7_9c8_source: caller=00232057 task=18 ', text)
    if ([row[0] for row in completions] != ['1584', '1599'] or
            completions[0][1] != completions[1][1] or len(reports) != 1 or
            len(submitted) != 2 or len(callers) != 2):
        raise ValueError('bounded first-completion/post-release-timeout contract differs')
    packets = PACKET.findall(text)
    if len(packets) != 49:
        raise ValueError('bounded page-parser fixture unexpectedly advanced to access')


def check_page_response(text, card):
    check_location_update(text.split('nsb7_radio_tx: index=46 ', 1)[0])
    packets = PACKET.findall(text)
    if len(packets) != 69:
        raise ValueError('bounded paging-response TX inventory differs')
    expected = '0080013f41062707032058810809101010325476982b2b2b2b000000'
    if packets[60][1:4] != ('1b', '28', expected):
        raise ValueError('own SABM does not contain the qualified Paging Response')
    checks = [
        r'nsb7_radio_tx: index=49 type=57 length=4 data=03050000 ',
        r'nsb7_radio_rx: index=29 type=8b length=166 ',
        r'nsb7_service_completion: pending=[0-9a-f]{8} tag=03ec caller=00220deb ',
        r'nsb7_9c8_completion: event=1584 record=[0-9a-f]{8} ',
        r'nsb7_radio_tx: index=57 type=0c length=8 data=',
        r'nsb7_radio_tx: index=61 type=1b length=28 data=',
    ]
    # Skip the first, pre-registration completion; require the later one
    # after the actual background request/reply, not just global counters.
    position = 0
    for pattern in checks:
        match = re.search(pattern, text[position:])
        if match is None:
            raise ValueError('background measurement to paging-response chain is incomplete')
        position += match.end()
    ua = '01734106270703205881080910101032547698' + '2b'*5
    for pc, task in [('002e49e6', '4'), ('002645ec', '14')]:
        if not re.search(r'nsb7_lapdm_downlink: pc=' + pc + ' task=' + task +
                         ' selector=80 block=' + ua + ' ', text):
            raise ValueError('Paging Response contention echo did not reach both MCU consumers')
    if 'nsb7_9c8_completion: event=1599 ' in text:
        raise ValueError('service request still timed out')
    if card[1600:1611] != bytes.fromhex('ffffffff00f1100001ff00'):
        raise ValueError('accepted laboratory location was not retained')


def check_incoming_call_setup(text, card, traffic=False, startup_offset=0, request=None):
    check_location_update(text.split(f'nsb7_radio_tx: index={46+startup_offset} ', 1)[0],
                          first_ack=43+startup_offset, request=request)
    rows = PACKET.findall(text)
    if [int(row[0]) for row in rows] != list(range(1, len(rows)+1)):
        raise ValueError('call packet inventory is reordered, missing or duplicated')
    packets = {int(row[0]): (row[1], row[3]) for row in rows}
    expected = {
        61: ('1b', '0080013f41062707032058810809101010325476982b2b2b2b000000'),
        64: ('1b', '00800140090632' + '2b'*18 + '000000'),
        67: ('1b', '008001622d8308040460020180150101' + '2b'*9 + '000000'),
        68: ('1b', '00800184098341' + '2b'*18 + '000000'),
        70: ('02', '041202000271012fc10002580000000400000000'),
    }
    if any(packets.get(index+startup_offset) != value for index, value in expected.items()):
        raise ValueError('own paging/cipher-complete/call-confirmed/alerting/TCH request differs')
    setup = '032445030504046002008134015c0581551532f4' + '2b'*4
    for pc, task in [('002e49e6', 4), ('002645ec', 14)]:
        if f'nsb7_lapdm_downlink: pc={pc} task={task} selector=80 block={setup} ' not in text:
            raise ValueError('incoming SETUP did not reach both MCU consumers')
    if not re.search(r'nsb7_channel_pending: address=[0-9a-f]{8} tag=0402 bit=00 armed=01 ', text):
        raise ValueError('own TCH configuration descriptor is not armed with bit zero')
    if not traffic and any(kind == '1b' and payload.startswith('00b0') for kind, payload in packets.values()):
        raise ValueError('bounded call-setup fixture unexpectedly entered the traffic link')
    if card[1600:1611] != bytes.fromhex('ffffffff00f1100001ff00'):
        raise ValueError('laboratory location was not retained during call setup')


def check_physical_call(text, card, startup_offset=0, request=None, armed_second=19):
    check_incoming_call_setup(text, card, traffic=True, startup_offset=startup_offset, request=request)
    rows = PACKET.findall(text)
    if len(rows) != 92+startup_offset:
        raise ValueError('bounded physical-call packet inventory differs')
    def block(prefix):
        return prefix + '2b'*(25-len(bytes.fromhex(prefix))) + '000000'
    expected = {
        71: ('1b', block('00b0013f01')),
        72: ('1b', block('00b001000d062900')),
        75: ('1b', block('00b00104098307')),
        78: ('1b', block('00b0012615836502e090')),
        80: ('1b', block('00b0014809832a')),
        83: ('02', '041202001117001a600002580000001400000001'),
        86: ('02', '0412020200000016600002580000000001297000'),
        87: ('57', '03050000'),
    }
    if any((rows[index+startup_offset-1][1], rows[index+startup_offset-1][3]) != value for index, value in expected.items()):
        raise ValueError('own traffic establishment/connect/disconnect/release sequence differs')
    keys = re.findall(r'nsb7_call_input: key=(Send|End) state=([01]) t=([0-9.]+)', text)
    if keys != [('Send', '1', '22.000000000'), ('Send', '0', '22.200000000'),
                ('End', '1', '27.000000000'), ('End', '0', '27.200000000')]:
        raise ValueError('physical Answer/End fixture differs')
    for key, index in [('Send', 75), ('End', 78)]:
        if text.index(f'nsb7_call_input: key={key} state=1 ') > text.index(f'nsb7_radio_tx: index={index+startup_offset} '):
            raise ValueError('call signalling predates physical input')
    if not 22 <= float(rows[74+startup_offset][7]) < 27 or float(rows[77+startup_offset][7]) < 27:
        raise ValueError('CONNECT/clearing does not follow the physical call interval')
    for prefix in ['017301', '036009030f']:
        downlink = prefix + '2b'*(24-len(bytes.fromhex(prefix)))
        for pc, task in [('002e49e6', 4), ('002645ec', 14)]:
            if f'nsb7_lapdm_downlink: pc={pc} task={task} selector=b0 block={downlink} ' not in text:
                raise ValueError('traffic UA/CONNECT ACK did not reach both MCU consumers')
    if not re.search(r'nsb7_dedicated_armed: address=[0-9a-f]{8} value=00 pc=002e5086 task=4 t='+str(armed_second)+r'\.', text):
        raise ValueError('firmware did not clear the traffic configuration descriptor')


def check_trace(text, measurement=False, synchronization=False, channel=False, bcch=False, ccch=False, rach=False, assignment=False, dedicated=False, uplink=False, contention=False):
    uplink = uplink or contention
    dedicated = dedicated or uplink
    assignment = assignment or dedicated
    ccch = ccch or rach or assignment
    bcch = bcch or ccch
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
    if ccch:
        expected_types += (0x46, 0x02, 0x0f, 0x02, 0x1f, 0x0c)
        expected_lengths += (8, 20, 68, 20, 4, 8)
    if rach:
        expected_types += (0x0c, 0x02, 0x02, 0x1f, 0x0c, 0x0c, 0x02)
        expected_lengths += (8, 20, 20, 4, 8, 8, 20)
    if assignment:
        expected_types += (0x0c, 0x1f, 0x02)
        expected_lengths += (8, 4, 20)
    if uplink:
        expected_types += (0x1b,)
        expected_lengths += (28,)
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
        if ccch:
            expected_replies.extend((str(index), '89', '8') for index in range(14, 17))
        if rach:
            expected_replies.extend((str(index), kind, '8') for index, kind in enumerate(
                ['84', '84', '89', '89', '84', '84', '89'], 17))
        if assignment:
            expected_replies.extend([('17', '84', '8'), ('18', '80', '34')])
        if dedicated:
            expected_replies.append(('19', '89', '8'))
        if uplink:
            expected_replies.append(('20', '86', '8'))
        if contention:
            expected_replies.append(('21', '80', '34'))
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
    if ccch:
        if inventory[32][1] != bytes.fromhex('3210321000010000'):
            raise ValueError('post-CCCH type-46 publication differs')
        if (inventory[33][1][:12] != bytes.fromhex('041202020000001660000258') or
                inventory[35][1][:12] != bytes.fromhex('041202000000001a60000258')):
            raise ValueError('own common-control reconfiguration sequence differs')
        if inventory[34][1] != bytes.fromhex('8400' + 'ff'*64 + '0000'):
            raise ValueError('own empty neighbour-list publication differs')
        if inventory[36:38] != [(0x1f, bytes.fromhex('00040000')), (0x0c, bytes.fromhex('00000f6600000000'))]:
            raise ValueError('organic access-policy/random-access request differs')
        for mode, previous in [('10', '50'), ('16', '10'), ('1a', '16')]:
            cached = f'nsb7_channel_cached: descriptor=00000000 data=04{mode}00000000120002580000 live_channel=0258 live_mode={previous} '
            if cached not in text:
                raise ValueError(f'own mode-{mode} configuration was not cached from the previous receiver mode')
            if not re.search(r'nsb7_channel_ack: pc=002e5054 task=4 type=89 bit=00 cached=00[1-7][0-9a-f]{5} '
                             f'channel=0258 mode={previous} bsic=12 ', text):
                raise ValueError(f'own mode-{mode} acknowledgement missing')
        if not rach and not dedicated and 'nsb7_channel_state: cached=00000000 channel=0258 mode=1a bsic=12 t=14.000000000 ' not in text:
            raise ValueError('common-control receiver did not finish in mode 1a')
    if rach:
        check_rach(text, inventory)
        if 'nsb7_channel_state: cached=00000000 channel=0258 mode=16 bsic=12 t=24.000000000 ' not in text:
            raise ValueError('unanswered-assignment lifecycle did not settle in receiver mode 16')
    if assignment:
        check_assignment(text, inventory, dedicated)
    if dedicated:
        check_dedicated(text, inventory, uplink)
    if contention:
        check_contention(text, inventory)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('run', type=Path)
    parser.add_argument('--contract-json', type=Path)
    parser.add_argument('--measurement', action='store_true')
    parser.add_argument('--synchronization', action='store_true')
    parser.add_argument('--channel', action='store_true')
    parser.add_argument('--bcch', action='store_true')
    parser.add_argument('--ccch', action='store_true')
    parser.add_argument('--rach', action='store_true')
    parser.add_argument('--assignment', action='store_true')
    parser.add_argument('--dedicated', action='store_true')
    parser.add_argument('--uplink', action='store_true')
    parser.add_argument('--contention', action='store_true')
    parser.add_argument('--release', action='store_true')
    parser.add_argument('--paging', action='store_true')
    parser.add_argument('--addressed-page', action='store_true')
    parser.add_argument('--page-response', action='store_true')
    parser.add_argument('--incoming-call', action='store_true')
    parser.add_argument('--physical-call', action='store_true')
    args = parser.parse_args()
    try:
        report = contract(args.image.read_bytes())
        text = (args.run / 'error.log').read_text()
        if args.physical_call:
            check_security(text)
            check_physical_call(text, (args.run / 'nvram/nsb7hle/sim_card').read_bytes())
        elif args.incoming_call:
            check_security(text)
            check_incoming_call_setup(text, (args.run / 'nvram/nsb7hle/sim_card').read_bytes())
        elif args.page_response:
            check_security(text)
            check_page_response(text, (args.run / 'nvram/nsb7hle/sim_card').read_bytes())
        elif args.addressed_page:
            check_security(text)
            check_addressed_page(text)
        elif args.release or args.paging:
            check_security(text)
            check_receiver_return(text, (args.run / 'nvram/nsb7hle/sim_card').read_bytes(), paging=args.paging)
        else:
            check_trace(text, measurement=args.measurement, synchronization=args.synchronization,
                        channel=args.channel, bcch=args.bcch, ccch=args.ccch, rach=args.rach,
                        assignment=args.assignment, dedicated=args.dedicated, uplink=args.uplink,
                        contention=args.contention)
        states = re.findall(r'nsb7_radio_state: bands=([0-9a-f]{8}) measurement_route=([0-9a-f]{2}) t=([0-9.]+)', text)
        if states != [('98000000', '01', f'{time:.9f}') for time in (3.5, 6, 14, 24)]:
            raise ValueError('live feature/forwarding predicate differs')
        if args.contract_json:
            args.contract_json.write_text(json.dumps(report, indent=2) + '\n')
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-7 radio inventory: FAIL: {error}\n')
    print('PASS: ' + ('physical Answer/End produce CONNECT and cleared signalling call; no speech claim' if args.physical_call else
                     'own CALL CONFIRMED and ALERTING; traffic configuration pending; no connected-call claim' if args.incoming_call else
                     'background request completed; own Paging Response SABM and contention echo; LOCI retained' if args.page_response else
                     'own addressed page matches IMSI and returns parser event 083e; no access response' if args.addressed_page else
                     'own LOCI retained; repeated empty paging blocks reach firmware decoder' if args.paging else
                     'own LU/release acknowledged; receiver returned; SIM LOCI retained; paging withheld' if args.release else
                     'own contention echo accepted; firmware link state 2 to 4; no LU Accept supplied' if args.contention else
                     'own LAPDm SABM carries Location Updating Request; no UA supplied' if args.uplink else
                     'own bit-1 acknowledgement commits dedicated receiver; block request withheld' if args.dedicated else
                     'own Immediate Assignment accepted; dedicated-channel acknowledgement withheld' if args.assignment else
                     'own timed access completions reach task 17; assignment unanswered' if args.rach else
                     'own CCCH configuration committed; organic random-access request left unanswered' if args.ccch else
                     'own SI bitmap complete; organic CCCH request left unanswered' if args.bcch else
                     'own channel acknowledgement commits receiver; no BCCH replies' if args.channel else
                     'own SCH accepted; organic channel request left unanswered' if args.synchronization else
                     'PCS RSSI consumed by task 13; organic 56 request left unanswered' if args.measurement else
                     'own 29-packet no-peer TX baseline; final 55/03050000') + '; no RF completion claim')


if __name__ == '__main__':
    main()

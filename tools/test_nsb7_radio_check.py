import unittest
from tools.nsb7_radio_check import check_trace, channel_class, contract, TYPES, LENGTHS
from pathlib import Path
from tools.test_nsb7_security_check import trace as security_trace


def trace():
    rows = [security_trace()]
    cursor = 0
    for index, (kind, length) in enumerate(zip(TYPES, LENGTHS), 1):
        payload = '03050000' if kind == 0x55 else '00'*length
        producer = (cursor+(length+3)//2) % 0x52
        rows.append(f'nsb7_radio_tx: index={index} type={kind:02x} length={length} data={payload} '
                    f'cursor={cursor:02x} producer={producer:02x} pc=002cb0ec t=1.0 observer_writes=0')
        cursor = producer
    return '\n'.join(rows)


def measurement_trace():
    cursor = sum((length+3)//2 for length in LENGTHS) % 0x52
    producer = (cursor+81) % 0x52
    return trace() + '\n' + '\n'.join([
        'nsb7_candidate_object: index=1 type=56 length=160 words=' + ','.join(['0258'] + ['ffff']*79) +
        ' t=3 observer_writes=0',
        'nsb7_candidate_context: route=01 root08=00000000 root0c=00109438 root18=00109438 root28=00109438 t=3 observer_writes=0',
        'nsb7_candidate_record: address=00109438 channel=0258 flags=a2 t=3 observer_writes=0',
        f'nsb7_radio_tx: index=30 type=56 length=160 data=0258' + 'ff'*158 +
        f' cursor={cursor:02x} producer={producer:02x} pc=002cb0ec t=3.5 observer_writes=0',
        'nsb7_radio_rx: index=1 type=8e length=10 t=1 observer_writes=0',
        'nsb7_radio_rx: index=2 type=8e length=10 t=1 observer_writes=0',
        'nsb7_radio_rx: index=3 type=8b length=166 t=3 observer_writes=0',
        'nsb7_measurement: pc=002e5258 task=4 type=8b first=0258 second=ffff rssi=-60 t=3 observer_writes=0',
        'nsb7_measurement: pc=002a3704 task=13 type=8b first=0258 second=ffff rssi=-60 t=3 observer_writes=0',
        'nsb7_measurement_class: channel=0258 class=2 t=3 observer_writes=0'])


def synchronization_trace():
    cursor = (sum((length+3)//2 for length in LENGTHS) + 81) % 0x52
    producer = (cursor+11) % 0x52
    rows = [measurement_trace(),
            f'nsb7_radio_tx: index=31 type=02 length=20 data=041202000000005050000258' + '00'*8 +
            f' cursor={cursor:02x} producer={producer:02x} pc=002cb0ec t=6 observer_writes=0',
            'nsb7_radio_rx: index=4 type=80 length=14 t=5 observer_writes=0']
    for pc, task in [('002e4ef4', 4), ('00287574', 13), ('0028734c', 13)]:
        rows.append(f'nsb7_sch: pc={pc} task={task} selected=00109438 flags=a2 '
                    'payload=4012000004eb0258000048001d80 t=5 observer_writes=0')
    rows.append('nsb7_sch_candidate_commit: channel=0258 flags=a4 bsic=12 t=6 observer_writes=0')
    return '\n'.join(rows)


def channel_trace():
    cursor = (sum((length+3)//2 for length in LENGTHS) + 81 + 11) % 0x52
    return synchronization_trace() + '\n' + '\n'.join([
        f'nsb7_radio_tx: index=32 type=03 length=0 data= cursor={cursor:02x} producer={(cursor+1)%0x52:02x} pc=002cb0ec t=8 observer_writes=0',
        f'nsb7_radio_tx: index=33 type=55 length=4 data=04080000 cursor={(cursor+1)%0x52:02x} producer={(cursor+4)%0x52:02x} pc=002cb0ec t=9 observer_writes=0',
        'nsb7_radio_rx: index=5 type=89 length=8 t=5.8 observer_writes=0',
        'nsb7_channel_cached: descriptor=00000000 data=045000000000120002580000 live_channel=0000 live_mode=00 t=5.8 observer_writes=0',
        'nsb7_channel_ack: pc=002e5054 task=4 type=89 bit=00 cached=00101d78 channel=0000 mode=00 bsic=00 t=5.8 observer_writes=0',
        'nsb7_channel_state: cached=00000000 channel=0258 mode=50 bsic=12 t=6.000000000 observer_writes=0'])


def bcch_trace():
    rows = [line for line in channel_trace().splitlines() if not line.startswith(
        ('nsb7_radio_tx: index=32 ', 'nsb7_radio_tx: index=33 '))]
    cursor = (sum((length+3)//2 for length in LENGTHS) + 81 + 11) % 0x52
    rows.append(f'nsb7_radio_tx: index=32 type=02 length=20 data=041202090000001060000258' + '00'*8 +
                f' cursor={cursor:02x} producer={(cursor+11)%0x52:02x} pc=002cb0ec t=8 observer_writes=0')
    for index, kind in enumerate([0x1a, 0x1b, 0x1c, 0x1a, 0x1a, 0x1b, 0x1c, 0x19], 6):
        block = bytearray(24)
        block[:3] = bytes([0x49, 6, kind])
        if kind == 0x1b:
            block[3:10] = bytes.fromhex('000100f1100001')
        rows.append(f'nsb7_radio_rx: index={index} type=80 length=34 t=7 observer_writes=0')
        for pc, task, tag in [('002e4e96', 4, '1802'), ('0029d42c', 17, '07ff')]:
            rows.append(f'nsb7_bcch: pc={pc} task={task} tag={tag} mode=50 error=00 channel=0258 '
                        f'block={block.hex()} flags=00 t=7 observer_writes=0')
    rows.append('nsb7_bcch_flags: flags=0f pc=0029d5ae t=8 observer_writes=0')
    return '\n'.join(rows)


def ccch_trace():
    rows = [bcch_trace()]
    cursor = (sum((length+3)//2 for length in LENGTHS) + 81 + 22) % 0x52
    payloads = [(0x46, '3210321000010000'),
                (2, '041202020000001660000258' + '00'*8),
                (0x0f, '8400' + 'ff'*64 + '0000'),
                (2, '041202000000001a60000258' + '00'*8),
                (0x1f, '00040000'), (0x0c, '00000f6600000000')]
    for index, (kind, payload) in enumerate(payloads, 33):
        length = len(payload)//2
        producer = (cursor+(length+3)//2) % 0x52
        rows.append(f'nsb7_radio_tx: index={index} type={kind:02x} length={length} data={payload} '
                    f'cursor={cursor:02x} producer={producer:02x} pc=002cb0ec t=11 observer_writes=0')
        cursor = producer
    for index, (mode, previous) in enumerate([('10', '50'), ('16', '10'), ('1a', '16')], 14):
        rows.extend([
            f'nsb7_radio_rx: index={index} type=89 length=8 t=10 observer_writes=0',
            f'nsb7_channel_cached: descriptor=00000000 data=04{mode}00000000120002580000 live_channel=0258 live_mode={previous} t=10 observer_writes=0',
            f'nsb7_channel_ack: pc=002e5054 task=4 type=89 bit=00 cached=00101e08 channel=0258 mode={previous} bsic=12 t=10 observer_writes=0'])
    rows.append('nsb7_channel_state: cached=00000000 channel=0258 mode=1a bsic=12 t=14.000000000 observer_writes=0')
    return '\n'.join(rows)


def rach_trace():
    rows = [ccch_trace().replace('mode=1a bsic=12 t=14', 'mode=16 bsic=12 t=14')]
    cursor = (sum((length+3)//2 for length in LENGTHS) + 81 + 22 + 5 + 11 + 35 + 11 + 3 + 5) % 0x52
    config16 = '041202020000001660000258' + '00'*8
    config1a = '041202000000001a60000258' + '00'*8
    for index, (kind, payload) in enumerate([
            (0x0c, '00010902098e0000'), (2, config16), (2, config1a),
            (0x1f, '00040000'), (0x0c, '0000086c00000000'),
            (0x0c, '000108020def0000'), (2, config16)], 39):
        length = len(payload)//2
        producer = (cursor+(length+3)//2) % 0x52
        rows.append(f'nsb7_radio_tx: index={index} type={kind:02x} length={length} data={payload} '
                    f'cursor={cursor:02x} producer={producer:02x} pc=002cb0ec t=12 observer_writes=0')
        cursor = producer
    for index, kind in enumerate(['84', '84', '89', '89', '84', '84', '89'], 17):
        rows.append(f'nsb7_radio_rx: index={index} type={kind} length=8 t=12 observer_writes=0')
    cases = [(15, 0, 2288, 2288, 0, 102, 2390, 2391),
             (9, 1, 2391, 2391, 2446, 2, 2448, 2449),
             (8, 0, 3402, 3402, 0, 108, 3510, 3512),
             (8, 1, 3512, 3512, 3567, 2, 3569, 3570)]
    for index, (ra, mode, frame, anchor, base, slots, deadline, actual) in enumerate(cases, 1):
        rows.append(f'dsp_hle: access_deadline ra={ra:02x} mode={mode} frame={frame} anchor={anchor} '
                    f'base={base} slots={slots} deadline={deadline} t=12')
        payload = bytes([ra]) + actual.to_bytes(3, 'big') + bytes(4)
        for pc, task in [('002e4fa4', 4), ('002f5274', 13)]:
            rows.append(f'nsb7_rach: pc={pc} task={task} type=84 payload={payload.hex()} t=12 observer_writes=0')
        rows.append(f'nsb7_rach_reference: index={index} task=13 target=17 tag=07f9 '
                    f't1={(actual//1326)&31:02x} t3={actual%51:02x} t2={actual%26:02x} ra={ra:02x} t=12 observer_writes=0')
    rows.append('nsb7_channel_state: cached=00000000 channel=0258 mode=16 bsic=12 t=24.000000000 observer_writes=0')
    return '\n'.join(rows)


def assignment_trace():
    rows = [ccch_trace()]
    cursor = (sum((length+3)//2 for length in LENGTHS) + 81 + 22 + 5 + 11 + 35 + 11 + 3 + 5) % 0x52
    for index, (kind, payload) in enumerate([
            (0x0c, '00010902098e0000'), (0x1f, '00090000'),
            (2, '0412000000000040800002584000000000000000')], 39):
        length = len(payload)//2
        producer = (cursor+(length+3)//2) % 0x52
        rows.append(f'nsb7_radio_tx: index={index} type={kind:02x} length={length} data={payload} '
                    f'cursor={cursor:02x} producer={producer:02x} pc=002cb0ec t=12 observer_writes=0')
        cursor = producer
    rows.extend([
        'nsb7_radio_rx: index=17 type=84 length=8 t=11 observer_writes=0',
        'nsb7_radio_rx: index=18 type=80 length=34 t=12 observer_writes=0',
        'dsp_hle: access_deadline ra=0f mode=0 frame=2288 anchor=2288 base=0 slots=102 deadline=2390 t=10',
        'nsb7_rach: pc=002e4fa4 task=4 type=84 payload=0f00095700000000 t=11 observer_writes=0',
        'nsb7_rach: pc=002f5274 task=13 type=84 payload=0f00095700000000 t=11 observer_writes=0',
        'nsb7_rach_reference: index=1 task=13 target=17 tag=07f9 t1=01 t3=2d t2=19 ra=0f t=11 observer_writes=0',
        'dsp_hle: assignment_deadline access_frame=2391 deadline=2395',
        'nsb7_channel_pending: address=00102138 tag=0402 bit=01 armed=01 t=12 observer_writes=0',
        'nsb7_channel_state: cached=00000000 channel=0258 mode=1a bsic=12 t=24.000000000 observer_writes=0'])
    block = (bytes.fromhex('2d063f002002580f0db90000') + b'\x2b'*12).hex()
    rows.extend([
        f'nsb7_agch_forward: index=1 task=4 mode=60 error=00 frame=2395 block={block} t=12 observer_writes=0',
        f'nsb7_agch_consumer: index=1 task=14 tag=0623 mode=60 block={block} t=12 observer_writes=0',
        f'nsb7_rr_decoder_result: index=1 task=14 code=061a block={block} t=12 observer_writes=0'])
    return '\n'.join(rows)


def dedicated_trace():
    return assignment_trace().replace('mode=1a bsic=12 t=', 'mode=40 bsic=12 t=') + '\n' + '\n'.join([
        'nsb7_radio_rx: index=19 type=89 length=8 t=12 observer_writes=0',
        'nsb7_channel_ack: pc=002e5054 task=4 type=89 bit=01 cached=00000000 channel=0258 mode=1a bsic=12 t=12 observer_writes=0',
        'nsb7_dedicated_armed: address=00102138 value=00 pc=002e5086 task=4 t=12 observer_writes=0'])


def uplink_trace():
    payload = '0080013f4905087000f000fffe200809101010325476982b2b000000'
    cursor = (sum((length+3)//2 for length in LENGTHS) + 81 + 22 + 5 + 11 + 35 + 11 + 3 + 5 + 5 + 3 + 11) % 0x52
    return dedicated_trace() + '\n' + '\n'.join([
        'nsb7_radio_rx: index=20 type=86 length=8 t=12 observer_writes=0',
        'nsb7_block_request: pc=002e5228 task=4 selector=80 queue=001021f8 t=12 observer_writes=0',
        'nsb7_block_request: pc=002e513e task=4 selector=00 queue=001021f8 t=12 observer_writes=0',
        f'nsb7_radio_tx: index=42 type=1b length=28 data={payload} cursor={cursor:02x} producer={(cursor+15)%0x52:02x} pc=002cb0ec t=12 observer_writes=0'])


def contention_trace():
    information = '05087000f000fffe20080910101032547698'
    block = '017349' + information + '2b2b2b'
    return uplink_trace() + '\n' + '\n'.join([
        'nsb7_radio_rx: index=21 type=80 length=34 t=13 observer_writes=0',
        f'nsb7_lapdm_downlink: pc=002e49e6 task=4 selector=80 block={block} t=13 observer_writes=0',
        f'nsb7_lapdm_downlink: pc=002645ec task=14 selector=80 block={block} t=13 observer_writes=0',
        f'nsb7_ua_compare: context=0012a6dc state=2 length=18 sent={information} received={information} t=13 observer_writes=0',
        'nsb7_lapdm_state: context=0012a6dc state=4 pc=002665c0 task=14 t=13 observer_writes=0',
        f'nsb7_rr_decoder_result: index=3 task=14 code=0614 block={block} t=13 observer_writes=0'])


class RadioInventoryTests(unittest.TestCase):
    def test_contention_requires_native_echo_comparison_and_link_state_change(self):
        check_trace(contention_trace(), contention=True)
        for old, new in [('received=05087000f000fffe20', 'received=05087000f000fffe23'),
                         ('state=4 pc=002665c0', 'state=2 pc=002665c0'),
                         ('code=0614 block=', 'code=0613 block='),
                         ('block=017349', 'block=017301'),
                         ('context=0012a6dc', 'context=00000000')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_trace(contention_trace().replace(old, new), contention=True)
        with self.assertRaises(ValueError):
            check_trace(contention_trace() + '\nnsb7_radio_rx: index=22 type=80 length=34 t=14', contention=True)
        with self.assertRaises(ValueError):
            check_trace(contention_trace() + '\nnsb7_lapdm_state: context=0012a6dc state=2 pc=002665c0 task=14 t=15', contention=True)

    def test_dedicated_ack_requires_own_descriptor_clear(self):
        check_trace(dedicated_trace(), dedicated=True)
        for old, new in [('bit=01 cached=00000000', 'bit=00 cached=00000000'),
                         ('address=00102138 value=00', 'address=00102138 value=01'),
                         ('mode=40 bsic=12 t=24', 'mode=1a bsic=12 t=24')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_trace(dedicated_trace().replace(old, new), dedicated=True)

    def test_block_request_pulls_own_sabm_with_location_update(self):
        check_trace(uplink_trace(), uplink=True)
        for old, new in [('selector=80 queue=', 'selector=70 queue='),
                         ('pc=002e513e task=4 selector=00 queue=001021f8', 'pc=002e513e task=4 selector=00 queue=00000000'),
                         ('0080013f49', '0080010349'), ('fffe200809', 'fffe230809'),
                         ('0910101032547698', '0910101032547699')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_trace(uplink_trace().replace(old, new), uplink=True)
        with self.assertRaises(ValueError):
            check_trace(uplink_trace() + '\nnsb7_radio_rx: index=21 type=80 length=34 t=13', uplink=True)

    def test_assignment_preserves_access_reference_and_arms_dedicated_request(self):
        check_trace(assignment_trace(), assignment=True)
        for old, new in [('frame=2395 block=', 'frame=2391 block='),
                         ('code=061a block=', 'code=061b block='),
                         ('tag=0623 mode=', 'tag=0622 mode='),
                         ('02580f0db9', '0258090db9'),
                         ('tag=0402 bit=01 armed=01', 'tag=0402 bit=00 armed=01'),
                         ('004080000258', '004080000001')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_trace(assignment_trace().replace(old, new), assignment=True)
        with self.assertRaises(ValueError):
            check_trace(assignment_trace() + '\nnsb7_radio_rx: index=19 type=89 length=8 t=13', assignment=True)

    def test_access_completion_honours_deadline_and_own_request_reference(self):
        check_trace(rach_trace(), rach=True)
        for old, new in [('deadline=2390', 'deadline=2289'),
                         ('frame=2391 anchor=2391', 'frame=2391 anchor=2288'),
                         ('payload=0f000957', 'payload=0f0008f1'),
                         ('t1=01 t3=2d t2=19 ra=0f', 't1=01 t3=2d t2=19 ra=09'),
                         ('mode=16 bsic=12 t=24', 'mode=1a bsic=12 t=24')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_trace(rach_trace().replace(old, new), rach=True)
        with self.assertRaises(ValueError):
            check_trace(rach_trace() + '\nnsb7_radio_rx: index=24 type=80 length=34 t=17', rach=True)

    def test_common_control_reconfiguration_reaches_organic_access_request(self):
        check_trace(ccch_trace(), ccch=True)
        for old, new in [('mode=1a bsic=12 t=14', 'mode=16 bsic=12 t=14'),
                         ('data=041a00000000120002580000', 'data=041600000000120002580000'),
                         ('data=00000f6600000000', 'data=0000006600000000'),
                         ('channel=0258 mode=16 bsic=12 t=10', 'channel=0258 mode=10 bsic=12 t=10')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_trace(ccch_trace().replace(old, new), ccch=True)
        with self.assertRaises(ValueError):
            check_trace(ccch_trace() + '\nnsb7_radio_rx: index=17 type=84 length=8 t=12', ccch=True)

    def test_bcch_reaches_own_si_parser_and_ccch_request(self):
        check_trace(bcch_trace(), bcch=True)
        for old, new in [('flags=0f pc=0029d5ae', 'flags=0e pc=0029d5ae'),
                         ('pc=0029d42c task=17', 'pc=0029d42c task=13'),
                         ('tag=07ff mode=50', 'tag=07fe mode=50'),
                         ('block=49061b000100f1100001', 'block=49061b000100f1100002'),
                         ('041202090000001060000258', '041202090000001050000258')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_trace(bcch_trace().replace(old, new), bcch=True)
        with self.assertRaises(ValueError):
            check_trace(bcch_trace() + '\nnsb7_radio_rx: index=14 type=89 length=8 t=9', bcch=True)

    def test_channel_ack_commits_config_without_bcch_continuation(self):
        check_trace(channel_trace(), channel=True)
        for old, new in [('bit=00 cached=', 'bit=01 cached='),
                         ('cached=00000000 channel=0258', 'cached=00101d78 channel=0258'),
                         ('channel=0258 mode=50', 'channel=0258 mode=00'),
                         ('data=045000000000120002580000', 'data=045000000000120000010000')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_trace(channel_trace().replace(old, new), channel=True)
        with self.assertRaises(ValueError):
            check_trace(channel_trace() + '\nnsb7_radio_rx: index=6 type=80 length=34 t=7', channel=True)

    def test_synchronization_requires_own_parser_and_organic_request(self):
        check_trace(synchronization_trace(), synchronization=True)
        for old, new in [('flags=a4 bsic=12', 'flags=a2 bsic=12'),
                         ('pc=0028734c task=13', 'pc=0028734c task=12'),
                         ('4012000004eb0258', '4012010004eb0258'),
                         ('041202000000005050000258', '041202000000005050000001'),
                         ('0048001d80', '0000001d80')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_trace(synchronization_trace().replace(old, new), synchronization=True)
        with self.assertRaises(ValueError):
            check_trace(synchronization_trace() + '\nnsb7_radio_rx: index=5 type=89 length=8 t=7', synchronization=True)

    def test_runtime_bootstrap_alias_corruption_rejected(self):
        payload = '0258' + 'ff'*158
        corrupted = payload[:49*4] + '0006' + payload[50*4:]
        with self.assertRaises(ValueError):
            check_trace(measurement_trace().replace(payload, corrupted), True)
        with self.assertRaises(ValueError):
            check_trace(measurement_trace().replace('nsb7_candidate_object:', 'missing_object:'), True)

    def test_measurement_reaches_own_consumer_without_extra_responses(self):
        check_trace(measurement_trace(), True)
        for old, new in [('task=13', 'task=12'), ('first=0258', 'first=0001'),
                         ('index=3 type=8b', 'index=3 type=89'),
                         ('root08=00000000', 'root08=00109438'),
                         ('flags=a2', 'flags=a4'),
                         ('root28=00109438', 'root28=00000000'),
                         ('class=2 t=3', 'class=3 t=3')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_trace(measurement_trace().replace(old, new), True)
        with self.assertRaises(ValueError):
            check_trace(measurement_trace() + '\nnsb7_radio_rx: index=4 type=80 length=10 t=4', True)

    def test_own_receive_and_exhaustive_classifier_contract(self):
        image = Path(__file__).resolve().parents[1] / 'roms/noki8290/8290f522a.fls'
        if not image.exists():
            self.skipTest('own NSB-7 image unavailable')
        report = contract(image.read_bytes())
        self.assertEqual(report['task'], 13)
        self.assertEqual(len(report['rx_dispatch']), 13)
        self.assertEqual(report['rx_dispatch']['8a'], '00309fbc')
        self.assertEqual(report['rx_dispatch']['85'], '0030a006')
        self.assertEqual(report['rx_8a']['task'], 13)
        self.assertEqual(report['rx_80']['subtype_handlers']['40'], '002e4f8a')
        self.assertEqual(report['rx_80']['frame_payload_offsets'], [3, 4, 5])
        self.assertEqual(report['rx_80']['subtype40_gate_address'], '00130784')
        self.assertEqual(report['rx_80']['candidate_channel_payload_offsets'], [6, 7])
        self.assertEqual(report['rx_80']['success_record_state'], 4)
        self.assertEqual(report['rx_80']['bcch_task'], 17)
        self.assertEqual(report['rx_80']['bcch_block_payload_offset'], 10)
        self.assertEqual(report['rx_80']['complete_si_bitmap'], 15)
        self.assertEqual(report['rx_84']['request_reference_task'], 17)
        self.assertEqual(report['rx_84']['frame_payload_offsets'], [1, 2, 3])
        self.assertEqual(report['tx_0c']['scheduled_base_offsets'], [4, 5])
        self.assertEqual(report['rx_80']['agch_task'], 14)
        self.assertEqual(report['rx_80']['agch_tag'], '0623')
        self.assertEqual(report['rx_80']['ccch_block_endings'], [9, 15, 19, 25, 29, 35, 39, 45, 49])
        self.assertEqual(report['rx_89']['dedicated_bit'], 1)
        self.assertEqual(report['rx_86']['dedicated_uplink_queue'], '00130434')
        self.assertEqual(report['contention']['state_offset'], 0x163)
        self.assertEqual(report['contention']['after'], 4)
        self.assertEqual(report['payload_prefix_bytes'], 4)
        self.assertEqual(report['classified_inputs'], 65536)
        self.assertEqual(report['ranges'], [
            {'first': 0, 'last': 0, 'class': 0},
            {'first': 1, 'last': 124, 'class': 1},
            {'first': 125, 'last': 511, 'class': 0},
            {'first': 512, 'last': 885, 'class': 2},
            {'first': 886, 'last': 65535, 'class': 0}])

    def test_classifier_precedence_and_edges(self):
        for channel, value in ((511, 0), (512, 3), (810, 3), (811, 0)):
            self.assertEqual(channel_class(channel, 1 << 27), value)
        self.assertEqual(channel_class(600, (1 << 27) | (1 << 28)), 2)
        self.assertEqual(channel_class(0, 1 << 30), 1)
        self.assertEqual(channel_class(975, 1 << 30), 1)
        self.assertEqual(channel_class(1024, 1 << 30), 0)
        self.assertEqual(channel_class(955, 1 << 29), 1)
        self.assertEqual(channel_class(974, 1 << 29), 1)
        with self.assertRaises(ValueError):
            channel_class(65536, 0xffffffff)

    def test_complete_wrapped_inventory(self):
        check_trace(trace())

    def test_missing_packet_rejected(self):
        for line in trace().splitlines()[-len(TYPES):]:
            with self.subTest(line=line[:60]), self.assertRaises(ValueError):
                check_trace(trace().replace(line, ''))

    def test_reject_bad_geometry_request_or_observer(self):
        for old, new in [('producer=06', 'producer=07'), ('pc=002cb0ec', 'pc=00000000'),
                         ('data=03050000', 'data=03030000'), ('index=2 ', 'index=3 ')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_trace(trace().replace(old, new))
        with self.assertRaises(ValueError):
            check_trace(trace() + '\nnsb7_radio_capture: invalid_extent')


if __name__ == '__main__':
    unittest.main()

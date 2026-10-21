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


class RadioInventoryTests(unittest.TestCase):
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

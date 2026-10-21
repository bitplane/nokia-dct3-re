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


class RadioInventoryTests(unittest.TestCase):
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

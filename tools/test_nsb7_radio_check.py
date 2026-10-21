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


class RadioInventoryTests(unittest.TestCase):
    def test_own_receive_and_exhaustive_classifier_contract(self):
        image = Path(__file__).resolve().parents[1] / 'roms/noki8290/8290f522a.fls'
        if not image.exists():
            self.skipTest('own NSB-7 image unavailable')
        report = contract(image.read_bytes())
        self.assertEqual(report['task'], 13)
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

import unittest
from pathlib import Path

from tools.noki8210_channel_config_contract import (
    acquisition_descriptor, serialize, verify, verify_bookkeeping_trace)


class ChannelConfigContractTest(unittest.TestCase):
    def test_rejects_unpinned_image(self):
        with self.assertRaisesRegex(ValueError, 'acquired 8210'):
            verify(b'')

    def test_acquired_constructor(self):
        path = Path(__file__).resolve().parents[1] / 'roms/noki8210/8210_5.31ppm_c.fls'
        if not path.exists():
            self.skipTest('acquired NSM-3 ROM unavailable')
        verify(path.read_bytes())

    def test_observed_descriptor_wire_pairs(self):
        captures = (
            ('04500000000012000337000000296ff90000000000000000',
             '000214020412020000000050500003370000000000296ff9'),
            ('041000000000120003370000002970000000000000010900',
             '000214020412020900000010600003371000000000297000'),
            ('041600000000120003370000002970000000000001000200',
             '000214020412020200000016600003370000000001297000'),
            ('041a0000000f120003370000000000000000000000000000',
             '00021402041202000000001a600003370000000f00000000'))
        for descriptor, expected in captures:
            with self.subTest(descriptor=descriptor):
                packet, updated = serialize(bytes.fromhex(descriptor))
                self.assertEqual(packet.hex(), expected)
                self.assertEqual(updated[10], packet[12])

    def test_exact_50_skips_tail_but_51_does_not(self):
        descriptor = bytearray(range(24))
        descriptor[1] = 0x50
        descriptor[21] = 1
        packet, _ = serialize(descriptor)
        self.assertEqual((packet[7], packet[16], packet[20]), (0, 0, 0))
        descriptor[1] = 0x51
        packet, _ = serialize(descriptor)
        self.assertEqual((packet[7], packet[16], packet[20]), (22, 16, 20))

    def test_other_nibble_preserves_descriptor_byte_a(self):
        descriptor = bytearray(range(24))
        descriptor[1] = 0x20
        packet, updated = serialize(descriptor)
        self.assertEqual(packet[12], 0)
        self.assertEqual(updated[10], 10)
        self.assertEqual(packet[6], descriptor[6] & 7)

    def test_capture_length(self):
        for length in (0, 23, 25):
            with self.assertRaises(ValueError):
                serialize(bytes(length))

    def test_live_acquisition_record_to_descriptor(self):
        record = bytes.fromhex('00296ff90000033700c41200ffa4c60c0000000000000338')
        descriptor = acquisition_descriptor(record)
        self.assertEqual(descriptor.hex(),
                         '04500000000012000337000000296ff90000000000000000')
        packet, _ = serialize(descriptor)
        self.assertEqual(packet.hex(),
                         '000214020412020000000050500003370000000000296ff9')

    def test_acquisition_capture_bounds(self):
        for length in (0, 23, 25):
            with self.assertRaises(ValueError):
                acquisition_descriptor(bytes(length))

    def test_bookkeeping_completion_and_failures(self):
        lines = [
            '8210_channel_config_bookkeeping: pc=002eaf74 caller=002eb25f argument=0013ac7c saved=00000000 data=none controller=000000000000000000000000 state=10 t=7.897816',
            '8210_channel_config_bookkeeping: pc=002eaf74 caller=002df295 argument=00000000 saved=001132ac data=045000000000120003370000 controller=000000000000000000000000 state=10 t=7.908436',
            '8210_channel_config_bookkeeping: pc=00305228 caller=002eafc5 argument=00000001 saved=00000000 data=none controller=000000000337001250000000 state=10 t=7.908521']
        text = '\n'.join(lines)
        self.assertEqual(verify_bookkeeping_trace(text), 1)
        for bad in ('', '\n'.join(lines[:-1]), '\n'.join(lines[1:]),
                    text.replace('0337001250', '0338001250'),
                    text.replace('t=7.908521', 't=7.000000'),
                    text.replace('saved=00000000 data=none controller=000000000337',
                                 'saved=001132ac data=none controller=000000000337')):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                verify_bookkeeping_trace(bad)

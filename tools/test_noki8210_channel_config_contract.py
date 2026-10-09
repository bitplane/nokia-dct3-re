import unittest
from pathlib import Path

from tools.noki8210_channel_config_contract import serialize, verify


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

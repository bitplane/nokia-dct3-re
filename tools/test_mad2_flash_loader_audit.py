import struct
import unittest
from tools.mad2_flash_loader_audit import bootstrap_span, checksum_words, pc_sub_table


class FlashLoaderAuditTest(unittest.TestCase):
    def test_mov_pc_is_not_literal_load_aligned_and_table_is_bounded(self):
        data = bytearray(0xa0)
        struct.pack_into('>3H', data, 0x7a, 0x4679, 0x3952, 0x5809)
        self.assertEqual(pc_sub_table(data, 0x7a, 1), 0x2c)
        for site, register in ((0x7a, 2), (0x9e, 1), (-1, 1)):
            with self.assertRaises(ValueError):
                pc_sub_table(data, site, register)
        struct.pack_into('>H', data, 0x7c, 0x39ff)
        with self.assertRaises(ValueError):
            pc_sub_table(data, 0x7a, 1)

    def test_checksum_is_big_endian_and_excludes_suffix(self):
        data = bytes.fromhex('1234edccaaaa')
        self.assertEqual(checksum_words(data, 4), 0)
        self.assertEqual(checksum_words(data, 6), 0xaaaa)
        for span in (0, 3, 8, -2):
            with self.assertRaises(ValueError):
                checksum_words(data, span)

    def test_length_uses_decoded_immediates_not_file_size(self):
        data = b'\x00' * 0x12 + struct.pack('>4H', 0x20a0, 0x2201, 0x0212, 0x4310)
        self.assertEqual(bootstrap_span(data), 0x1a0)
        with self.assertRaises(ValueError):
            bootstrap_span(data[:-1])
        for index in (0x12, 0x14, 0x16, 0x18):
            changed = bytearray(data)
            changed[index] ^= 0x80
            with self.assertRaises(ValueError):
                bootstrap_span(changed)


if __name__ == '__main__':
    unittest.main()

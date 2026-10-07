import unittest

from tools.gsm7_text import DEFAULT_ALPHABET, encode_text


def unpack(hex_string, count):
    value = int.from_bytes(bytes.fromhex(hex_string), 'little')
    return bytes((value >> (7 * index)) & 0x7f for index in range(count))


class Gsm7TextTest(unittest.TestCase):
    def test_alphabet_extent(self):
        self.assertEqual(len(DEFAULT_ALPHABET), 128)

    def test_known_hello_vector(self):
        self.assertEqual(encode_text('hello'), ('e8329bfd06', 5))

    def test_non_ascii_and_ascii_relocations(self):
        data, count = encode_text('@_\u00a3\u00e9\u0394')
        self.assertEqual(count, 5)
        self.assertEqual(unpack(data, count), bytes((0, 0x11, 1, 5, 0x10)))

    def test_all_extension_codes_and_counts(self):
        data, count = encode_text('\f^{}\\[~]|\u20ac')
        self.assertEqual(count, 20)
        self.assertEqual(unpack(data, count), bytes((
            0x1b, 0x0a, 0x1b, 0x14, 0x1b, 0x28, 0x1b, 0x29, 0x1b, 0x2f,
            0x1b, 0x3c, 0x1b, 0x3d, 0x1b, 0x3e, 0x1b, 0x40, 0x1b, 0x65)))

    def test_unsupported_and_literal_escape_rejected(self):
        for text in ('`', '\x1b', '\u4e2d', '\x00'):
            self.assertIsNone(encode_text(text))

    def test_ussd_spare_septet_is_cr_not_at(self):
        sms, count = encode_text('1234567')
        ussd, ussd_count = encode_text('1234567', ussd=True)
        self.assertEqual((count, ussd_count), (7, 7))
        self.assertEqual(unpack(sms, 8)[-1], 0)
        self.assertEqual(unpack(ussd, 8)[-1], 0x0d)

    def test_ussd_real_trailing_cr_preserved(self):
        data, count = encode_text('1234567\r', ussd=True)
        self.assertEqual(count, 8)
        self.assertEqual(unpack(data, 9)[-2:], b'\r\r')

    def test_sms_does_not_append_real_cr(self):
        data, count = encode_text('1234567\r')
        self.assertEqual((count, len(bytes.fromhex(data))), (8, 7))


if __name__ == '__main__':
    unittest.main()

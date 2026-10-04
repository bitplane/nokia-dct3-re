import unittest
from tools.noki6250_speech_control_check import recover, verify


class SpeechControlCheckTests(unittest.TestCase):
    def image(self):
        data = bytearray(0x22a140)
        for address, value in ((0x429aa8, 0x429de6), (0x42a138, 0xffff8000),
                               (0x42a108, 0x100a8), (0x3fbfec, 0x263160),
                               (0x3fc028, 0x263154)):
            offset = address - 0x200000
            data[offset:offset + 4] = value.to_bytes(4, "big")
        for n in range(5):
            offset = 0x63154 + 2*n
            data[offset:offset + 2] = b"\x02\x00"
        data[0x63160:0x63162] = b"\xfd\xff"
        return data

    def test_own_field_and_big_endian_tables(self):
        self.assertEqual(recover(self.image()), 0x200)

    def test_foreign_field_rejected(self):
        data = self.image()
        data[0x63154:0x63156] = b"\x02\x01"
        with self.assertRaisesRegex(ValueError, "add/remove"):
            recover(data)

    def test_decoding_alone_is_not_runtime_proof(self):
        with self.assertRaisesRegex(ValueError, "control evidence"):
            verify(self.image(), "")

    def test_truncated_image_rejected(self):
        with self.assertRaisesRegex(ValueError, "outside image"):
            recover(bytes(100))


if __name__ == "__main__":
    unittest.main()

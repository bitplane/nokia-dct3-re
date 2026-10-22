import unittest
from tools.noki6250_speech_control_check import CONTROL_CODE, recover, verify


class SpeechControlCheckTests(unittest.TestCase):
    EVENTS = (
        '6250_audio_field: selector=01 keep=fdff add=0200 before=0426',
        '6250_audio_helper: command=08 value=0626 commit=1 caller=003fc33f',
        'doorbell wire=8626 speech_control=0626',
        '6250_audio_helper: command=08 value=0426 commit=1 caller=003fc33f',
        'doorbell wire=8426 speech_control=0426',
    )

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
        for address, encoding, _ in CONTROL_CODE:
            expected = bytes.fromhex(encoding)
            offset = address - 0x200000
            data[offset:offset + len(expected)] = expected
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

    def test_ordered_runtime_control(self):
        self.assertEqual(verify(self.image(), '\n'.join(self.EVENTS)), 0x200)

    def test_each_runtime_transition_required(self):
        for index in range(len(self.EVENTS)):
            with self.subTest(index=index):
                events = self.EVENTS[:index] + self.EVENTS[index+1:]
                with self.assertRaisesRegex(ValueError, 'control evidence'):
                    verify(self.image(), '\n'.join(events))

    def test_end_before_answer_rejected(self):
        events = self.EVENTS[:1] + self.EVENTS[3:] + self.EVENTS[1:3]
        with self.assertRaisesRegex(ValueError, 'control evidence'):
            verify(self.image(), '\n'.join(events))

    def test_matching_tables_do_not_hide_changed_consumers(self):
        for address, _, meaning in CONTROL_CODE:
            with self.subTest(meaning=meaning):
                data = self.image()
                data[address - 0x200000] ^= 1
                with self.assertRaisesRegex(ValueError, 'instruction contract'):
                    recover(data)

    def test_little_endian_instruction_lanes_rejected(self):
        data = self.image()
        address, encoding, _ = CONTROL_CODE[2]
        expected = bytes.fromhex(encoding)
        offset = address - 0x200000
        for n in range(0, len(expected), 2):
            data[offset+n:offset+n+2] = expected[n:n+2][::-1]
        with self.assertRaisesRegex(ValueError, 'instruction contract'):
            recover(data)

    def test_truncated_image_rejected(self):
        with self.assertRaisesRegex(ValueError, "outside image"):
            recover(bytes(100))


if __name__ == "__main__":
    unittest.main()

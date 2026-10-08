import unittest
from unittest.mock import patch

from tools.noki8890_speech_control_check import recover, verify_call


class SpeechControlTest(unittest.TestCase):
    def test_physical_call_correlates_own_selector_and_writer(self):
        text = ('8890_call_physical: action=send\n8890_keypad_decoded: key=0e\n'
                'dsp_control_write: data=860b pc=002c338e r4=00000008 rest\n'
                '8890_call_physical: action=end\n8890_keypad_decoded: key=0f\n'
                'dsp_control_write: data=840a pc=002c338e r4=00000008 rest\n')
        self.assertTrue(verify_call(text)['runtime_validated'])
        self.assertFalse(verify_call(text)['pcm_validated'])
        incoming = text.replace('8890_call_physical: action=send',
                                '8890_incoming_physical: action=Call / Send').replace(
                                    '8890_call_physical: action=end',
                                    '8890_incoming_physical: action=End')
        self.assertTrue(verify_call(incoming, incoming=True)['runtime_validated'])
        with self.assertRaises(ValueError):
            verify_call(incoming)
        with self.assertRaises(ValueError):
            verify_call(text, incoming=True)
        for wrong in (text.replace('002c338e', '002cb3ca'),
                      text.replace('r4=00000008', 'r4=00000009'),
                      text.replace('key=0e', 'key=0f'),
                      text.replace('action=end', 'action=send'),
                      text.replace('data=840a', 'data=860b')):
            with self.subTest(text=wrong), self.assertRaises(ValueError):
                verify_call(wrong)

    def fixture(self):
        image = bytearray(0xc36b8)
        instructions = {0x2c30a2: '2834', 0x2c32f8: '0270', 0x2c3300: '0240',
                        0x2c3302: '49e1', 0x2c32b8: '4e0e', 0x2c32ba: '8877',
                        0x2c32bc: '4039', 0x2c32aa: '8070', 0x2c3340: '48db',
                        0x2c3348: '49da', 0x2c31c8: '8008', 0x2c31ca: '4a83',
                        0x2c338e: '8010'}
        for address, value in instructions.items():
            image[address - 0x200000:address - 0x200000 + 2] = bytes.fromhex(value)
        words = {0x2c30b0 + 8 * 4: 0x2c3340, 0x2c30b0 + 17 * 4: 0x2c32f8,
                 0x2c32f4: 0x134c4c, 0x2c3688: 0xfdff, 0x2c36b0: 0xffff8000,
                 0x2c36b4: 0x134c4e, 0x2c33d8: 0x100a8}
        for address, value in words.items():
            image[address - 0x200000:address - 0x200000 + 4] = value.to_bytes(4, 'big')
        return image

    def check(self, image):
        with patch('tools.noki8890_speech_control_check.hashlib.sha1') as digest:
            digest.return_value.hexdigest.return_value = 'a214a0d69760ecd8eeca0b9d82f95c94bdfe70ed'
            return recover(image)

    def test_own_shadow_not_sibling_layout(self):
        result = self.check(self.fixture())
        self.assertEqual(result['field_shadow'], 0x134c4e)
        self.assertEqual(result['command_shadow'], result['field_shadow'])
        self.assertFalse(result['runtime_validated'])
        self.assertFalse(result['pcm_validated'])
        self.assertFalse(result['native_speech_validated'])

    def test_corrupt_instruction_table_or_literal_rejected(self):
        for address in (0x2c3300, 0x2c30b0 + 8 * 4, 0x2c36b4, 0x2c33d8):
            image = self.fixture()
            image[address - 0x200000] ^= 1
            with self.subTest(address=address), self.assertRaises(ValueError):
                self.check(image)

    def test_wrong_rom_rejected(self):
        with self.assertRaisesRegex(ValueError, 'NSB-6'):
            recover(b'not the acquired image')


if __name__ == '__main__':
    unittest.main()

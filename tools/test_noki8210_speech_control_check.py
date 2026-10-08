import unittest

from tools.noki8210_speech_control_check import recover, verify_call


class SpeechControlTests(unittest.TestCase):
    def test_wrong_rom_rejected(self):
        with self.assertRaises(ValueError):
            recover(b'not the acquired image')

    def test_ordered_call(self):
        text = ('8210_call_physical: action=send\n'
                'dsp_control_write: data=860b pc=002cb2ea r4=00000008 rest\n'
                '8210_call_physical: action=end\n'
                'dsp_control_write: data=840a pc=002cb2ea r4=00000008 rest\n')
        self.assertEqual(verify_call(text)['call_field'], 0x0200)
        with self.assertRaises(ValueError):
            verify_call(text.replace('data=840a', 'data=860b'))
        with self.assertRaises(ValueError):
            verify_call(text.replace('action=end', 'action=send'))

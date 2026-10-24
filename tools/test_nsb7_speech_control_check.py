import unittest

from tools.nsb7_speech_control_check import recover, verify_call


class SpeechControlTests(unittest.TestCase):
    def setUp(self):
        self.events = [
            'nsb7_outgoing_call_input: key=Send state=1 t=54',
            'nsb7_key_decode: key=0e observer_writes=0',
            'dsp_control_write: data=860b pc=002cb93e r4=00000008 ',
            'nsb7_outgoing_call_input: key=End state=1 t=66',
            'nsb7_key_decode: key=0f observer_writes=0',
            'dsp_control_write: data=840a pc=002cb93e r4=00000008 ',
        ]

    def test_ordered_physical_controls(self):
        result = verify_call('\n'.join(self.events))
        self.assertTrue(result['runtime_validated'])
        self.assertFalse(result['pcm_validated'])
        self.assertFalse(result['native_speech_validated'])

    def test_every_event_required(self):
        for index in range(len(self.events)):
            with self.subTest(index=index), self.assertRaises(ValueError):
                verify_call('\n'.join(self.events[:index] + self.events[index + 1:]))

    def test_incoming_uses_own_answer_contacts(self):
        text = '\n'.join(self.events).replace('nsb7_outgoing_call_input', 'nsb7_call_input')
        self.assertTrue(verify_call(text, incoming=True)['runtime_validated'])
        with self.assertRaises(ValueError):
            verify_call(text)

    def test_other_product_writer_rejected(self):
        with self.assertRaises(ValueError):
            verify_call('\n'.join(self.events).replace('002cb93e', '002c338e'))

    def test_reversed_controls_rejected(self):
        with self.assertRaises(ValueError):
            verify_call('\n'.join(reversed(self.events)))

    def test_wrong_rom_rejected(self):
        with self.assertRaises(ValueError):
            recover(bytes(256))


if __name__ == '__main__':
    unittest.main()

import unittest

from tools.nsb3_activation_census import callers, check_counts, inspect


class Nsb3ActivationTest(unittest.TestCase):
    def test_native_byte_lanes_and_shifted_argument_are_recovered(self):
        # MOV87; LSL3; ADD7; BL200020. The argument is043f, not an immediate.
        image = bytes.fromhex('208700c03007f000f80b4770') + bytes(32)
        result = callers(image, 0x200020)
        self.assertEqual(result['direct_bl_candidates'], 1)
        self.assertEqual(result['arguments_recovered'], 1)
        self.assertEqual(result['calls'], [{'site': '0x200006', 'input': 0x43f}])
        self.assertEqual(result['undecoded_candidates'], [])

    def test_unknown_input_stays_unknown(self):
        image = bytes.fromhex('f000f80e4770') + bytes(32)
        result = callers(image, 0x200020)
        self.assertEqual(result['direct_bl_candidates'], 1)
        self.assertEqual(result['arguments_recovered'], 0)
        self.assertIsNone(result['calls'][0]['input'])

    def test_absence_requires_explicit_unique_zero_count_control(self):
        text = 'nsb3_radio_activation_counts: controller=0 initializer=0 input13a3=0 construct13a3=0 observer_writes=0'
        check_counts(text)
        for bad in ('', text + '\n' + text, text.replace('initializer=0', 'initializer=1'),
                    text + '\nnsb3_radio_activation: site=initializer', text.replace('observer_writes=0', 'observer_writes=1')):
            with self.assertRaises(ValueError):
                check_counts(bad)
        with self.assertRaises(ValueError):
            inspect(b'not own image')


if __name__ == '__main__':
    unittest.main()

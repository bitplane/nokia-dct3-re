import unittest

from tools.nsb3_activation_census import callers, check_counts, check_upstream, completion_routes, inspect
from pathlib import Path


class Nsb3ActivationTest(unittest.TestCase):
    def test_own_completion_selectors_and_mutation_guard(self):
        path = Path('roms/noki6190/6190f613a.fls')
        if not path.exists():
            self.skipTest('optional own NSB-3 ROM not installed')
        image = path.read_bytes()
        result = completion_routes(image)
        self.assertEqual(result['input03f1']['required_value'], 0x1f)
        self.assertEqual(result['input03e9']['selector4_prior_states'], [6, 7])
        for address in (0x2112e4, 0x213a30, 0x213bfa, 0x211bdc):
            changed = bytearray(image)
            changed[address - 0x200000] ^= 1
            with self.assertRaises(ValueError):
                completion_routes(changed)

    def test_upstream_requires_request_start_publication_and_uncapped_control(self):
        text = '\n'.join([
            'nsb3_radio_upstream: header=03e90100 caller=00214931 count=1 observer_writes=0',
            'nsb3_radio_service_start: mode=00000002 caller=00213f41 count=2 observer_writes=0',
            'nsb3_radio_service_post: object=00101d44 header=0002441a body=00419800 caller=002128a1 count=3 observer_writes=0',
            'nsb3_radio_upstream_count: count=3 cap=64 observer_writes=0'])
        check_upstream(text)
        for bad in (text.replace('03e90100', '03e90200'), text.replace('mode=00000002', 'mode=00000001'),
                    text.replace('body=00419800', 'body=00000000'), text.replace('object=00101d44', 'object=00201d44'),
                    text.replace('count=3 cap=64', 'count=64 cap=64')):
            with self.assertRaises(ValueError):
                check_upstream(bad)

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

import unittest
from pathlib import Path
from tools.noki6210_accessory_contract import verify


class AccessoryContractTest(unittest.TestCase):
    def test_rejects_other_image(self):
        with self.assertRaisesRegex(ValueError, 'acquired NPE-3'):
            verify(b'')

    def test_own_decision_thresholds(self):
        path = Path(__file__).resolve().parents[1] / 'roms/noki6210/6210_556c.fls'
        if not path.exists():
            self.skipTest('own acquired NPE-3 image unavailable')
        result = verify(path.read_bytes())
        self.assertEqual(result['state_0f_interval'], [300, 500])
        self.assertEqual((result['decision_threshold'], result['cleanup_threshold']), (786, 912))
        self.assertEqual((result['selector'], result['state'], result['sample']),
                         (0, 0x173a97, 0x173aa2))
        self.assertEqual(result['context_setter'], 0x41a944)
        self.assertEqual(result['parameter_context'], 0x16ed5c)
        self.assertEqual(result['context_initializer'], 0x41ac32)
        self.assertEqual(len(result['context_setter_direct_calls']), 8)
        self.assertEqual(result['context_setter_direct_calls'][0x39d786], 1)
        self.assertEqual(result['context_transition_pairs'],
                         {0x0f: 0, 0x10: 1, 0x20: 2, 0x80: 3, 0x84: 4})
        self.assertFalse(result['indirect_writer_coverage'])
        self.assertFalse(result['pcm_semantics_validated'])

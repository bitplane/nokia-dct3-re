import unittest
from tools.noki5510_bootstrap_check import CHAIN, verify


class BootstrapCheckTest(unittest.TestCase):
    def test_ordered_native_boundary(self):
        verify('\n'.join(CHAIN))

    def test_missing_reordered_or_forced_evidence(self):
        for text in ('\n'.join(CHAIN[:-1]), '\n'.join(reversed(CHAIN)),
                     '\n'.join(CHAIN) + '\nruntime_hle_handoff'):
            with self.assertRaises(ValueError):
                verify(text)


if __name__ == '__main__':
    unittest.main()

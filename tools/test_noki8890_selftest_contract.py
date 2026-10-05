import unittest
from tools.noki8890_selftest_contract import verify


class ContractTest(unittest.TestCase):
    def test_rejects_unpinned_image(self):
        with self.assertRaisesRegex(ValueError, 'acquired 8890'):
            verify(bytes(0x1d0000))

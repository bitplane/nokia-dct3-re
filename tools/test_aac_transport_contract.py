import unittest

from aac_transport_contract import program_configuration_prefix


class TransportContractTest(unittest.TestCase):
    def test_original_al00_configuration_relocates_with_new_alignment(self):
        self.assertEqual(program_configuration_prefix(bytes.fromhex('1200050400000000')),
                         bytes.fromhex('a0a08000000000'))

    def test_comment_is_preserved_after_recomputed_alignment(self):
        self.assertEqual(program_configuration_prefix(bytes.fromhex('12000504000000024142')),
                         bytes.fromhex('a0a080000000024142'))

    def test_truncation_and_unhandled_tail_are_not_ignored(self):
        asc = bytes.fromhex('1200050400000000')
        for size in range(len(asc)):
            with self.subTest(size=size), self.assertRaises(ValueError):
                program_configuration_prefix(asc[:size])
        with self.assertRaises(ValueError):
            program_configuration_prefix(asc + b'\x00')

    def test_unsupported_or_conflicting_configuration_fails_closed(self):
        for value in ('1210050400000000', '1201050400000000',
                      '1200090400000000', '1200050400000100'):
            with self.subTest(value=value), self.assertRaises(ValueError):
                program_configuration_prefix(bytes.fromhex(value))


if __name__ == '__main__':
    unittest.main()

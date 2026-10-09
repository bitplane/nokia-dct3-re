import unittest

from tools.c54x_rom4_control_source_check import check
from tools.test_c54x_rom4_rf_boundary_check import summary


def capture():
    text = summary()
    for hit in (1, 2):
        text += (f'rom4_rf_control_table: hit={hit} base=1920 '
                 'words=2a04,0006,0041,0040,27a2,0030,0041,0020 '
                 'pointer197b=1914 pointer197d=1920 '
                 'setup=76f8,197b,1914,76f8,197d,1920,fc00\n')
        for pc in ('a22f', 'a23e'):
            text += f'rom4_rf_operand: pc={pc} hit={hit} sp=1ec4\n'
    text += ('rom4_rf_operand: pc=4025 hit=1 sp=1ec4\n'
             'rom4_rf_control_accumulator: a=0000302813 b=0000302813 '
             'al=2813 ah=0030 routine=75f8,0008,0031,f495,f495,75f8,0009,0032\n')
    return text


class ControlSourceCheckTest(unittest.TestCase):
    def test_accepts_bounded_sources_and_native_boundary(self):
        self.assertEqual(check(capture())['rf_port32_writes'], 3)

    def test_rejects_changed_pointer_table_or_setup(self):
        for old, new in (('pointer197d=1920', 'pointer197d=1922'),
                         ('words=2a04', 'words=2a05'), ('197b,1914', '197b,1915')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check(capture().replace(old, new))

    def test_rejects_missing_or_recursive_observations(self):
        for text in (capture().replace('hit=2 base', 'hit=3 base'),
                     capture() + 'rom4_rf_operand: pc=4025 hit=2 sp=1ec4\n',
                     capture() + 'rom4_rf_control_table: hit=3 base=1920 words=0000 pointer197b=1914 pointer197d=1920 setup=fc00\n'):
            with self.assertRaises(ValueError):
                check(text)

    def test_rejects_wrong_accumulator_or_opcode(self):
        for old, new in (('al=2813', 'al=2814'), ('ah=0030', 'ah=0031'),
                         ('75f8,0009,0032', '75f8,0009,0031')):
            with self.assertRaises(ValueError):
                check(capture().replace(old, new))

    def test_requires_existing_rf_boundary(self):
        with self.assertRaises(ValueError):
            check(capture().replace('rf_reads=207040', 'rf_reads=0'))


if __name__ == '__main__':
    unittest.main()

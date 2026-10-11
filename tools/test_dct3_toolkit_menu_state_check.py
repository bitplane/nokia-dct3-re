import unittest

from tools.dct3_toolkit_menu_state_check import verify_state
from tools.test_noki8210_toolkit_menu_state_check import complete


class SharedMenuStateTest(unittest.TestCase):
    def test_each_product(self):
        verify_state(complete(), '8210', 'toolkit_physical')
        text = complete().replace('8210', '8850').replace('toolkit_physical', 'toolkit_interactive')
        verify_state(text, '8850', 'toolkit_interactive')

    def test_exact_restoration(self):
        for original, replacement in (
            ('event=restored pc=1c', 'event=restored pc=20'),
            ('event=restored pc=1c sp=1384b4', 'event=restored pc=1c sp=1384b8'),
            ('event=restored pc=1c sp=1384b4 ram=edbb543a',
             'event=restored pc=1c sp=1384b4 ram=edbb543b'),
        ):
            with self.subTest(original=original), self.assertRaises(ValueError):
                verify_state(complete().replace(original, replacement), '8210', 'toolkit_physical')

    def test_missing_or_duplicate_interval(self):
        marker = 'state_replay: phase=restored event=begin'
        for text in (complete().replace(marker, 'ignored'), complete() + '\n' + marker):
            with self.assertRaises(ValueError):
                verify_state(text, '8210', 'toolkit_physical')

    def test_wrong_product(self):
        with self.assertRaises(ValueError):
            verify_state(complete(), '8850', 'toolkit_interactive')


if __name__ == '__main__':
    unittest.main()

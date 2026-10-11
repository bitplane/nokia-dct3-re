import unittest

from tools.dct3_toolkit_check import menu_events
from tools.noki8850_toolkit_menu_state_check import verify
from tools.test_noki8210_toolkit_menu_state_check import complete as state_replay


def complete():
    replay = state_replay().split('8210_menu_state: event=saved', 1)[1]
    replay = ('8850_menu_state: event=saved' + replay).replace('8210', '8850')
    replay = replay.replace('toolkit_physical', 'toolkit_interactive')
    prefix = menu_events('8850')
    prefix = prefix[:prefix.index('8850_toolkit_interactive: action=menu_select')]
    return '\n'.join(prefix) + '\n' + replay


class OwnMenuStateTest(unittest.TestCase):
    def test_complete(self):
        verify(complete())

    def test_wrong_terminal_profile(self):
        with self.assertRaises(ValueError):
            verify(complete().replace('ins=10 p1=00 p2=00 p3=09',
                                      'ins=10 p1=00 p2=00 p3=05'))

    def test_missing_menu_install(self):
        with self.assertRaises(ValueError):
            verify(complete().replace('terminal-response data=810304250002028281030100',
                                      'terminal-response missing'))

    def test_restored_selection_failures(self):
        before, after = complete().split('state_replay: phase=restored event=begin', 1)
        for original, replacement in (
            ('action=menu_select phase=restored', 'action=ignored'),
            ('d30702020181100101', 'd30702020181100102'),
            ('SIM status ins=c2 sw=9000', 'SIM status ins=c2 sw=9300'),
        ):
            with self.subTest(original=original), self.assertRaises(ValueError):
                verify(before + 'state_replay: phase=restored event=begin'
                       + after.replace(original, replacement))


if __name__ == '__main__':
    unittest.main()

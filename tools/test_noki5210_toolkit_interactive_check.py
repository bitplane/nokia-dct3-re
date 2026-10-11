import unittest
from tools.dct3_toolkit_check import interactive_events
from tools.noki5210_toolkit_interactive_check import verify


def complete():
    return '\n'.join(interactive_events('5210')).replace(
        '5210_toolkit_interactive: action=inkey_5',
        '5210_toolkit_interactive: action=inkey_5\n5210_toolkit_interactive: action=inkey_confirm')


class OwnToolkitTest(unittest.TestCase):
    def test_complete(self):
        verify(complete())

    def test_wrong_reply_or_missing_confirmation(self):
        for original, replacement in (
            ('action=inkey_confirm', 'action=missing'),
            ('0d020435', '0d020436'),
            ('0d03043432', '0d03043433'),
            ('ins=10 p1=00 p2=00 p3=09', 'ins=10 p1=00 p2=00 p3=05'),
        ):
            with self.subTest(original=original), self.assertRaises(ValueError):
                verify(complete().replace(original, replacement))


if __name__ == '__main__':
    unittest.main()

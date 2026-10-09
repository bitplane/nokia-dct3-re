import unittest

from tools.dct3_toolkit_check import interactive_events, verify_interactive


class InteractiveToolkitTest(unittest.TestCase):
    def trace(self):
        return '\n'.join(interactive_events('6250'))

    def test_ordered_card_owned_commands(self):
        verify_interactive(self.trace(), '6250')
        verify_interactive('\n'.join(interactive_events('6210')), '6210')

    def test_wrong_response_text_result_and_pending_length(self):
        for old, new in (('0d020435', '0d020436'), ('0d03043432', '0d03043433'),
                         ('sw=911a', 'sw=9000'), ('0301000d03', '0301010d03')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                verify_interactive(self.trace().replace(old, new), '6250')

    def test_wrong_input_order_or_product(self):
        with self.assertRaises(ValueError):
            verify_interactive(self.trace().replace('action=input_4', 'action=input_2', 1), '6250')
        with self.assertRaises(ValueError):
            verify_interactive(self.trace(), '6210')

    def test_lua_error_rejected(self):
        with self.assertRaises(ValueError):
            verify_interactive(self.trace() + '\n[LUA ERROR]', '6250')


if __name__ == '__main__':
    unittest.main()

import unittest

from tools.dct3_toolkit_check import interactive_events, verify_interactive, menu_events, verify_menu


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

    def test_menu_round_trip(self):
        verify_menu('\n'.join(menu_events('6210')), '6210')

    def test_menu_wrong_item_result_and_fetch_length(self):
        trace = '\n'.join(menu_events('6210'))
        for old, new in (('d30702020181100101', 'd30702020181100102'),
                         ('810304250002028281030100', '810304250002028281030101'),
                         ('sw=9128', 'sw=9000'), ('action=menu_exit', 'action=absent')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                verify_menu(trace.replace(old, new), '6210')


if __name__ == '__main__':
    unittest.main()

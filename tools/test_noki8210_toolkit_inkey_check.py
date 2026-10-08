import unittest
from tools.noki8210_toolkit_inkey_check import EVENTS, INPUT_EVENTS, verify_protocol


class InkeyTest(unittest.TestCase):
    def test_get_input_sequence(self):
        verify_protocol('\n'.join(INPUT_EVENTS), get_input=True)
        for index in range(len(INPUT_EVENTS)):
            with self.subTest(index=index), self.assertRaises(ValueError):
                verify_protocol('\n'.join(INPUT_EVENTS[:index] + INPUT_EVENTS[index + 1:]), get_input=True)
        with self.assertRaises(ValueError):
            verify_protocol('\n'.join(INPUT_EVENTS).replace('0d03043432', '0d03043433'), get_input=True)

    def test_complete_exchange(self):
        verify_protocol('\n'.join(EVENTS))
        verify_protocol('\n'.join('[:sim_card] ' + event for event in EVENTS))

    def test_every_link_required(self):
        for index in range(len(EVENTS)):
            with self.subTest(index=index), self.assertRaises(ValueError):
                verify_protocol('\n'.join(EVENTS[:index] + EVENTS[index + 1:]))

    def test_wrong_digit_product_and_error(self):
        log = '\n'.join(EVENTS)
        for text in (log.replace('0d020435', '0d020436'),
                     log.replace('8210_', '3210_'), log + '\n[LUA ERROR]'):
            with self.assertRaises(ValueError):
                verify_protocol(text)


if __name__ == '__main__':
    unittest.main()

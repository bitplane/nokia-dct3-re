import unittest
from tools.noki8890_toolkit_busy_check import EVENTS, verify_protocol


class ToolkitBusyTest(unittest.TestCase):
    def test_complete_exchange(self):
        verify_protocol('\n'.join(EVENTS))
        verify_protocol('\n'.join('[:sim_card] ' + event for event in EVENTS))

    def test_every_link_required(self):
        for index in range(len(EVENTS)):
            with self.subTest(index=index), self.assertRaises(ValueError):
                verify_protocol('\n'.join(EVENTS[:index] + EVENTS[index + 1:]))

    def test_success_cannot_pass_busy_gate(self):
        text = '\n'.join(EVENTS)
        for old, new in (('03022001', '030100'), ('p3=0d', 'p3=0c'),
                         ('8890_clock', '8850_clock'), ('p3=09', 'p3=05')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                verify_protocol(text.replace(old, new))

    def test_response_order_and_lua_error(self):
        events = list(EVENTS)
        events[7], events[8] = events[8], events[7]
        with self.assertRaises(ValueError):
            verify_protocol('\n'.join(events))
        with self.assertRaises(ValueError):
            verify_protocol('\n'.join(EVENTS) + '\n[LUA ERROR]')


if __name__ == '__main__':
    unittest.main()

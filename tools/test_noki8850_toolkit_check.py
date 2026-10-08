import unittest
from tools.noki8850_toolkit_check import EVENTS, verify_protocol


class ToolkitProtocolTest(unittest.TestCase):
    def test_complete_exchange(self):
        verify_protocol('\n'.join(EVENTS))
        verify_protocol('\n'.join('[:sim_card] ' + event for event in EVENTS))

    def test_every_link_required(self):
        for index in range(len(EVENTS)):
            with self.subTest(index=index), self.assertRaises(ValueError):
                verify_protocol('\n'.join(EVENTS[:index] + EVENTS[index + 1:]))

    def test_wrong_product_profile_and_result(self):
        text = '\n'.join(EVENTS)
        for old, new in (('p3=09', 'p3=05'), ('8850_toolkit', '6250_toolkit'),
                         ('030100', '030120'), ('sw=9116', 'sw=9000')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                verify_protocol(text.replace(old, new))
        with self.assertRaises(ValueError):
            verify_protocol(text + '\n[LUA ERROR]')

    def test_physical_dismissal_precedes_response(self):
        events = list(EVENTS)
        events[6], events[7] = events[7], events[6]
        with self.assertRaises(ValueError):
            verify_protocol('\n'.join(events))


if __name__ == '__main__':
    unittest.main()

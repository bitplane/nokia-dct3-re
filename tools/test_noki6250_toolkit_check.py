import unittest
from tools.run_noki6250_acceptance import check_toolkit_protocol


EVENTS = [
    'read-binary fid=6fae offset=0 length=1 first=03',
    'header cla=a0 ins=10 p1=00 p2=00 p3=09',
    'SIM status ins=10 sw=9000',
    'proactive DISPLAY TEXT ready',
    'SIM completion ins=f2 sw=9116',
    'header cla=a0 ins=12 p1=00 p2=00 p3=16',
    '6250_toolkit_physical: action=dismiss',
    'header cla=a0 ins=14 p1=00 p2=00 p3=0c',
    'terminal-response data=810301218002028281030100',
    'SIM status ins=14 sw=9000',
]


class ToolkitTest(unittest.TestCase):
    def test_complete(self):
        check_toolkit_protocol('\n'.join(EVENTS))

    def test_every_event_required(self):
        for index in range(len(EVENTS)):
            with self.subTest(index=index), self.assertRaises(ValueError):
                check_toolkit_protocol('\n'.join(EVENTS[:index] + EVENTS[index + 1:]))

    def test_product_profile_result_and_physical_order(self):
        text = '\n'.join(EVENTS)
        for old, new in (('6250_', '6210_'), ('p3=09', 'p3=05'), ('030100', '030120'),
                         ('sw=9116', 'sw=9000')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_toolkit_protocol(text.replace(old, new))
        reordered = EVENTS.copy()
        reordered[6], reordered[8] = reordered[8], reordered[6]
        with self.assertRaises(ValueError):
            check_toolkit_protocol('\n'.join(reordered))


if __name__ == '__main__':
    unittest.main()

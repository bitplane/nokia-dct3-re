import unittest
from tools.noki6210_toolkit_check import verify


EVENTS = [
    'read-binary fid=6fae offset=0 length=1 first=03',
    'header cla=a0 ins=10 p1=00 p2=00 p3=09',
    'SIM status ins=10 sw=9000',
    'proactive DISPLAY TEXT ready',
    'SIM completion ins=f2 sw=9116',
    'header cla=a0 ins=12 p1=00 p2=00 p3=16',
    '6210_toolkit_physical: action=dismiss',
    'header cla=a0 ins=14 p1=00 p2=00 p3=0c',
    'terminal-response data=810301218002028281030100',
    'SIM status ins=14 sw=9000',
]


class ToolkitTest(unittest.TestCase):
    def test_complete(self):
        verify('\n'.join(EVENTS))

    def test_missing_checkpoint(self):
        for index in range(len(EVENTS)):
            with self.subTest(index=index), self.assertRaises(ValueError):
                verify('\n'.join(EVENTS[:index] + EVENTS[index + 1:]))

    def test_wrong_profile_length_status_or_result(self):
        text = '\n'.join(EVENTS)
        for old, new in (('p3=09', 'p3=05'), ('sw=9116', 'sw=9000'),
                         ('030100', '03022001')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                verify(text.replace(old, new))

    def test_fragmented_device_log_is_normalized(self):
        text = '\n'.join(EVENTS).replace('data=810301218002028281030100',
            'data=' + ''.join('[:sim_card] ' + byte for byte in
                             ('81', '03', '01', '21', '80', '02', '02', '82', '81', '03', '01', '00')))
        verify(text)

    def test_response_before_physical_clearance(self):
        order = EVENTS.copy()
        order[6], order[8] = order[8], order[6]
        with self.assertRaises(ValueError):
            verify('\n'.join(order))


if __name__ == '__main__':
    unittest.main()

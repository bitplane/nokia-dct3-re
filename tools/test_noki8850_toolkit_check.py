import unittest
from tools.noki8850_toolkit_check import EVENTS, verify_protocol
from tools.dct3_toolkit_check import interactive_events, menu_events


class ToolkitProtocolTest(unittest.TestCase):
    def test_call_decline_requires_refusal_and_no_outgoing_request(self):
        events = (*menu_events('8850', selection_status='911c')[:-1],
                  'proactive SET UP CALL ready', 'SIM status ins=c2 sw=911c',
                  'header cla=a0 ins=12 p1=00 p2=00 p3=1c',
                  '8850_toolkit_interactive: action=call_decline',
                  'terminal-response data=810306100002028281030122',
                  'SIM status ins=14 sw=9000',
                  '8850_toolkit_interactive: action=menu_exit')
        text = '\n'.join(events)
        verify_protocol(text, call_decline=True)
        for changed in (text.replace('030122', '030100'),
                        text.replace('action=call_decline', 'action=call_accept'),
                        text + '\nGSM outgoing request id=1 digits=5551234'):
            with self.assertRaises(ValueError):
                verify_protocol(changed, call_decline=True)

    def test_menu_requires_complete_ordered_exchange(self):
        events = menu_events('8850')
        verify_protocol('\n'.join(events), menu=True)
        for index in range(len(events)):
            with self.subTest(index=index), self.assertRaises(ValueError):
                verify_protocol('\n'.join(events[:index] + events[index + 1:]), menu=True)
        with self.assertRaises(ValueError):
            verify_protocol('\n'.join(events).replace('d30702020181100101', 'd30702020181100102'), menu=True)
    def test_interactive_exchange_requires_every_link(self):
        events = interactive_events('8850')
        verify_protocol('\n'.join(events), interactive=True)
        for index in range(len(events)):
            with self.subTest(index=index), self.assertRaises(ValueError):
                verify_protocol('\n'.join(events[:index] + events[index + 1:]), interactive=True)
        for old, new in (('0d020435', '0d020436'), ('0d03043432', '0d03043433'),
                         ('8850_toolkit', '8210_toolkit')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                verify_protocol('\n'.join(events).replace(old, new), interactive=True)

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

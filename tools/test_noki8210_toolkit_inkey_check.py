import unittest
from tools.noki8210_toolkit_inkey_check import EVENTS, INPUT_EVENTS, MENU_EVENTS, verify_protocol


class InkeyTest(unittest.TestCase):
    def test_card_owned_call_requires_physical_consent_and_release(self):
        from tools.test_sim_toolkit_call_trace_check import GOOD
        call = GOOD.replace('GSM service uplink sapi=0 pd=03 message=05',
                            '8210_toolkit_physical: action=call_accept\nGSM service uplink sapi=0 pd=03 message=05')
        call = call.replace('GSM service uplink sapi=0 pd=03 message=25',
                            '8210_toolkit_physical: action=menu_exit\nGSM service uplink sapi=0 pd=03 message=25')
        text = '\n'.join(MENU_EVENTS[:-1]) + '\n' + call
        verify_protocol(text, call=True)
        for changed in (text.replace('action=call_accept', 'action=ignored'),
                        text.replace('action=menu_exit', 'action=ignored'),
                        text.replace('81551532f4', '81214365f7'),
                        text.replace('terminal-response data=810306100002028281030100', 'wrong response')):
            with self.assertRaises(ValueError):
                verify_protocol(changed, call=True)

    def test_card_owned_sms_completion(self):
        from tools.test_sim_toolkit_sms_trace_check import GOOD
        sms = GOOD.replace('data=2901', 'data=3901').replace('cp=29', 'cp=39').replace(
            'data=2904', 'data=3904').replace('090e01000781', '090e01010781')
        text = '\n'.join(MENU_EVENTS[:-1]) + '\n' + sms + '\n8210_toolkit_physical: action=menu_exit'
        verify_protocol(text, sms=True)
        for changed in (text.replace('3534154', '3534155'),
                        text.replace('menu_exit', 'ignored'),
                        text.replace('terminal-response data=810305130002028281030100', 'wrong response'),
                        text + '\ngsm_sms_submit: duplicate'):
            with self.assertRaises(ValueError):
                verify_protocol(changed, sms=True)

    def test_menu_sequence_and_missing_links(self):
        verify_protocol('\n'.join(MENU_EVENTS), menu=True)
        for index in range(len(MENU_EVENTS)):
            with self.subTest(index=index), self.assertRaises(ValueError):
                verify_protocol('\n'.join(MENU_EVENTS[:index] + MENU_EVENTS[index + 1:]), menu=True)
        for wrong in ('d30702020181100102', 'd30702028101100101'):
            with self.assertRaises(ValueError):
                verify_protocol('\n'.join(MENU_EVENTS).replace('d30702020181100101', wrong), menu=True)

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

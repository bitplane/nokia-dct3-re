import unittest
from unittest.mock import patch
from tools.dct3_toolkit_check import display_text_events,interactive_events,menu_events
from tools.nsb7_toolkit_check import check,check_card,SMSS_OFFSET


def fixture():
    rows=list(display_text_events('nsb7'))
    rows.insert(rows.index('nsb7_toolkit_physical: action=dismiss')+1,
                'nsb7_key_decode: key=19 observer_writes=0')
    return '\n'.join(rows)


def interactive_fixture(menu=False):
    rows=list((menu_events if menu else interactive_events)('nsb7'))
    rows.insert(rows.index('header cla=a0 ins=14 p1=00 p2=00 p3=10'),
                'nsb7_toolkit_interactive: action=inkey_confirm')
    if menu:
        rows.insert(rows.index('terminal-response data=810304250002028281030100'),
                    'header cla=a0 ins=14 p1=00 p2=00 p3=0c')
        rows.insert(rows.index('nsb7_toolkit_interactive: action=menu_last'),
                    'nsb7_toolkit_interactive: action=menu_skip_netmonitor')
    keys={'dismiss':'19','inkey_5':'05','inkey_confirm':'19',
          'input_4':'04','input_2':'02','confirm':'19','menu':'19',
          'menu_skip_netmonitor':'17','menu_last':'17','menu_open':'19',
          'menu_select':'19','menu_exit':'0f'}
    expanded=[]
    for row in rows:
        expanded.append(row)
        if row.startswith('nsb7_toolkit_interactive: action='):
            expanded.append('nsb7_key_decode: key='+keys[row.split('=')[1]]+' observer_writes=0')
    return '\n'.join(expanded)


def call_fixture(decline=False):
    text=interactive_fixture(menu=True).replace('SIM status ins=c2 sw=9000',
        'menu selection item=1 accepted\nproactive SET UP CALL ready\nSIM status ins=c2 sw=911c')
    exit_event='nsb7_toolkit_interactive: action=menu_exit\nnsb7_key_decode: key=0f observer_writes=0'
    header='header cla=a0 ins=12 p1=00 p2=00 p3=1c'
    if decline:
        tail='\n'.join((header,'nsb7_toolkit_interactive: action=call_decline',
            'nsb7_key_decode: key=1a observer_writes=0',
            'header cla=a0 ins=14 p1=00 p2=00 p3=0c',
            'terminal-response data=810306100002028281030122',
            'SIM status ins=14 sw=9000',exit_event))
    else:
        from tools.test_sim_toolkit_call_trace_check import GOOD
        tail=header+GOOD.split(header,1)[1]
        setup='GSM service uplink sapi=0 pd=03 message=05 '
        tail=tail.replace(setup,'nsb7_toolkit_interactive: action=call_accept\n'
            'nsb7_key_decode: key=19 observer_writes=0\n'+setup)
        tail=tail.replace('terminal-response data=810306',
            'header cla=a0 ins=14 p1=00 p2=00 p3=0c\nterminal-response data=810306')
        disconnect='GSM service uplink sapi=0 pd=03 message=25 '
        tail=tail.replace(disconnect,exit_event+'\n'+disconnect)
    return text.replace(exit_event,tail)


class ToolkitTest(unittest.TestCase):
    @patch('tools.nsb7_toolkit_check.check_authentication')
    def test_call_consent_network_and_physical_release(self,authentication):
        text=call_fixture()
        check(text,call=True)
        setup=next(line for line in text.splitlines() if 'message=05 length=15' in line)
        early=text.replace(setup+'\n','').replace('nsb7_toolkit_interactive: action=call_accept',
            setup+'\nnsb7_toolkit_interactive: action=call_accept')
        for bad in (early,text.replace('action=call_accept','action=missing'),
                    text.replace('810306100002028281030100','810306100002028281030120'),
                    text.replace('message=25 length=5','message=24 length=5'),
                    text+'\nGSM outgoing request id=2 digits=5551234'):
            with self.subTest(text=bad),self.assertRaises(ValueError):
                check(bad,call=True)

    @patch('tools.nsb7_toolkit_check.check_authentication')
    def test_declined_call_must_not_start_network(self,authentication):
        text=call_fixture(decline=True)
        check(text,decline=True)
        for bad in (text.replace('030122','030100'),text.replace('key=1a','key=19'),
                    text+'\nGSM outgoing request id=1 digits=5551234'):
            with self.subTest(text=bad),self.assertRaises(ValueError):
                check(bad,decline=True)

    def test_sms_storage_allows_only_exact_reference_change(self):
        from tools.nsb7_pin_check import serialized_profile
        from tools.nsb7_pin_registration_check import LOCI
        initial=bytearray(serialized_profile())
        initial[1600:1611]=LOCI
        final=initial.copy()
        final[SMSS_OFFSET]=0
        check_card(bytes(initial),bytes(final),sms=True)
        check_card(bytes(initial),bytes(initial))
        wrong=final.copy()
        wrong[0]^=1
        for bad in (bytes(initial),bytes(wrong),bytes(final[:-1])):
            with self.subTest(card=bad[:8]),self.assertRaises(ValueError):
                check_card(bytes(initial),bad,sms=True)

    @patch('tools.nsb7_toolkit_check.check_authentication')
    def test_menu_sms_requires_network_and_terminal_completion(self,authentication):
        from tools.test_sim_toolkit_sms_trace_check import GOOD
        text=interactive_fixture(menu=True).replace('SIM status ins=c2 sw=9000',
            'menu selection item=1 accepted\nproactive SEND SHORT MESSAGE ready\nSIM status ins=c2 sw=9124')
        header='header cla=a0 ins=12 p1=00 p2=00 p3=24'
        tail=header+GOOD.split(header,1)[1]
        tail=tail.replace('data=2901','data=3901').replace('cp=29','cp=39').replace('data=2904','data=3904')
        tail=tail.replace('terminal-response data=810305',
            'header cla=a0 ins=14 p1=00 p2=00 p3=0c\nterminal-response data=810305')
        tail+='\nupdate-binary fid=6f43 offset=0 length=2 t=87.0\n'
        text=text.replace('nsb7_toolkit_interactive: action=menu_exit',
                          tail+'nsb7_toolkit_interactive: action=menu_exit')
        check(text,sms=True)
        exit_event='nsb7_toolkit_interactive: action=menu_exit\nnsb7_key_decode: key=0f observer_writes=0'
        early=text.replace(exit_event,'').replace('GSM service downlink kind=18',
                    exit_event+'\nGSM service downlink kind=18')
        for bad in (text.replace('downlink kind=18','downlink kind=19'),
                    text.replace('810305130002028281030100','810305130002028281030120'),
                    text.replace('fid=6f43 offset=0 length=2','fid=6f43 offset=0 length=1'),
                    text+'\ngsm_sms_submit:',early):
            with self.subTest(text=bad),self.assertRaises(ValueError):
                check(bad,sms=True)

    @patch('tools.nsb7_toolkit_check.check_authentication')
    def test_menu_envelope_and_product_navigation(self,authentication):
        text=interactive_fixture(menu=True)
        check(text,menu=True)
        for bad in (text.replace('d30702020181100101','d30702020181100102'),
                    text.replace('action=menu_skip_netmonitor','action=missing'),
                    text.replace('key=17','key=18'),
                    text+'\nheader cla=a0 ins=c2 p1=00 p2=00 p3=09'):
            with self.subTest(text=bad),self.assertRaises(ValueError):
                check(bad,menu=True)

    @patch('tools.nsb7_toolkit_check.check_authentication')
    def test_interactive_product_specific_confirmation(self,authentication):
        text=interactive_fixture()
        check(text,interactive=True)
        for bad in (text.replace('action=inkey_confirm','action=missing'),
                    text.replace('key=02','key=03'),
                    text.replace('nsb7_key_decode: key=19 observer_writes=0','',1)+
                    '\nnsb7_key_decode: key=19 observer_writes=0',
                    text.replace('0d03043432','0d03043433')):
            with self.subTest(text=bad),self.assertRaises(ValueError):
                check(bad,interactive=True)

    @patch('tools.nsb7_toolkit_check.check_authentication')
    def test_successful_physical_dismissal(self,authentication):
        text=fixture()
        check(text)
        authentication.assert_called_once_with(text.split('nsb7_toolkit_physical:',1)[0])

    @patch('tools.nsb7_toolkit_check.check_authentication')
    def test_timeout_wrong_key_and_missing_negotiation_rejected(self,authentication):
        for bad in (fixture().replace('810301218002028281030100','810301218002028281030112'),
                    fixture().replace('key=19','key=17'),
                    fixture().replace('sw=9116','sw=9000'),
                    fixture().replace('ins=10','ins=11'),
                    fixture()+'\nheader cla=a0 ins=12 p1=00 p2=00 p3=16'):
            with self.subTest(text=bad),self.assertRaises(ValueError):
                check(bad)

    @patch('tools.nsb7_toolkit_check.check_authentication')
    def test_response_before_physical_input_rejected(self,authentication):
        with self.assertRaises(ValueError):
            check(fixture().replace('nsb7_toolkit_physical: action=dismiss','')+
                  '\nnsb7_toolkit_physical: action=dismiss')

    def test_authentication_is_not_optional(self):
        with self.assertRaises(ValueError):
            check(fixture())


if __name__=='__main__':
    unittest.main()

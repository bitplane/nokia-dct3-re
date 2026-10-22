import unittest
import re
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
    def test_explicit_capability_rejections_and_clean_recovery(self,authentication):
        for mode,name,length,response in (
                ('events','SET UP EVENT LIST','0f','810308050002028281030131'),
                ('timer','TIMER MANAGEMENT','13','81030f270002028281030131')):
            text=interactive_fixture(menu=True).replace('SIM status ins=c2 sw=9000',
                f'menu selection item=1 accepted\nproactive {name} ready\nSIM status ins=c2 sw=91{length}')
            exit_event='nsb7_toolkit_interactive: action=menu_exit'
            tail='\n'.join((f'header cla=a0 ins=12 p1=00 p2=00 p3={length}',
                'header cla=a0 ins=14 p1=00 p2=00 p3=0c',
                f'terminal-response data={response}','SIM status ins=14 sw=9000',
                'ccont_rtc: event=second time=13:49:33 day=0 t=108.0',exit_event))
            text=text.replace(exit_event,tail)
            with self.subTest(mode=mode):
                check(text,mode=mode)
            for bad in (text.replace(response,response[:-2]+'00'),
                        text+'\nenvelope data=d60100',text+'\nenvelope data=d70100',
                        text+'\nGSM outgoing request id=1 digits=5551234',
                        text+'\ngsm_sms_submit: unexpected',
                        text.replace('t=108.0','t=100.0')):
                with self.subTest(mode=mode,bad=bad),self.assertRaises(ValueError):
                    check(bad,mode=mode)

    @patch('tools.nsb7_toolkit_check.check_authentication')
    def test_refresh_reprofiles_and_completes_new_application(self,authentication):
        from tools.test_sim_toolkit_refresh_trace_check import GOOD
        text=interactive_fixture(menu=True).replace('SIM status ins=c2 sw=9000',
            'proactive REFRESH ready\nSIM status ins=c2 sw=910b')
        exit_event='nsb7_toolkit_interactive: action=menu_exit\nnsb7_key_decode: key=0f observer_writes=0'
        header='header cla=a0 ins=12 p1=00 p2=00 p3=0b'
        refresh=header+GOOD.split(header,1)[1]
        refresh=refresh.replace('p3=05','p3=09').replace('length=5','length=9')
        refresh=refresh.replace('terminal-response data=810309',
            'header cla=a0 ins=14 p1=00 p2=00 p3=0c\nterminal-response data=810309')
        fetch='header cla=a0 ins=12 p1=00 p2=00 p3=16'
        resumed=fetch+interactive_fixture(menu=True).split(fetch,1)[1]
        resumed=resumed.split('nsb7_toolkit_interactive: action=menu',1)[0]
        names={'dismiss':'refresh_dismiss','inkey_5':'refresh_inkey5',
               'inkey_confirm':'refresh_inkey_confirm','input_4':'refresh_input4',
               'input_2':'refresh_input2','confirm':'refresh_confirm'}
        resumed=re.sub(r'action=(\w+)',lambda match:'action='+names[match[1]],resumed)
        text=text.replace(exit_event,refresh+'\n'+resumed+'\n'+exit_event)
        check(text,mode='refresh')
        for bad in (text.replace('p3=16','p3=00'),
                    text.replace('header cla=a0 ins=10 p1=00 p2=00 p3=09','missing',1),
                    text.replace('action=refresh_confirm','action=missing'),
                    text.replace('810309010002028281030100','810309010002028281030120'),
                    text+'\nSIM completion ins=f2 sw=9100'):
            with self.subTest(text=bad),self.assertRaises(ValueError):
                check(bad,mode='refresh')

    @patch('tools.nsb7_toolkit_check.check_authentication')
    def test_polling_protocol_and_bounded_ordinary_status_cadence(self,authentication):
        text=interactive_fixture(menu=True).replace('SIM status ins=c2 sw=9000',
            'menu selection item=1 accepted\nproactive POLL INTERVAL ready\nSIM status ins=c2 sw=910f')
        exit_event='nsb7_toolkit_interactive: action=menu_exit\nnsb7_key_decode: key=0f observer_writes=0'
        rows=['header cla=a0 ins=12 p1=00 p2=00 p3=0f',
              'header cla=a0 ins=14 p1=00 p2=00 p3=10',
              'terminal-response data=81030a03000202828103010004020105',
              'proactive POLLING OFF ready','SIM status ins=14 sw=910b',
              'header cla=a0 ins=12 p1=00 p2=00 p3=0b',
              'header cla=a0 ins=14 p1=00 p2=00 p3=0c',
              'terminal-response data=81030b040002028281030100','SIM status ins=14 sw=9000']
        rows += [f'header cla=a0 ins=f2 p1=00 p2=00 p3=16 t={t}.0\nSIM completion ins=f2 sw=9000'
                 for t in (93,124,155,186)]
        rows += ['ccont_rtc: event=second time=13:50:53 day=0 t=188.0',exit_event]
        text=text.replace(exit_event,'\n'.join(rows))
        check(text,mode='polling')
        for bad in (text.replace('t=124.0','t=98.0'),text.replace('t=188.0','t=100.0'),
                    text.replace('SIM completion ins=f2 sw=9000','SIM completion ins=f2 sw=9404',1),
                    text.replace('03010004020105','03010004020106'),
                    text+'\nGSM outgoing request id=1 digits=5551234'):
            with self.subTest(text=bad),self.assertRaises(ValueError):
                check(bad,mode='polling')

    def test_unknown_mode_rejected(self):
        with self.assertRaisesRegex(ValueError,'unknown Toolkit'):
            check('',mode='unknown')

    @patch('tools.nsb7_toolkit_check.check_authentication')
    def test_select_item_requires_physical_second_item(self,authentication):
        text=interactive_fixture(menu=True).replace('SIM status ins=c2 sw=9000',
            'menu selection item=1 accepted\nproactive SELECT ITEM ready\nSIM status ins=c2 sw=9122')
        exit_event='nsb7_toolkit_interactive: action=menu_exit\nnsb7_key_decode: key=0f observer_writes=0'
        header='header cla=a0 ins=14 p1=00 p2=00 p3=0f'
        tail='\n'.join(('header cla=a0 ins=12 p1=00 p2=00 p3=22',
            'nsb7_toolkit_interactive: action=item_next','nsb7_key_decode: key=18 observer_writes=0',
            'nsb7_toolkit_interactive: action=item_accept','nsb7_key_decode: key=19 observer_writes=0',
            header,'terminal-response data=810307240002028281030100100102',
            'SIM status ins=14 sw=9000',exit_event))
        text=text.replace(exit_event,tail)
        check(text,mode='select_item')
        early=text.replace(header+'\n','').replace('nsb7_toolkit_interactive: action=item_accept',
            header+'\nnsb7_toolkit_interactive: action=item_accept')
        for bad in (early,text.replace('030100100102','030100100101'),
                    text.replace('key=18','key=17'),
                    text+'\nGSM outgoing request id=1 digits=5551234'):
            with self.subTest(text=bad),self.assertRaises(ValueError):
                check(bad,mode='select_item')

    @patch('tools.nsb7_toolkit_check.check_authentication')
    def test_call_consent_network_and_physical_release(self,authentication):
        text=call_fixture()
        check(text,mode='call')
        setup=next(line for line in text.splitlines() if 'message=05 length=15' in line)
        early=text.replace(setup+'\n','').replace('nsb7_toolkit_interactive: action=call_accept',
            setup+'\nnsb7_toolkit_interactive: action=call_accept')
        for bad in (early,text.replace('action=call_accept','action=missing'),
                    text.replace('810306100002028281030100','810306100002028281030120'),
                    text.replace('message=25 length=5','message=24 length=5'),
                    text+'\nGSM outgoing request id=2 digits=5551234'):
            with self.subTest(text=bad),self.assertRaises(ValueError):
                check(bad,mode='call')

    @patch('tools.nsb7_toolkit_check.check_authentication')
    def test_declined_call_must_not_start_network(self,authentication):
        text=call_fixture(decline=True)
        check(text,mode='decline')
        for bad in (text.replace('030122','030100'),text.replace('key=1a','key=19'),
                    text+'\nGSM outgoing request id=1 digits=5551234'):
            with self.subTest(text=bad),self.assertRaises(ValueError):
                check(bad,mode='decline')

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
        check(text,mode='sms')
        exit_event='nsb7_toolkit_interactive: action=menu_exit\nnsb7_key_decode: key=0f observer_writes=0'
        early=text.replace(exit_event,'').replace('GSM service downlink kind=18',
                    exit_event+'\nGSM service downlink kind=18')
        for bad in (text.replace('downlink kind=18','downlink kind=19'),
                    text.replace('810305130002028281030100','810305130002028281030120'),
                    text.replace('fid=6f43 offset=0 length=2','fid=6f43 offset=0 length=1'),
                    text+'\ngsm_sms_submit:',early):
            with self.subTest(text=bad),self.assertRaises(ValueError):
                check(bad,mode='sms')

    @patch('tools.nsb7_toolkit_check.check_authentication')
    def test_menu_envelope_and_product_navigation(self,authentication):
        text=interactive_fixture(menu=True)
        check(text,mode='menu')
        for bad in (text.replace('d30702020181100101','d30702020181100102'),
                    text.replace('action=menu_skip_netmonitor','action=missing'),
                    text.replace('key=17','key=18'),
                    text+'\nheader cla=a0 ins=c2 p1=00 p2=00 p3=09'):
            with self.subTest(text=bad),self.assertRaises(ValueError):
                check(bad,mode='menu')

    @patch('tools.nsb7_toolkit_check.check_authentication')
    def test_interactive_product_specific_confirmation(self,authentication):
        text=interactive_fixture()
        check(text,mode='interactive')
        for bad in (text.replace('action=inkey_confirm','action=missing'),
                    text.replace('key=02','key=03'),
                    text.replace('nsb7_key_decode: key=19 observer_writes=0','',1)+
                    '\nnsb7_key_decode: key=19 observer_writes=0',
                    text.replace('0d03043432','0d03043433')):
            with self.subTest(text=bad),self.assertRaises(ValueError):
                check(bad,mode='interactive')

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

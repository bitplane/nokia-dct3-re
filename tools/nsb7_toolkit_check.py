"""Authenticated NSB-7 Toolkit display/input and physical successful responses."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.dct3_toolkit_check import verify_display_text,verify_interactive,verify_menu
from tools.nsb7_pin_services_check import check_authentication,check_source,SMSS_OFFSET
from tools.sim_toolkit_sms_trace_check import verify as verify_sms
from tools.sim_toolkit_call_trace_check import verify as verify_call,verify_decline,DISCONNECT
from tools.sim_toolkit_select_item_trace_check import verify as verify_select_item
from tools.sim_toolkit_polling_trace_check import verify_commands as verify_polling_commands
from tools.sim_toolkit_refresh_trace_check import verify as verify_refresh
from tools.sim_toolkit_trace_check import require_in_order
from tools.sim_toolkit_event_list_trace_check import verify as verify_event_rejection
from tools.sim_toolkit_timer_trace_check import verify as verify_timer_rejection
from tools.nsb7_pin_check import check_frame
from tools.nsb7_radio_check import contract
from tools.nsb7_storage_check import inspect
from tools.radio_call_lifecycle_common import require_ordered

FRAMES={
    'nsb7_toolkit_display.png':'0b61ca941a012e9feffe2d13b834c720031b71295220fc32139decd034172b2c',
    'nsb7_toolkit_after_dismiss.png':'32c41625e02a87a0ec97ec98a250040fc87ad8c5722c78200c54b27f7de2d560',
}
INTERACTIVE_FRAMES={
    'nsb7_toolkit_display.png':FRAMES['nsb7_toolkit_display.png'],
    'nsb7_toolkit_inkey.png':'e94ea9d879033d48822bb76a8f080f95e12456c3b897a1717be10fb4ddbbb77f',
    'nsb7_toolkit_input.png':'2c7ca84fb1f98d4a782fa3b3cc5f8f5fc0eadf68b409bbee9ebd3bdc70214597',
    'nsb7_toolkit_input_filled.png':'3b003249f2e069abd5a91b9acd05b2ffe1b0732a69d6589e60d18c4e70f71df1',
    'nsb7_toolkit_interactive_final.png':'29dcc2200f6f5c170afea80e751d1d3afefd83332964c4bdc0f2a843be5e2567',
}
MENU_FRAMES={
    **INTERACTIVE_FRAMES,
    'nsb7_toolkit_menu.png':'76728306449ad0f2ae8cbdbd1fc5e880d3dcff054c955e8b4a2aabc87db8e7e4',
    'nsb7_toolkit_menu_skip_netmonitor.png':'8bc22a997848abf796726c946c9936d051263ff4f8301f6fbd59275e074b848a',
    'nsb7_toolkit_menu_last.png':'d25fbda213ecf2bf85918ea4daf584036e45ef112c6172a7b48a4d6356acb494',
    'nsb7_toolkit_menu_open.png':'a5a9b402a8ca6111d5b4fd24ab253a6bc98f73f8119d11bc3bca646915e66b8f',
    'nsb7_toolkit_menu_select.png':'a5a9b402a8ca6111d5b4fd24ab253a6bc98f73f8119d11bc3bca646915e66b8f',
    'nsb7_toolkit_menu_exit.png':INTERACTIVE_FRAMES['nsb7_toolkit_interactive_final.png'],
}
SMS_FRAMES={**MENU_FRAMES,
    'nsb7_toolkit_menu_select.png':'1141f7a02395b10c1029cf16f26935cbbbbb7ae853cde9c5ad21a4ab096a723a'}
CALL_FRAMES={**MENU_FRAMES,
    'nsb7_toolkit_menu_select.png':'630c788b0a678db6f5fd04c7dce3c30774a512211a09f37ba2284fbaa0e965a1',
    'nsb7_toolkit_call_accept.png':'9521157fb28adca04d612231bb5d05c161476c1895fbf793b4fdab8e7efdaeb0',
    'nsb7_toolkit_call_active.png':'cfd5053b6d0589d7cc7cce673ffb8913bc32b18e7ae49f51e0f72195eddcf8ed'}
DECLINE_FRAMES={**MENU_FRAMES,
    'nsb7_toolkit_menu_select.png':CALL_FRAMES['nsb7_toolkit_menu_select.png'],
    'nsb7_toolkit_call_decline.png':MENU_FRAMES['nsb7_toolkit_menu_open.png'],
    'nsb7_toolkit_call_active.png':MENU_FRAMES['nsb7_toolkit_menu_open.png']}
SELECT_ITEM_FRAMES={**MENU_FRAMES,
    'nsb7_toolkit_menu_select.png':'2fd7bf03bc43f6e81353678de00f40c96ef9a7dad56cf47672f8793552fe394d',
    'nsb7_toolkit_item_next.png':'1a9cdbfa614372e7d78e887b5cc3be697a8e065e46b8ab0303b0caf13cd0f0ee',
    'nsb7_toolkit_item_accept.png':MENU_FRAMES['nsb7_toolkit_menu_open.png']}
POLLING_FRAMES={**MENU_FRAMES,
    'nsb7_toolkit_menu_exit.png':'9da919d7d031f0e7d2f8184dab4a20d2f91d747ac025de9908bf24112faa162a'}
REFRESH_FRAMES={**MENU_FRAMES,
    'nsb7_toolkit_menu_select.png':'4ae624eb4a175cd58c4fb8b0a6b0c74e612da2a9b2958a85cd1a75957aa2bbf9',
    'nsb7_toolkit_menu_exit.png':POLLING_FRAMES['nsb7_toolkit_menu_exit.png'],
    'nsb7_toolkit_refresh_display.png':FRAMES['nsb7_toolkit_display.png'],
    'nsb7_toolkit_refresh_dismiss.png':'0c320ff7bbdd1dc904aea3b70ac3320f93a5787130732a72e1ca05aa6575835e',
    'nsb7_toolkit_refresh_inkey5.png':'fec0cd87203f42163c39c4938c6078971d3479d8aa0ef70e42853407c3881598',
    'nsb7_toolkit_refresh_inkey_confirm.png':INTERACTIVE_FRAMES['nsb7_toolkit_input.png'],
    'nsb7_toolkit_refresh_input4.png':'590b4acd0270a990148edba76f785e8f60394bff63edeb6d8a6eeb25752ed186',
    'nsb7_toolkit_refresh_input2.png':'169b9f1bb144e9cd4e061a74980b1479956c9431c17482149b7701300b6e83b0',
    'nsb7_toolkit_refresh_confirm.png':POLLING_FRAMES['nsb7_toolkit_menu_exit.png']}


MODE_FRAMES={'display':FRAMES,'interactive':INTERACTIVE_FRAMES,'menu':MENU_FRAMES,
             'sms':SMS_FRAMES,'call':CALL_FRAMES,'decline':DECLINE_FRAMES,
             'select_item':SELECT_ITEM_FRAMES,'polling':POLLING_FRAMES,'refresh':REFRESH_FRAMES,
             'events':MENU_FRAMES,'timer':MENU_FRAMES}


def check(text,mode='display'):
    if mode not in MODE_FRAMES:
        raise ValueError('unknown Toolkit acceptance mode')
    sms=mode=='sms'
    select_item=mode=='select_item'
    polling=mode=='polling'
    refresh=mode=='refresh'
    rejected=mode in ('events','timer')
    decline=mode=='decline'
    call=mode in ('call','decline')
    menu=mode not in ('display','interactive')
    interactive=mode!='display'
    marker='nsb7_toolkit_interactive' if interactive else 'nsb7_toolkit_physical'
    check_authentication(text.split(marker+':',1)[0])
    verifier=verify_menu if menu else verify_interactive if interactive else verify_display_text
    selection_status={'sms':'9124','call':'911c','decline':'911c',
                      'select_item':'9122','polling':'910f','refresh':'910b',
                      'events':'910f','timer':'9113'}.get(mode)
    if selection_status:
        verifier(text,'nsb7',selection_status=selection_status)
    else:
        verifier(text,'nsb7')
    if sms:
        verify_sms(text,cp=0x39)
        if text.count('update-binary fid=6f43 offset=0 length=2 ')!=1:
            raise ValueError('expected one Toolkit SMS reference update')
        require_ordered(text.replace('[:sim_card] ',''),[
            ('SMS completion',r'terminal-response data=810305130002028281030100'),
            ('radio release',r'LAPDm service Channel Release acknowledged nr=2'),
            ('physical exit',r'nsb7_toolkit_interactive: action=menu_exit'),
        ],'NSB-7 Toolkit SMS completion before exit')
    if call:
        if decline:
            verify_decline(text,'nsb7')
        else:
            verify_call(text)
            if text.count('GSM outgoing request ')!=1:
                raise ValueError('expected exactly one Toolkit call request')
            require_ordered(text.replace('[:sim_card] ',''),[
                ('physical consent',r'nsb7_toolkit_interactive: action=call_accept'),
                ('decoded consent',r'nsb7_key_decode: key=19 observer_writes=0'),
                ('SETUP',r'GSM service uplink sapi=0 pd=03 message=05 '),
                ('call request',r'GSM outgoing request id=1 digits=5551234'),
                ('Connect acknowledgment',r'GSM service uplink sapi=0 pd=03 message=0f length=2'),
                ('successful response',r'terminal-response data=810306100002028281030100'),
                ('physical End',r'nsb7_toolkit_interactive: action=menu_exit'),
                ('decoded End',r'nsb7_key_decode: key=0f observer_writes=0'),
                ('Disconnect',DISCONNECT),
            ],'NSB-7 Toolkit call consent and release')
    if select_item:
        verify_select_item(text)
        if 'GSM outgoing request ' in text or 'gsm_sms_submit:' in text:
            raise ValueError('item selection unexpectedly started network service')
        require_ordered(text.replace('[:sim_card] ',''),[
            ('physical next item',r'nsb7_toolkit_interactive: action=item_next'),
            ('decoded Down',r'nsb7_key_decode: key=18 observer_writes=0'),
            ('physical OK',r'nsb7_toolkit_interactive: action=item_accept'),
            ('decoded OK',r'nsb7_key_decode: key=19 observer_writes=0'),
            ('response header',r'header cla=a0 ins=14 p1=00 p2=00 p3=0f'),
            ('selected item2',r'terminal-response data=810307240002028281030100100102'),
            ('physical exit',r'nsb7_toolkit_interactive: action=menu_exit'),
        ],'NSB-7 Toolkit item selection')
    if polling:
        verify_polling_commands(text)
        compact=text.replace('[:sim_card] ','')
        off=compact.index('terminal-response data=81030b040002028281030100')
        after=compact[off:]
        headers=list(re.finditer(r'header cla=a0 ins=f2 [^\n]*t=([0-9.]+)',after))
        polls=[float(row[1]) for row in headers]
        ticks=[float(t) for t in re.findall(r'ccont_rtc: event=second [^\n]*t=([0-9.]+)',compact[off:])]
        # Measured ordinary STATUS traffic, not proof of its firmware owner
        # or the physical timer rate. POLLING OFF leaves presence detection.
        if len(polls)<4 or any(not 30<=b-a<=32 for a,b in zip(polls,polls[1:])):
            raise ValueError('post-off ordinary STATUS cadence differs')
        if not ticks or max(ticks)<188 or polls[-1]<185:
            raise ValueError('post-off observation was truncated')
        for index,row in enumerate(headers):
            end=headers[index+1].start() if index+1<len(headers) else len(after)
            if 'SIM completion ins=f2 sw=9000' not in after[row.end():end]:
                raise ValueError('post-off STATUS did not complete successfully')
        if 'GSM outgoing request ' in text or 'gsm_sms_submit:' in text:
            raise ValueError('polling unexpectedly started network service')
    if refresh:
        verify_refresh(text,profile_length=9)
        compact=text.replace('[:sim_card] ','')
        if 'header cla=a0 ins=12 p1=00 p2=00 p3=00' in compact or 'SIM completion ins=f2 sw=9100' in compact:
            raise ValueError('REFRESH resumed an empty proactive command')
        if 'GSM outgoing request ' in text or 'gsm_sms_submit:' in text:
            raise ValueError('REFRESH unexpectedly started network service')
        require_in_order(compact,(
            'terminal-response data=810309010002028281030100',
            'proactive DISPLAY TEXT ready','header cla=a0 ins=12 p1=00 p2=00 p3=16',
            'nsb7_toolkit_interactive: action=refresh_dismiss',
            'terminal-response data=810301218002028281030100',
            'proactive GET INKEY ready','header cla=a0 ins=12 p1=00 p2=00 p3=15',
            'nsb7_toolkit_interactive: action=refresh_inkey5',
            'nsb7_toolkit_interactive: action=refresh_inkey_confirm',
            'terminal-response data=8103022200020282810301000d020435',
            'proactive GET INPUT ready','header cla=a0 ins=12 p1=00 p2=00 p3=1a',
            'nsb7_toolkit_interactive: action=refresh_input4',
            'nsb7_toolkit_interactive: action=refresh_input2',
            'nsb7_toolkit_interactive: action=refresh_confirm',
            'terminal-response data=8103032300020282810301000d03043432',
            'proactive SET UP MENU ready','header cla=a0 ins=12 p1=00 p2=00 p3=28',
            'terminal-response data=810304250002028281030100','SIM status ins=14 sw=9000',
            'nsb7_toolkit_interactive: action=menu_exit'))
    if rejected:
        (verify_event_rejection if mode=='events' else verify_timer_rejection)(text)
        compact=text.replace('[:sim_card] ','')
        if 'envelope data=d6' in compact or 'envelope data=d7' in compact:
            raise ValueError('rejected command produced a download envelope')
        if 'GSM outgoing request ' in text or 'gsm_sms_submit:' in text:
            raise ValueError('rejected command started network service')
        ticks=[float(t) for t in re.findall(r'ccont_rtc: event=second [^\n]*t=([0-9.]+)',text)]
        if not ticks or max(ticks)<108:
            raise ValueError('capability-rejection observation was truncated')
    for instruction in ('10','12','14'):
        count=9 if refresh else 6 if polling else 5 if sms or call or select_item or rejected else 4 if menu else 3
        expected=count if interactive and instruction!='10' else 1
        if refresh and instruction=='10': expected=2
        if text.count(f'header cla=a0 ins={instruction} ')!=expected:
            raise ValueError('Toolkit profile/FETCH/terminal-response count differs')
    if text.count('header cla=a0 ins=c2 ')!=(1 if menu else 0):
        raise ValueError('Toolkit envelope count differs')
    if interactive:
        actions=list(re.finditer(r'nsb7_toolkit_interactive: action=(\w+)',text))
        expected=[('dismiss','19'),('inkey_5','05'),('inkey_confirm','19'),
                  ('input_4','04'),('input_2','02'),('confirm','19')]
        if menu:
            expected += [('menu','19'),('menu_skip_netmonitor','17'),('menu_last','17'),
                         ('menu_open','19'),('menu_select','19'),('menu_exit','0f')]
        if call:
            expected.insert(len(expected)-1,('call_decline','1a') if decline else ('call_accept','19'))
        if select_item:
            expected[-1:-1]=[('item_next','18'),('item_accept','19')]
        if refresh:
            expected[-1:-1]=[('refresh_dismiss','19'),('refresh_inkey5','05'),
                ('refresh_inkey_confirm','19'),('refresh_input4','04'),
                ('refresh_input2','02'),('refresh_confirm','19')]
        if [action[1] for action in actions]!=[name for name,_ in expected]:
            raise ValueError('physical Toolkit input sequence differs')
        for index,(_,code) in enumerate(expected):
            end=actions[index+1].start() if index+1<len(actions) else len(text)
            for instruction in ('14','c2'):
                response=text.find(f'header cla=a0 ins={instruction} ',actions[index].end())
                if response>=0:
                    end=min(end,response)
            if f'nsb7_key_decode: key={code} observer_writes=0' not in text[actions[index].end():end]:
                raise ValueError('physical Toolkit contact did not decode')
        return
    require_ordered(text,[
        ('physical OK',r'nsb7_toolkit_physical: action=dismiss'),
        ('decoded softkey',r'nsb7_key_decode: key=19 observer_writes=0'),
        ('terminal response',r'header cla=a0 ins=14 p1=00 p2=00 p3=0c'),
    ],'NSB-7 physical Toolkit dismissal')


def check_card(initial,final,sms=False):
    check_source(initial)
    expected=bytearray(initial)
    if sms:
        expected[SMSS_OFFSET]=0
    if final!=bytes(expected):
        raise ValueError('Toolkit persistent SIM storage differs')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image',type=Path)
    parser.add_argument('run',type=Path)
    modes=parser.add_mutually_exclusive_group()
    for flag,mode in (('--interactive','interactive'),('--menu','menu'),('--sms','sms'),
                      ('--call','call'),('--call-decline','decline'),('--select-item','select_item'),
                      ('--polling','polling'),('--refresh','refresh'),
                      ('--events-rejected','events'),('--timer-rejected','timer')):
        modes.add_argument(flag,dest='mode',action='store_const',const=mode)
    parser.set_defaults(mode='display')
    args=parser.parse_args()
    try:
        contract(args.image.read_bytes())
        check((args.run/'error.log').read_text(),mode=args.mode)
        initial=(args.run/'initial_sim_card.bin').read_bytes()
        storage=args.run/'nvram/nsb7hle'
        check_card(initial,(storage/'sim_card').read_bytes(),sms=args.mode=='sms')
        inspect((storage/'flash').read_bytes())
        for name,digest in MODE_FRAMES[args.mode].items():
            check_frame(args.run/'snap'/name,digest)
    except (OSError,ValueError) as error:
        parser.exit(1,f'NSB-7 Toolkit: FAIL: {error}\n')
    outcome='explicit result31 rejection' if args.mode in ('events','timer') else 'physical successful responses'
    print(f'PASS: authenticated registered Toolkit {args.mode}, {outcome}, exact card/UI')


if __name__=='__main__':
    main()

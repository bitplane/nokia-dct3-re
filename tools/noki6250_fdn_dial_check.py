"""Own NHM-3 FDN permission controls; laboratory signaling, not speech."""
import re
from tools.noki6250_fdn_record_check import expected_card as saved_card
from tools.noki6210_puk_check import contacts,check_rgb_frame
from tools.noki6250_call_check import verify_outgoing
from tools.sim_security_trace_check import statuses
from tools import radio_outgoing_call_trace_check as call
from tools.radio_call_lifecycle_common import require_ordered


def actions(mode):
    if mode not in ('listed','denied','off'):
        raise ValueError('FDN dial mode must be listed/denied/off')
    digits='12345' if mode=='listed' else '12346'
    return ([(8+i,'Keypad '+digit) for i,digit in enumerate('1234')]+[(12,'Left Softkey / Menu')]+
            [(20+i,'Keypad '+digit) for i,digit in enumerate(digits)]+
            [(26,'Send'),(38,'End'),(40,'Left Softkey / Menu')])


def expected_card(mode):
    actions(mode)
    card=bytearray(saved_card(True))
    if mode=='off':card[3575]=1
    return bytes(card)


def check(text,initial,final,mode):
    items=actions(mode)
    if initial!=expected_card(mode) or final!=initial:
        raise ValueError('FDN dial control must preserve the complete saved card')
    contacts(text,'6250_fdn_dial',items)
    if statuses(text,0x20)!=[(0x9000,3,10)] or statuses(text,0x20,2)!=[(0x9000,3,10)] or \
            re.findall(r'header cla=a0 ins=20 p1=00 p2=(\w\w) p3=08',text)!=['01']:
        raise ValueError('FDN dialing requires exactly one startup PIN1 VERIFY')
    if re.search(r'header cla=a0 ins=(?:04|24|26|28|2c|dc) ',text):
        raise ValueError('FDN dialing changed validity, credentials or records')
    targets=re.findall(r'header cla=a0 ins=44 p1=00 p2=00 p3=00 selected=(\w{4})',text)
    wanted=[] if mode=='off' else ['6f7e','6f07']
    if targets!=wanted or statuses(text,0x44)!=[(0x9000,3,10)]*len(wanted):
        raise ValueError('FDN dial startup identity rehabilitation differs')
    if re.findall(r'header cla=a0 ins=b2 p1=(\w\w) p2=04 p3=20 selected=6f3b',text)!=[f'{index:02x}' for index in range(1,11)]:
        raise ValueError('FDN dial control must load all ten card records')
    require_ordered(text,[
        ('PIN1',r'header cla=a0 ins=20 p1=00 p2=01 p3=08'),
        ('registered',r'LAPDm Location Updating Accept acknowledged'),
        ('Send',r'6250_fdn_dial: key=Send state=1 t=26\.'),
        ('End',r'6250_fdn_dial: key=End state=1 t=38\.'),
        ('Menu',r'6250_fdn_dial: key=Left Softkey / Menu state=1 t=40\.'),
        ('complete',r'6250_fdn_dial: complete=1 firmware_state_forcing=0'),
    ],'6250 FDN dial')
    if mode=='denied':
        if any(pattern.search(text) for pattern in (call.CM_SERVICE_REQUEST,call.SETUP,
                                                   call.TRAFFIC_ASSIGNMENT,call.CONNECT)):
            raise ValueError('FDN-denied number reached call establishment')
        if re.search(r'GSM service (?:establish|uplink) sapi=\d+ pd=03 ',text):
            raise ValueError('FDN-denied number emitted call-control signaling')
    else:
        verify_outgoing(text,'12345' if mode=='listed' else '12346',configured_carrier=True,
                        physical_send=r'6250_fdn_dial: key=Send state=1 t=26\.',
                        physical_end=r'6250_fdn_dial: key=End state=1 t=38\.')


def check_frames(directory,mode):
    actions(mode)
    frames={
        'listed':('69ea8503846ec9d25c8dc376b474a598de6c05feaa58baf4a7280b004366c891',
                  '044184201ffb59e41b409faa5350bdfae1c8ebb5f4386c2fac50b459939406e1'),
        'denied':('c07981d9cdf4d878a8e4c2df6fe666ae08cece97e56156465c465bb2cc6a778e',
                  '32f41c808f7e3bb08a8f46cca4d0c7fc43a733661a9ff19735016e9fdd773de4'),
        'off':('ccb86370f4ecb8b1f59fe4f7f7bd9102a55689146c4f7250093fc0a1dbfa169a',
               '044184201ffb59e41b409faa5350bdfae1c8ebb5f4386c2fac50b459939406e1'),
    }
    for name,digest in zip(('early','result'),frames[mode]):
        check_rgb_frame(directory/f'6250_fdn_dial_{name}.png',digest)
    check_rgb_frame(directory/'6250_fdn_dial_menu.png','2c2a6aa19024c3f6bf1d3797a8f7c38edb950243ec17fe2241793a5ef029ac24')

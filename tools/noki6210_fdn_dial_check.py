"""6210 physical FDN listed/unlisted/Off controls; HLE signaling, not speech."""
import re
from tools.noki6210_fdn_record_check import expected_card
from tools.noki6210_puk_check import contacts,check_rgb_frame
from tools.sim_security_trace_check import statuses
from tools.noki6210_outgoing_call_check import verify as check_call
from tools import radio_outgoing_call_trace_check as call
from tools.radio_call_lifecycle_common import require_ordered


def input_contacts(digits):
    return ([(8+i,'Keypad '+d) for i,d in enumerate('1234')]+[(12,'Left Softkey / Menu')]+
            [(20+i,'Keypad '+d) for i,d in enumerate(digits)]+
            [(26,'Send'),(38,'End'),(40,'Left Softkey / Menu')])


def check(text,initial,final,mode):
    if mode not in ('listed','denied','off'):
        raise ValueError('unknown FDN dial control')
    digits='12345' if mode=='listed' else '12346'
    expected=bytearray(expected_card(True))
    if mode=='off':
        expected[3575]=1
    if initial!=bytes(expected) or final!=initial:
        raise ValueError('FDN dial control changed card validity, credentials or records')
    contacts(text,'6210_fdn_dial',input_contacts(digits))
    rows=list(re.finditer(r'6210_fdn_dial: key=(.*?) state=([01]) t=[\d.]+',text))
    if len(rows)!=2*len(input_contacts(digits)):
        raise ValueError('FDN dial contact edge count differs')
    for index in range(0,len(rows),2):
        if rows[index][2]!='1' or rows[index+1][2]!='0' or rows[index][1]!=rows[index+1][1]:
            raise ValueError('FDN dial contact release differs')
        name=rows[index][1]
        code={'Send':0x0e,'End':0x0f,'Left Softkey / Menu':0x19}.get(name)
        if code is None:
            code=int(name[-1])
        if f'6210_keypad_decoded: key={code:02x}' not in text[rows[index].end():rows[index+1].start()]:
            raise ValueError('FDN dial contact did not decode during its physical press')
    if statuses(text,0x20)!=[(0x9000,3,10)] or any(statuses(text,ins) for ins in (4,0x24,0x26,0x28,0x2c,0xdc)):
        raise ValueError('FDN dialing changed authentication or card records')
    targets=re.findall(r'header cla=a0 ins=44 p1=00 p2=00 p3=00 selected=(\w{4})',text)
    expected_targets=[] if mode=='off' else ['6f7e','6f07']
    if targets!=expected_targets or statuses(text,0x44)!=[(0x9000,3,10)]*len(expected_targets):
        raise ValueError('FDN dial startup identity rehabilitation differs')
    records=re.findall(r'header cla=a0 ins=b2 p1=(\w\w) p2=04 p3=20 selected=6f3b',text)
    if records!=[f'{i:02x}' for i in range(1,11)]:
        raise ValueError('FDN dial control did not load its ten card records')
    require_ordered(text,[
        ('PIN1',r'SIM status ins=20 sw=9000'),
        ('registration',r'LAPDm Location Updating Accept acknowledged nr=1'),
        ('Send',r'key=Send state=1 t=26\.'),
        ('End',r'key=End state=1 t=38\.'),
        ('Menu',r'key=Left Softkey / Menu state=1 t=40\.'),
        ('complete',r'6210_fdn_dial: complete=1 firmware_state_forcing=0'),
    ],'6210 FDN dial')
    if mode=='denied':
        if any(pattern.search(text) for pattern in (call.CM_SERVICE_REQUEST,call.SETUP,
                                                   call.TRAFFIC_ASSIGNMENT,call.CONNECT)):
            raise ValueError('FDN-denied number reached call establishment')
        if re.search(r'GSM service (?:establish|uplink) sapi=\d+ pd=03 ',text):
            raise ValueError('FDN-denied number emitted call-control signaling')
    else:
        check_call(text,number=digits,coherent_pin=True)


def check_frames(directory,mode):
    if mode not in ('listed','denied','off'):
        raise ValueError('unknown FDN dial frame control')
    frames={'6210_fdn_dial_menu.png':'5a3c0ca03c73478aa8c645a8a263e5edb04cc833639c893dc638ac18c300ff12',
            '6210_fdn_dial_result.png':('a9d12080d0b9f5f9eaa7c47cafaa5b4b4ade76b0e7d0159f1504fc2b14b84c7d' if mode=='denied'
                                       else '900e665d43692353d2232696df79822ec01c0ee7b8dcf4c3f70815d08dafd851')}
    if mode=='denied':
        frames['6210_fdn_dial_early.png']='c07981d9cdf4d878a8e4c2df6fe666ae08cece97e56156465c465bb2cc6a778e'
    for name,digest in frames.items():
        check_rgb_frame(directory/name,digest)

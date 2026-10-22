"""6210 physical FDN validity lifecycle; no connected-call claim."""
import re
from tools.sim_fdn_profile import extend
from tools.noki6210_puk_check import profile,contacts,check_rgb_frame
from tools.noki6210_pin2_change_check import settings_contacts
from tools.sim_security_trace_check import statuses
from tools.radio_call_lifecycle_common import require_ordered

OFF='8bc5bc2f85cf11a55c3eb0762b00e40f94471b59407caf5fae6b89c993b1bdd0'
ON='72dbb095c41e70afd8487a989d70e1a57830f56068b194245e1a7f22a9a8d49d'
MENU='5a3c0ca03c73478aa8c645a8a263e5edb04cc833639c893dc638ac18c300ff12'


def check_frames(directory,phase,rejected=False):
    input_contacts(phase)
    frames={'6210_fdn_key_40.png':OFF if phase in (0,3) else ON,
            '6210_fdn_then_menu.png':MENU}
    if phase%2==0:
        frames['6210_fdn_key_44.png']='de9dd2e4fb6d913c2a51dc9fd1c2b4d1f88bdd1b2ae4281a031bcbb7a7816612'
        frames['6210_fdn_key_51.png']=('4b5b9b9163e95c74ccf859323d9b07046e44a39ab44c0a1c4fb12a2519c74e4a' if phase==0
                                           else 'f83d70cfadf905d1b0c77848e40e9ef1875d911098128c23c4643b470f1df47b')
    if rejected:
        frames['6210_fdn_key_51.png']='c1371c4cdca6b0f72c9c1dcc6e602de981330f67719714bc5b124e1a3b4fcfcd'
    for name,digest in frames.items():
        check_rgb_frame(directory/name,digest)


def input_contacts(phase,rejected=False):
    if phase not in range(4):
        raise ValueError('FDN phase must be On/cold-On/Off/cold-Off')
    items=[item for item in settings_contacts() if item[0]<=34]
    items += [(36,'Scroll Down'),(37,'Scroll Down'),(40,'Left Softkey / Menu')]
    if phase%2==0:
        items += [(42,'Scroll Up' if phase==0 else 'Scroll Down'),(44,'Left Softkey / Menu')]
        items += [(47+i,'Keypad '+d) for i,d in enumerate('5675' if rejected else '5678')]
        items += [(51,'Left Softkey / Menu'),(58,'End'),(60,'Left Softkey / Menu')]
    else:
        items += [(50,'End'),(52,'Left Softkey / Menu')]
    return items


def check(text,card,phase,rejected=False,initial_pin2_attempts=3,record=None):
    if rejected and phase!=0:
        raise ValueError('only activation rejection is qualified')
    if initial_pin2_attempts not in (2,3):
        raise ValueError('FDN requires an evidenced PIN2 retry predecessor')
    items=input_contacts(phase,rejected)
    contacts(text,'6210_fdn_input',items)
    expected=bytearray(extend(profile(3,True)))
    if record is not None:
        if len(record)!=32:
            raise ValueError('FDN fixture record must have 32 bytes')
        expected[3578:3610]=record
    expected[3575]=0 if phase<2 and not rejected else 1
    if rejected:
        expected[3517]=initial_pin2_attempts-1
    if card!=bytes(expected):
        raise ValueError('FDN changed card bytes beyond registered location and ADN validity')
    result=0x9804 if rejected else 0x9000
    pin1_status=[(0x9000,3,10)]+([(result,3,10)] if phase%2==0 else [])
    pin2_status=[(0x9000,initial_pin2_attempts,10)]+([(result,initial_pin2_attempts-1 if rejected else 3,10)] if phase%2==0 else [])
    if statuses(text,0x20)!=pin1_status or statuses(text,0x20,2)!=pin2_status:
        raise ValueError('FDN PIN1/PIN2 verification sequence differs')
    verify=re.findall(r'header cla=a0 ins=20 p1=00 p2=(\w\w) p3=08',text)
    if verify!=(['01','02'] if phase%2==0 else ['01']):
        raise ValueError('FDN CHV selectors differ')
    if any(statuses(text,ins) for ins in (0x24,0x26,0x28,0x2c)):
        raise ValueError('FDN unexpectedly changed credentials')
    for instruction,targets in ((4,[0x6f3a] if phase==0 and not rejected else []),
                                (0x44,[] if rejected else ([0x6f7e,0x6f07],[0x6f7e,0x6f07],
                                       [0x6f7e,0x6f07,0x6f3a],[])[phase])):
        found=re.findall(rf'header cla=a0 ins={instruction:02x} p1=00 p2=00 p3=00 selected=(\w{{4}})',text)
        if [int(fid,16) for fid in found]!=targets or statuses(text,instruction)!=[(0x9000,3,10)]*len(targets):
            raise ValueError('FDN identity/ADN validity transaction differs')
    records=re.findall(r'header cla=a0 ins=b2 p1=(\w\w) p2=04 p3=20 selected=6f3b',text)
    if records!=[f'{i:02x}' for i in range(1,11)]:
        raise ValueError('FDN did not read the ten card records')
    order=[('PIN1',r'SIM status ins=20 sw=9000'),
           ('registration',r'LAPDm Location Updating Accept acknowledged nr=1'),
           ('FDN menu',r'key=Left Softkey / Menu state=1 t=40\.')]
    if phase%2==0:
        order += [('PIN2 confirm',r'key=Left Softkey / Menu state=1 t=51\.'),
                  ('VERIFY PIN2',r'header cla=a0 ins=20 p1=00 p2=02 p3=08'),
                  ('verified',rf'SIM status ins=20 sw={result:04x}')]
        if not rejected:
            order += [('ADN validity',rf'header cla=a0 ins={4 if phase==0 else 0x44:02x} p1=00 p2=00 p3=00 selected=6f3a')]
    order += [('Menu',rf'key=Left Softkey / Menu state=1 t={60 if phase%2==0 else 52}\.'),
              ('complete',r'6210_fdn_input: complete=1 firmware_state_forcing=0')]
    require_ordered(text,order,'6210 FDN lifecycle')

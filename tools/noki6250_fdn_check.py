"""Own NHM-3 physical FDN validity lifecycle, not dial permission or calls."""
import re
from tools.make_sim_card_profile import make_profile
from tools.sim_fdn_profile import extend
from tools.noki6250_pin_retry_check import registered_card
from tools.noki6250_pin2_change_check import actions as settings_actions,FRAMES
from tools.noki6210_puk_check import contacts,check_rgb_frame
from tools.sim_security_trace_check import statuses
from tools.radio_call_lifecycle_common import require_ordered


def actions(phase,*,rejected=False):
    if type(phase) is not int or phase not in range(4):
        raise ValueError('FDN phase must be On/cold-On/Off/cold-Off')
    if rejected and phase!=0:
        raise ValueError('only rejected FDN activation is qualified')
    items=[(second,key) for second,key in settings_actions() if second<=34]
    items += [(36,'Scroll Down'),(37,'Scroll Down'),(40,'Left Softkey / Menu')]
    if phase%2==0:
        items += [(42,'Scroll Up' if phase==0 else 'Scroll Down'),(44,'Left Softkey / Menu')]
        items += [(47+i,'Keypad '+digit) for i,digit in enumerate('5675' if rejected else '5678')]
        items += [(51,'Left Softkey / Menu'),(58,'End'),(60,'Left Softkey / Menu')]
    else:
        items += [(50,'End'),(52,'Left Softkey / Menu')]
    return items


def initial_card(phase,*,pin2_attempts=3,preserved=False,record=None):
    actions(phase)
    if type(pin2_attempts) is not int or pin2_attempts not in (2,3):
        raise ValueError('FDN retry predecessor must have two or three PIN2 attempts')
    card=bytearray(extend(make_profile(True)+bytes.fromhex('00f110')+b'\xff'*43) if phase==0 and not preserved else extend(registered_card()))
    card[3575]=int(phase in (0,3))
    card[3517]=pin2_attempts
    if record is not None:
        if len(record)!=32:raise ValueError('FDN record must have32 bytes')
        card[3578:3610]=record
    return bytes(card)


def check(text,initial,final,phase,*,rejected=False,pin2_attempts=3,preserved=False,record=None):
    items=actions(phase,rejected=rejected)
    expected=bytearray(extend(registered_card()))
    expected[3575]=int(phase>=2 or rejected)
    if phase%2:expected[3517]=pin2_attempts
    if rejected:expected[3517]=pin2_attempts-1
    expected_input=initial_card(phase,pin2_attempts=pin2_attempts,preserved=preserved,record=record)
    if record is not None:expected[3578:3610]=record
    if initial!=expected_input or final!=bytes(expected):
        raise ValueError('FDN lifecycle changed unrelated storage or lost inherited validity')
    result=0x9804 if rejected else 0x9000
    verifies=[(0x9000,3,10)]+([(result,3,10)] if phase%2==0 else [])
    pin2=[(0x9000,pin2_attempts,10)]+([(result,pin2_attempts-1 if rejected else 3,10)] if phase%2==0 else [])
    if statuses(text,0x20)!=verifies or statuses(text,0x20,2)!=pin2:
        raise ValueError('FDN requires startup PIN1 and PIN2 only for toggles')
    if re.findall(r'header cla=a0 ins=20 p1=00 p2=(\w\w) p3=08',text)!=(['01','02'] if phase%2==0 else ['01']):
        raise ValueError('FDN CHV selectors differ')
    if re.search(r'header cla=a0 ins=(?:24|26|28|2c|dc) ',text):
        raise ValueError('FDN lifecycle changed credentials or records')
    for instruction,targets in ((4,[0x6f3a] if phase==0 and not rejected else []),
                                (0x44,[] if rejected else ([0x6f7e,0x6f07],[0x6f7e,0x6f07],
                                       [0x6f7e,0x6f07,0x6f3a],[])[phase])):
        found=re.findall(rf'header cla=a0 ins={instruction:02x} p1=00 p2=00 p3=00 selected=(\w{{4}})',text)
        if [int(value,16) for value in found]!=targets or statuses(text,instruction)!=[(0x9000,3,10)]*len(targets):
            raise ValueError('FDN identity/ADN validity commands differ')
    if re.findall(r'header cla=a0 ins=b2 p1=(\w\w) p2=04 p3=20 selected=6f3b',text)!=[f'{index:02x}' for index in range(1,11)]:
        raise ValueError('FDN lifecycle must read all ten card records')
    contacts(text,'6250_fdn',items)
    order=[('PIN1',r'header cla=a0 ins=20 p1=00 p2=01 p3=08'),
           ('registered',r'LAPDm Location Updating Accept acknowledged'),
           ('FDN menu',r'6250_fdn: key=Left Softkey / Menu state=1 t=40\.')]
    if phase%2==0:
        order += [('PIN2 confirmation',r'6250_fdn: key=Left Softkey / Menu state=1 t=51\.'),
                  ('PIN2',r'header cla=a0 ins=20 p1=00 p2=02 p3=08'),
                  ('verified',rf'SIM status ins=20 sw={result:04x}')]
        if not rejected:
            order += [('ADN validity',rf'header cla=a0 ins={4 if phase==0 else 0x44:02x} p1=00 p2=00 p3=00 selected=6f3a')]
    order += [('Menu',rf'6250_fdn: key=Left Softkey / Menu state=1 t={60 if phase%2==0 else 52}\.'),
              ('complete',r'6250_fdn: complete=1 firmware_state_forcing=0')]
    require_ordered(text,order,'6250 FDN lifecycle')


def check_frames(directory,phase,*,rejected=False):
    actions(phase,rejected=rejected)
    for second in (26,31,34):
        check_rgb_frame(directory/f'6250_fdn_{second}.png',FRAMES[second])
    off='8bc5bc2f85cf11a55c3eb0762b00e40f94471b59407caf5fae6b89c993b1bdd0'
    on='72dbb095c41e70afd8487a989d70e1a57830f56068b194245e1a7f22a9a8d49d'
    check_rgb_frame(directory/'6250_fdn_40.png',off if phase in (0,3) else on)
    if phase%2==0:
        for second,digest in ((42,on if phase==0 else off),
                (44,'de9dd2e4fb6d913c2a51dc9fd1c2b4d1f88bdd1b2ae4281a031bcbb7a7816612'),
                (51,'c1371c4cdca6b0f72c9c1dcc6e602de981330f67719714bc5b124e1a3b4fcfcd' if rejected else
                    '4b5b9b9163e95c74ccf859323d9b07046e44a39ab44c0a1c4fb12a2519c74e4a' if phase==0 else
                    'f83d70cfadf905d1b0c77848e40e9ef1875d911098128c23c4643b470f1df47b'),
                (58,FRAMES[76]),(60,FRAMES[78])):
            check_rgb_frame(directory/f'6250_fdn_{second}.png',digest)
    else:
        check_rgb_frame(directory/'6250_fdn_50.png',FRAMES[76])
        check_rgb_frame(directory/'6250_fdn_52.png',FRAMES[78])

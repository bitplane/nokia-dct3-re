"""Own NHM-3 PIN2-protected FDN Save and independent cold Details."""
import re
from tools.noki6250_fdn_check import actions as fdn_actions,initial_card
from tools.nsk3_phonebook_check import expected_record
from tools.noki6210_puk_check import contacts,check_rgb_frame
from tools.noki6250_pin2_change_check import FRAMES
from tools.sim_security_trace_check import statuses
from tools.radio_call_lifecycle_common import require_ordered


def actions(mode):
    if mode not in ('write','read'):
        raise ValueError('FDN record mode must be write/read')
    items=[item for item in fdn_actions(1) if item[0]<=40]
    items += [(42,'Scroll Down'),(43,'Scroll Down'),(46,'Left Softkey / Menu')]
    if mode=='write':
        items += [(48,'Scroll Down'),(50,'Left Softkey / Menu')]
        items += [(53+i,'Keypad '+digit) for i,digit in enumerate('5678')]
        items += [(57,'Left Softkey / Menu')]
        items += [(60+i,'Keypad '+digit) for i,digit in enumerate('12345')]
        items += [(65,'Left Softkey / Menu'),(68,'Keypad 2'),(71,'Left Softkey / Menu'),
                  (78,'End'),(80,'Left Softkey / Menu')]
    else:
        items += [(48,'Left Softkey / Menu'),(50,'Left Softkey / Menu'),
                  (53,'Left Softkey / Menu'),(60,'End'),(62,'Left Softkey / Menu')]
    return items


def expected_card(saved):
    card=bytearray(initial_card(1))
    if saved:card[3578:3610]=expected_record()
    return bytes(card)


def check(text,initial,final,mode):
    items=actions(mode)
    writing=mode=='write'
    if initial!=expected_card(not writing) or final!=expected_card(True):
        raise ValueError('FDN Save/cold Details must preserve the exact complete record transition')
    verifies=[(0x9000,3,10)]*(2 if writing else 1)
    if statuses(text,0x20)!=verifies or statuses(text,0x20,2)!=verifies:
        raise ValueError('FDN record authentication results differ')
    if re.findall(r'header cla=a0 ins=20 p1=00 p2=(\w\w) p3=08',text)!=(['01','02'] if writing else ['01']):
        raise ValueError('FDN records require PIN1 and PIN2 only for Save')
    if re.search(r'header cla=a0 ins=(?:04|24|26|28|2c) ',text):
        raise ValueError('FDN record phase changed credentials or ADN validity')
    targets=re.findall(r'header cla=a0 ins=44 p1=00 p2=00 p3=00 selected=(\w{4})',text)
    if targets!=['6f7e','6f07'] or statuses(text,0x44)!=[(0x9000,3,10)]*2:
        raise ValueError('FDN record startup identity rehabilitation differs')
    updates=re.findall(r'header cla=a0 ins=dc p1=(\w\w) p2=(\w\w) p3=(\w\w) selected=(\w{4})',text)
    if updates!=([('01','04','20','6f3b')] if writing else []) or statuses(text,0xdc)!=([(0x9000,3,10)] if writing else []):
        raise ValueError('FDN UPDATE must target record1 of EF_FDN only after Save')
    if re.findall(r'header cla=a0 ins=b2 p1=(\w\w) p2=04 p3=20 selected=6f3b',text)!=[f'{index:02x}' for index in range(1,11)]+(['01'] if writing else []):
        raise ValueError('FDN record startup/readback extent differs')
    contacts(text,'6250_fdn',items)
    order=[('PIN1',r'header cla=a0 ins=20 p1=00 p2=01 p3=08'),
           ('registered',r'LAPDm Location Updating Accept acknowledged'),
           ('Number list',r'6250_fdn: key=Left Softkey / Menu state=1 t=46\.')]
    if writing:
        order += [('PIN2 confirm',r'6250_fdn: key=Left Softkey / Menu state=1 t=57\.'),
                  ('PIN2',r'header cla=a0 ins=20 p1=00 p2=02 p3=08'),
                  ('verified',r'SIM status ins=20 sw=9000'),
                  ('Save',r'6250_fdn: key=Left Softkey / Menu state=1 t=71\.'),
                  ('UPDATE',r'header cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3b'),
                  ('saved',r'SIM status ins=dc sw=9000'),
                  ('readback',r'header cla=a0 ins=b2 p1=01 p2=04 p3=20 selected=6f3b')]
    order += [('Menu',rf'6250_fdn: key=Left Softkey / Menu state=1 t={80 if writing else 62}\.'),
              ('complete',r'6250_fdn: complete=1 firmware_state_forcing=0')]
    require_ordered(text,order,'6250 FDN record')


def check_frames(directory,mode):
    actions(mode)
    check_rgb_frame(directory/'6250_fdn_40.png','72dbb095c41e70afd8487a989d70e1a57830f56068b194245e1a7f22a9a8d49d')
    frames={
        50:'de9dd2e4fb6d913c2a51dc9fd1c2b4d1f88bdd1b2ae4281a031bcbb7a7816612',
        57:'f65dfa98b41b8e71a94b214c3f854e49c9576aaad7a0a326e6066c2509d6ad37',
        65:'ac4453b4898f9ed8653596e933f81f3987ab2b0dadc849461333b74d6394b7a0',
        71:'c7629e485ea995654253901d181e1b828206d89c5981bf55c1adadaae430e3eb',
        78:FRAMES[76],80:FRAMES[78],
    } if mode=='write' else {
        50:'4ac94e50abdc52b7104a8811343ff57e647f8bd0f1f552cf8666f5ec19f88b53',
        53:'4ac94e50abdc52b7104a8811343ff57e647f8bd0f1f552cf8666f5ec19f88b53',
        60:FRAMES[76],62:FRAMES[78],
    }
    for second,digest in frames.items():
        check_rgb_frame(directory/f'6250_fdn_{second}.png',digest)

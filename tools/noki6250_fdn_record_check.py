"""Own NHM-3 protected FDN records and cold persistence."""
import re
from tools.noki6250_fdn_check import actions as fdn_actions,initial_card
from tools.nsk3_phonebook_check import expected_record
from tools.noki6210_puk_check import contacts,check_rgb_frame
from tools.noki6250_pin2_change_check import FRAMES
from tools.sim_security_trace_check import statuses
from tools.radio_call_lifecycle_common import require_ordered


def actions(mode):
    if mode not in ('write','read','edit','edited-read','cancel','erase','empty','wrong'):
        raise ValueError('unknown own FDN record mode')
    items=[item for item in fdn_actions(1) if item[0]<=40]
    items += [(42,'Scroll Down'),(43,'Scroll Down'),(46,'Left Softkey / Menu')]
    if mode in ('write','wrong'):
        items += [(48,'Scroll Down'),(50,'Left Softkey / Menu')]
        items += [(53+i,'Keypad '+digit) for i,digit in enumerate('5675' if mode=='wrong' else '5678')]
        items += [(57,'Left Softkey / Menu')]
        if mode=='wrong':return items+[(64,'End'),(66,'Left Softkey / Menu')]
        items += [(60+i,'Keypad '+digit) for i,digit in enumerate('12345')]
        items += [(65,'Left Softkey / Menu'),(68,'Keypad 2'),(71,'Left Softkey / Menu'),
                  (78,'End'),(80,'Left Softkey / Menu')]
    elif mode=='edit':
        items += [(48,'Left Softkey / Menu'),(50,'Scroll Down'),(51,'Scroll Down'),
                  (54,'Left Softkey / Menu')]
        items += [(57+i,'Keypad '+digit) for i,digit in enumerate('5678')]
        items += [(61,'Left Softkey / Menu'),(64,'Left Softkey / Menu'),
                  (67,'Right Softkey / C'),(70,'Keypad 6'),(73,'Left Softkey / Menu'),
                  (80,'End'),(82,'Left Softkey / Menu')]
    elif mode in ('cancel','erase'):
        items += [(48,'Left Softkey / Menu'),(50,'Scroll Down'),(51,'Scroll Down'),
                  (52,'Scroll Down'),(55,'Left Softkey / Menu')]
        items += [(58+i,'Keypad '+digit) for i,digit in enumerate('5678')]
        items += [(62,'Left Softkey / Menu'),
                  (65,'Right Softkey / C' if mode=='cancel' else 'Left Softkey / Menu'),
                  (72,'End'),(74,'Left Softkey / Menu')]
    elif mode=='empty':
        items += [(54,'End'),(56,'Left Softkey / Menu')]
    else:
        items += [(48,'Left Softkey / Menu'),(50,'Left Softkey / Menu'),
                  (53,'Left Softkey / Menu'),(60,'End'),(62,'Left Softkey / Menu')]
    return items


def expected_card(saved,edited=False,*,pin2_attempts=3):
    card=bytearray(initial_card(1,pin2_attempts=pin2_attempts))
    if saved:card[3578:3610]=expected_record()
    if saved and edited:card[3600]=0xf6
    return bytes(card)


def check(text,initial,final,mode,*,pin2_attempts=3):
    items=actions(mode)
    writing=mode in ('write','edit','erase')
    protected=mode in ('write','edit','cancel','erase','wrong')
    expected_final=bytearray(expected_card(mode not in ('erase','empty','wrong'),mode in ('edit','edited-read','cancel')))
    if not protected:expected_final[3517]=pin2_attempts
    if mode=='wrong':expected_final[3517]=pin2_attempts-1
    if (initial!=expected_card(mode not in ('write','empty','wrong'),mode in ('edited-read','cancel','erase'),pin2_attempts=pin2_attempts) or
            final!=bytes(expected_final)):
        raise ValueError('FDN record phase must preserve the exact whole-card transition')
    result=0x9804 if mode=='wrong' else 0x9000
    verifies=[(0x9000,3,10)]+([(result,3,10)] if protected else [])
    pin2=[(0x9000,pin2_attempts,10)]+([(result,pin2_attempts-1 if mode=='wrong' else 3,10)] if protected else [])
    if statuses(text,0x20)!=verifies or statuses(text,0x20,2)!=pin2:
        raise ValueError('FDN record authentication results differ')
    if re.findall(r'header cla=a0 ins=20 p1=00 p2=(\w\w) p3=08',text)!=(['01','02'] if protected else ['01']):
        raise ValueError('FDN record CHV selectors differ')
    if re.search(r'header cla=a0 ins=(?:04|24|26|28|2c) ',text):
        raise ValueError('FDN record phase changed credentials or ADN validity')
    targets=re.findall(r'header cla=a0 ins=44 p1=00 p2=00 p3=00 selected=(\w{4})',text)
    if targets!=['6f7e','6f07'] or statuses(text,0x44)!=[(0x9000,3,10)]*2:
        raise ValueError('FDN record startup identity rehabilitation differs')
    updates=re.findall(r'header cla=a0 ins=dc p1=(\w\w) p2=(\w\w) p3=(\w\w) selected=(\w{4})',text)
    if updates!=([('01','04','20','6f3b')] if writing else []) or statuses(text,0xdc)!=([(0x9000,3,10)] if writing else []):
        raise ValueError('FDN UPDATE must target record1 of EF_FDN only after confirmation')
    if re.findall(r'header cla=a0 ins=b2 p1=(\w\w) p2=04 p3=20 selected=6f3b',text)!=[f'{index:02x}' for index in range(1,11)]+(['01'] if mode in ('write','edit') else []):
        raise ValueError('FDN record startup/readback extent differs')
    contacts(text,'6250_fdn',items)
    order=[('PIN1',r'header cla=a0 ins=20 p1=00 p2=01 p3=08'),
           ('registered',r'LAPDm Location Updating Accept acknowledged'),
           ('Number list',r'6250_fdn: key=Left Softkey / Menu state=1 t=46\.')]
    if protected:
        confirm={'write':57,'edit':61,'cancel':62,'erase':62,'wrong':57}[mode]
        order += [('PIN2 confirm',rf'6250_fdn: key=Left Softkey / Menu state=1 t={confirm}\.'),
                  ('PIN2',r'header cla=a0 ins=20 p1=00 p2=02 p3=08'),
                  ('verified',rf'SIM status ins=20 sw={result:04x}')]
    if writing:
        save={'write':71,'edit':73,'erase':65}[mode]
        order += [('Save/erase',rf'6250_fdn: key=Left Softkey / Menu state=1 t={save}\.'),
                  ('UPDATE',r'header cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3b'),
                  ('saved',r'SIM status ins=dc sw=9000')]
        if mode in ('write','edit'):
            order += [('readback',r'header cla=a0 ins=b2 p1=01 p2=04 p3=20 selected=6f3b')]
    if mode=='cancel':order += [('cancel',r'6250_fdn: key=Right Softkey / C state=1 t=65\.')]
    menu={'write':80,'edit':82,'cancel':74,'erase':74,'empty':56,'wrong':66}.get(mode,62)
    order += [('Menu',rf'6250_fdn: key=Left Softkey / Menu state=1 t={menu}\.'),
              ('complete',r'6250_fdn: complete=1 firmware_state_forcing=0')]
    require_ordered(text,order,'6250 FDN record')


def check_frames(directory,mode):
    actions(mode)
    check_rgb_frame(directory/'6250_fdn_40.png','72dbb095c41e70afd8487a989d70e1a57830f56068b194245e1a7f22a9a8d49d')
    frames={'write':{
        50:'de9dd2e4fb6d913c2a51dc9fd1c2b4d1f88bdd1b2ae4281a031bcbb7a7816612',
        57:'f65dfa98b41b8e71a94b214c3f854e49c9576aaad7a0a326e6066c2509d6ad37',
        65:'ac4453b4898f9ed8653596e933f81f3987ab2b0dadc849461333b74d6394b7a0',
        71:'c7629e485ea995654253901d181e1b828206d89c5981bf55c1adadaae430e3eb',
        78:FRAMES[76],80:FRAMES[78],
    },'read':{
        50:'4ac94e50abdc52b7104a8811343ff57e647f8bd0f1f552cf8666f5ec19f88b53',
        53:'4ac94e50abdc52b7104a8811343ff57e647f8bd0f1f552cf8666f5ec19f88b53',
        60:FRAMES[76],62:FRAMES[78],
    },'edit':{
        54:'de9dd2e4fb6d913c2a51dc9fd1c2b4d1f88bdd1b2ae4281a031bcbb7a7816612',
        61:'fe0186ba9b510d906b413464db711156bc6a74bc28dfbdd7e9e83cdf3fb53905',
        64:'346299a94fc3900e921663377ad1191b0f099ba8d7d1933401543171615c0620',
        73:'c7629e485ea995654253901d181e1b828206d89c5981bf55c1adadaae430e3eb',
        80:FRAMES[76],82:FRAMES[78],
    },'edited-read':{
        50:'99cc0b0c75030851acbbcc1624bb2e89eb491a4cb328f4ff3e9a3a5cfa9b49b0',
        53:'99cc0b0c75030851acbbcc1624bb2e89eb491a4cb328f4ff3e9a3a5cfa9b49b0',
        60:FRAMES[76],62:FRAMES[78],
    },'cancel':{
        55:'de9dd2e4fb6d913c2a51dc9fd1c2b4d1f88bdd1b2ae4281a031bcbb7a7816612',
        62:'8f65425c18203075a14d304fc337aec6dfde47cd76eef7cdea1786acd8eefa39',
        65:'0886bf2512ae194b2fabbf467ff2023401690afcd4c2e43d7500523a79fed747',
        72:FRAMES[76],74:FRAMES[78],
    },'erase':{
        55:'de9dd2e4fb6d913c2a51dc9fd1c2b4d1f88bdd1b2ae4281a031bcbb7a7816612',
        62:'8f65425c18203075a14d304fc337aec6dfde47cd76eef7cdea1786acd8eefa39',
        65:'606cb1d89b0c4c116e071bd6857b7de4455ad3dc1d6ff78139ad4030fc94476f',
        72:FRAMES[76],74:FRAMES[78],
    },'wrong':{
        50:'de9dd2e4fb6d913c2a51dc9fd1c2b4d1f88bdd1b2ae4281a031bcbb7a7816612',
        57:'c1371c4cdca6b0f72c9c1dcc6e602de981330f67719714bc5b124e1a3b4fcfcd',
        64:FRAMES[76],66:FRAMES[78],
    },'empty':{
        46:'31ad5d7d50e7fa052cdad00312b163ac872c32860172637839b35f4b60eb95b0',
        54:FRAMES[76],56:FRAMES[78],
    }}[mode]
    for second,digest in frames.items():
        check_rgb_frame(directory/f'6250_fdn_{second}.png',digest)

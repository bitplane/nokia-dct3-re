"""6210 protected FDN records with cold Details and empty-list checks."""
import re
from tools.noki6210_fdn_check import input_contacts as fdn_contacts
from tools.noki6210_puk_check import profile,contacts,check_rgb_frame
from tools.sim_fdn_profile import extend
from tools.nsk3_phonebook_check import expected_record
from tools.sim_security_trace_check import statuses
from tools.radio_call_lifecycle_common import require_ordered

FRAMES={
    'write':{
        '6210_fdn_key_50.png':'de9dd2e4fb6d913c2a51dc9fd1c2b4d1f88bdd1b2ae4281a031bcbb7a7816612',
        '6210_fdn_key_57.png':'f65dfa98b41b8e71a94b214c3f854e49c9576aaad7a0a326e6066c2509d6ad37',
        '6210_fdn_key_65.png':'ac4453b4898f9ed8653596e933f81f3987ab2b0dadc849461333b74d6394b7a0',
        '6210_fdn_key_71.png':'c7629e485ea995654253901d181e1b828206d89c5981bf55c1adadaae430e3eb',
    },
    'read':{
        '6210_fdn_key_50.png':'4ac94e50abdc52b7104a8811343ff57e647f8bd0f1f552cf8666f5ec19f88b53',
        '6210_fdn_key_53.png':'4ac94e50abdc52b7104a8811343ff57e647f8bd0f1f552cf8666f5ec19f88b53',
    },
    'edit':{
        '6210_fdn_key_54.png':'de9dd2e4fb6d913c2a51dc9fd1c2b4d1f88bdd1b2ae4281a031bcbb7a7816612',
        '6210_fdn_key_61.png':'fe0186ba9b510d906b413464db711156bc6a74bc28dfbdd7e9e83cdf3fb53905',
        '6210_fdn_key_64.png':'346299a94fc3900e921663377ad1191b0f099ba8d7d1933401543171615c0620',
        '6210_fdn_key_73.png':'c7629e485ea995654253901d181e1b828206d89c5981bf55c1adadaae430e3eb',
    },
    'edited-read':{
        '6210_fdn_key_50.png':'99cc0b0c75030851acbbcc1624bb2e89eb491a4cb328f4ff3e9a3a5cfa9b49b0',
        '6210_fdn_key_53.png':'99cc0b0c75030851acbbcc1624bb2e89eb491a4cb328f4ff3e9a3a5cfa9b49b0',
    },
    'cancel':{
        '6210_fdn_key_55.png':'de9dd2e4fb6d913c2a51dc9fd1c2b4d1f88bdd1b2ae4281a031bcbb7a7816612',
        '6210_fdn_key_62.png':'8f65425c18203075a14d304fc337aec6dfde47cd76eef7cdea1786acd8eefa39',
        '6210_fdn_key_65.png':'0886bf2512ae194b2fabbf467ff2023401690afcd4c2e43d7500523a79fed747',
    },
    'erase':{
        '6210_fdn_key_55.png':'de9dd2e4fb6d913c2a51dc9fd1c2b4d1f88bdd1b2ae4281a031bcbb7a7816612',
        '6210_fdn_key_62.png':'8f65425c18203075a14d304fc337aec6dfde47cd76eef7cdea1786acd8eefa39',
        '6210_fdn_key_65.png':'606cb1d89b0c4c116e071bd6857b7de4455ad3dc1d6ff78139ad4030fc94476f',
    },
    'empty':{'6210_fdn_key_46.png':'31ad5d7d50e7fa052cdad00312b163ac872c32860172637839b35f4b60eb95b0'},
    'wrong':{
        '6210_fdn_key_50.png':'de9dd2e4fb6d913c2a51dc9fd1c2b4d1f88bdd1b2ae4281a031bcbb7a7816612',
        '6210_fdn_key_57.png':'c1371c4cdca6b0f72c9c1dcc6e602de981330f67719714bc5b124e1a3b4fcfcd',
    },
}


def check_frames(directory,mode):
    input_contacts(mode)
    for name,digest in FRAMES[mode].items():
        check_rgb_frame(directory/name,digest)
    check_rgb_frame(directory/'6210_fdn_then_menu.png',
                    '5a3c0ca03c73478aa8c645a8a263e5edb04cc833639c893dc638ac18c300ff12')


def input_contacts(mode):
    if mode not in ('write','read','edit','edited-read','cancel','erase','empty','wrong'):
        raise ValueError('unknown 6210 FDN record mode')
    items=[row for row in fdn_contacts(1) if row[0]<=40]
    items += [(42,'Scroll Down'),(43,'Scroll Down'),(46,'Left Softkey / Menu')]
    if mode in ('write','wrong'):
        items += [(48,'Scroll Down'),(50,'Left Softkey / Menu')]
        items += [(53+i,'Keypad '+d) for i,d in enumerate('5675' if mode=='wrong' else '5678')]
        items += [(57,'Left Softkey / Menu')]
        if mode=='wrong':
            items += [(64,'End'),(66,'Left Softkey / Menu')]
        else:
            items += [(60+i,'Keypad '+d) for i,d in enumerate('12345')]
            items += [(65,'Left Softkey / Menu'),(68,'Keypad 2'),(71,'Left Softkey / Menu'),
                      (78,'End'),(80,'Left Softkey / Menu')]
    elif mode=='edit':
        items += [(48,'Left Softkey / Menu'),(50,'Scroll Down'),(51,'Scroll Down'),
                  (54,'Left Softkey / Menu')]
        items += [(57+i,'Keypad '+d) for i,d in enumerate('5678')]
        items += [(61,'Left Softkey / Menu'),(64,'Left Softkey / Menu'),
                  (67,'Right Softkey / C'),(70,'Keypad 6'),(73,'Left Softkey / Menu'),
                  (80,'End'),(82,'Left Softkey / Menu')]
    elif mode in ('cancel','erase'):
        items += [(48,'Left Softkey / Menu'),(50,'Scroll Down'),(51,'Scroll Down'),
                  (52,'Scroll Down'),(55,'Left Softkey / Menu')]
        items += [(58+i,'Keypad '+d) for i,d in enumerate('5678')]
        items += [(62,'Left Softkey / Menu'),
                  (65,'Right Softkey / C' if mode=='cancel' else 'Left Softkey / Menu'),
                  (72,'End'),(74,'Left Softkey / Menu')]
    elif mode=='empty':
        items += [(54,'End'),(56,'Left Softkey / Menu')]
    else:
        items += [(48,'Left Softkey / Menu'),(50,'Left Softkey / Menu'),
                  (53,'Left Softkey / Menu'),(60,'End'),(62,'Left Softkey / Menu')]
    return items


def expected_card(saved,edited=False):
    card=bytearray(extend(profile(3,True)))
    card[3575]=0
    if saved:
        card[3578:3610]=expected_record()
        if edited:
            card[3600]=0xf6
    return bytes(card)


def check(text,initial,final,mode,pin2_attempts=3):
    if pin2_attempts not in (2,3):
        raise ValueError('FDN record retry predecessor must be 2 or 3')
    items=input_contacts(mode)
    writing=mode in ('write','edit','erase')
    protected=mode in ('write','edit','cancel','erase','wrong')
    edited=mode in ('edit','edited-read')
    before=bytearray(expected_card(mode not in ('write','empty','wrong'),mode=='edited-read'))
    before[3517]=pin2_attempts
    after=bytearray(expected_card(mode not in ('erase','empty','wrong'),edited))
    if mode=='wrong':
        after[3517]=pin2_attempts-1
    if initial!=bytes(before) or final!=bytes(after):
        raise ValueError('FDN record source/result differs from the exact record transition')
    contacts(text,'6210_fdn_input',items)
    result=0x9804 if mode=='wrong' else 0x9000
    pin1=[(0x9000,3,10)]+([(result,3,10)] if protected else [])
    pin2=[(0x9000,pin2_attempts,10)]+([(result,pin2_attempts-1 if mode=='wrong' else 3,10)] if protected else [])
    if statuses(text,0x20)!=pin1 or statuses(text,0x20,2)!=pin2:
        raise ValueError('FDN record PIN1/PIN2 verification differs')
    if re.findall(r'header cla=a0 ins=20 p1=00 p2=(\w\w) p3=08',text)!=(['01','02'] if protected else ['01']):
        raise ValueError('FDN record CHV selectors differ')
    if any(statuses(text,ins) for ins in (4,0x24,0x26,0x28,0x2c)):
        raise ValueError('FDN record changed validity or credentials')
    targets=re.findall(r'header cla=a0 ins=44 p1=00 p2=00 p3=00 selected=(\w{4})',text)
    if targets!=['6f7e','6f07'] or statuses(text,0x44)!=[(0x9000,3,10)]*2:
        raise ValueError('FDN record startup identity rehabilitation differs')
    updates=re.findall(r'header cla=a0 ins=dc p1=(\w\w) p2=(\w\w) p3=(\w\w) selected=(\w{4})',text)
    if updates!=([('01','04','20','6f3b')] if writing else []) or statuses(text,0xdc)!=([(0x9000,3,10)] if writing else []):
        raise ValueError('FDN record UPDATE target/result differs')
    records=re.findall(r'header cla=a0 ins=b2 p1=(\w\w) p2=04 p3=20 selected=6f3b',text)
    # Save/Edit reread the updated record; Erase completes without that readback.
    if records!=[f'{i:02x}' for i in range(1,11)]+(['01'] if mode in ('write','edit') else []):
        raise ValueError('FDN record startup did not read all ten records')
    order=[('PIN1',r'SIM status ins=20 sw=9000'),
           ('registration',r'LAPDm Location Updating Accept acknowledged nr=1'),
           ('Number list',r'key=Left Softkey / Menu state=1 t=46\.')]
    if protected:
        confirm={'write':57,'wrong':57,'edit':61,'cancel':62,'erase':62}[mode]
        order += [('PIN2 confirm',rf'key=Left Softkey / Menu state=1 t={confirm}\.'),
                  ('VERIFY PIN2',r'header cla=a0 ins=20 p1=00 p2=02 p3=08'),
                  ('verified',rf'SIM status ins=20 sw={result:04x}')]
    if writing:
        save={'write':71,'edit':73,'erase':65}[mode]
        order += [('save',rf'key=Left Softkey / Menu state=1 t={save}\.'),
                  ('UPDATE',r'header cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3b'),
                  ('saved',r'SIM status ins=dc sw=9000')]
        if mode in ('write','edit'):
            order += [('readback',r'header cla=a0 ins=b2 p1=01 p2=04 p3=20 selected=6f3b')]
    if mode=='cancel':
        order += [('cancel',r'key=Right Softkey / C state=1 t=65\.')]
    menu={'write':80,'wrong':66,'edit':82,'cancel':74,'erase':74,'empty':56}.get(mode,62)
    order += [('Menu',rf'key=Left Softkey / Menu state=1 t={menu}\.'),
              ('complete',r'6210_fdn_input: complete=1 firmware_state_forcing=0')]
    require_ordered(text,order,'6210 FDN record')

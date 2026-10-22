"""6210 physical PIN exhaustion and cold PUK/new-PIN recovery."""
import re
import hashlib

from tools.make_sim_card_profile import make_profile
from tools.sim_security_trace_check import statuses
from tools.radio_call_lifecycle_common import require_ordered

FRAMES={
    '6210_pin_blocked.png':'2f5d94d74625776d287cd5d7d452943865fee0755ba56b93920a94b85408d029',
    '6210_puk_prompt.png':'2f5d94d74625776d287cd5d7d452943865fee0755ba56b93920a94b85408d029',
    '6210_puk_new_pin.png':'2175c020da9e667de3adf54a2fad22991e9cb713f10459a71e40d7dcfbdfce7d',
    '6210_puk_confirm.png':'56857396bfdf762074a04bc14a8d1d23654b87460003c51a6c0548a18367293e',
    '6210_puk_idle.png':'570411100c4ce1b58be8303a8c912a08b698eaa6328605d110d54615be409eca',
}


def check_frames(directory,names):
    from PIL import Image
    for name in names:
        with Image.open(directory/name) as image:
            if image.size!=(96,60) or hashlib.sha256(image.convert('RGB').tobytes()).hexdigest()!=FRAMES[name]:
                raise ValueError('6210 PUK frame differs: '+name)


def profile(retries=0,registered=False,pin=b'1234\xff\xff\xff\xff'):
    card=bytearray(make_profile(True)+bytes.fromhex('00f110')+b'\xff'*43)
    card[3516]=retries
    card[3484:3492]=pin
    if registered:
        card[1604:1609]=bytes.fromhex('00f1100001')
        card[1610]=0
    return bytes(card)


def contacts(text,prefix,expected):
    for state,fraction in ((1,'000000000'),(0,'150000000')):
        observed=re.findall(rf'{prefix}: key=(.*?) state={state} t=(\d+)\.{fraction}',text)
        if observed!=[(key,str(second)) for second,key in expected]:
            raise ValueError('6210 PIN/PUK physical contacts differ')


def check_blocked(text,card):
    if statuses(text,0x20)!=[(0x9804,2,10),(0x9804,1,10),(0x9840,0,10)]:
        raise ValueError('6210 must exhaust all three PIN attempts physically')
    keys=[]
    for start in (8,16,24):
        keys.extend((start+i,'Keypad 9') for i in range(4))
        keys.append((start+4,'Left Softkey / Menu'))
    contacts(text,'6210_pin_retry_input',keys)
    if card!=profile() or 'wrong_complete=1 firmware_state_forcing=0' not in text:
        raise ValueError('blocked predecessor card differs beyond PIN1 retries')
    if statuses(text,0x2c) or 'Location Updating Accept acknowledged' in text:
        raise ValueError('blocked predecessor unexpectedly unblocked or registered')


def check_unblocked(text,initial,final):
    if initial!=profile():
        raise ValueError('PUK recovery must start from the exact blocked predecessor')
    if statuses(text,0x20) or statuses(text,0x2c)!=[(0x9000,3,10)]:
        raise ValueError('cold PUK recovery must unblock once without VERIFY shortcut')
    keys=[(7+i,'Keypad '+str(i)) for i in range(1,9)]+[(16,'Left Softkey / Menu')]
    for start in (20,28):
        keys.extend((start+i,'Keypad '+str(digit)) for i,digit in enumerate((2,4,6,8)))
        keys.append((start+4,'Left Softkey / Menu'))
    keys.append((52,'Left Softkey / Menu'))
    contacts(text,'6210_puk_input',keys)
    require_ordered(text,[
        ('new PIN confirmation',r'key=Left Softkey / Menu state=1 t=32\.'),
        ('UNBLOCK CHV1',r'SIM status ins=2c sw=9000'),
        ('registration',r'LAPDm Location Updating Accept acknowledged nr=1'),
        ('Menu',r'key=Left Softkey / Menu state=1 t=52\.'),
        ('complete',r'6210_puk_input: complete=1 firmware_state_forcing=0'),
    ],'6210 PUK recovery')
    if final!=profile(3,True,b'2468\xff\xff\xff\xff'):
        raise ValueError('PUK recovery changed unrelated card data or failed to replace PIN1')


def check_reauthenticated(text,initial,final):
    if initial!=profile(3,True,b'2468\xff\xff\xff\xff') or final!=initial:
        raise ValueError('cold new-PIN authentication must preserve the exact recovered card')
    if statuses(text,0x20)!=[(0x9000,3,10)] or statuses(text,0x2c):
        raise ValueError('cold new-PIN process must VERIFY once, without another UNBLOCK')
    contacts(text,'6210_puk_input',[(8+i,'Keypad '+str(d)) for i,d in enumerate((2,4,6,8))]+
             [(12,'Left Softkey / Menu'),(34,'Left Softkey / Menu')])
    require_ordered(text,[
        ('confirm',r'key=Left Softkey / Menu state=1 t=12\.'),
        ('VERIFY',r'SIM status ins=20 sw=9000'),
        ('registration',r'LAPDm Location Updating Accept acknowledged nr=1'),
        ('Menu',r'key=Left Softkey / Menu state=1 t=34\.'),
        ('complete',r'6210_puk_input: reauthenticated=1 firmware_state_forcing=0'),
    ],'6210 cold new-PIN authentication')

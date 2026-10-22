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
    # Only the wrong-PUK fixture asserts this result frame; successful recovery differs.
    '6210_puk_result.png':'2f5d94d74625776d287cd5d7d452943865fee0755ba56b93920a94b85408d029',
}


PERMANENT_FRAME='44cc6469b3934dfc418887d4efde6ba088cf7fc86dd20d94114e3035a71f86e8'
PERMANENT_INPUT_FRAMES={
    '6210_puk_prompt.png':'87e8208502b6de9a3f3ff16396408599abb8c863f6fa0d92678ec117fc567f3c',
    '6210_puk_result.png':'3a9ecadb464898a275d2c6708ba3bd919e7e46cf30cb9668c5dc3a6f0f10fab1',
    '6210_puk_permanent_after.png':FRAMES['6210_puk_prompt.png'],
}


def check_rgb_frame(path,digest):
    from PIL import Image
    with Image.open(path) as image:
        if image.size!=(96,60) or hashlib.sha256(image.convert('RGB').tobytes()).hexdigest()!=digest:
            raise ValueError('6210 PUK frame differs: '+str(path))


def check_frames(directory,names):
    for name in names:
        check_rgb_frame(directory/name,FRAMES[name])


def profile(retries=0,registered=False,pin=b'1234\xff\xff\xff\xff',puk_attempts=10):
    card=bytearray(make_profile(True)+bytes.fromhex('00f110')+b'\xff'*43)
    card[3516]=retries
    card[3518]=puk_attempts
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


def check_unblocked(text,initial,final,puk_attempts=10):
    if initial!=profile(puk_attempts=puk_attempts):
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


def check_wrong_puk(text,initial,final,attempt=1):
    if not 1<=attempt<=10:
        raise ValueError('PUK attempt must be in the complete ten-attempt domain')
    remaining=10-attempt
    if initial!=profile(puk_attempts=remaining+1) or final!=profile(puk_attempts=remaining):
        raise ValueError('wrong PUK must consume exactly one retry without changing PIN or unrelated card data')
    if statuses(text,0x20) or statuses(text,0x2c)!=[(0x9840 if not remaining else 0x9804,0,remaining)]:
        raise ValueError('wrong PUK must reject exactly one UNBLOCK without VERIFY')
    items=[(8+i,'Keypad 9') for i in range(8)]+[(16,'Left Softkey / Menu')]
    for start in (20,28):
        items.extend((start+i,'Keypad '+str(d)) for i,d in enumerate((2,4,6,8)))
        items.append((start+4,'Left Softkey / Menu'))
    contacts(text,'6210_puk_input',items)
    require_ordered(text,[
        ('confirm',r'key=Left Softkey / Menu state=1 t=32\.'),
        ('failed UNBLOCK',rf'SIM status ins=2c sw={"9840" if not remaining else "9804"}'),
        ('complete',r'6210_puk_input: wrong_complete=1 firmware_state_forcing=0'),
    ],'6210 wrong PUK')
    if 'Location Updating Accept acknowledged' in text:
        raise ValueError('wrong PUK unexpectedly registered the handset')


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


def check_permanent(text,initial,final):
    if initial!=profile(puk_attempts=0) or final!=initial:
        raise ValueError('permanently rejected cold card must remain completely unchanged')
    if any(statuses(text,ins) for ins in (0x20,0x24,0x26,0x28,0x2c)):
        raise ValueError('permanent rejection must not attempt further credentials')
    if 'Location Updating Accept acknowledged' in text:
        raise ValueError('permanently blocked card unexpectedly registered')
    if text.count('6210_puk_permanent: complete=1 credential_input=0 firmware_state_forcing=0')!=1:
        raise ValueError('permanent rejection observation incomplete')


def check_permanent_retry(text,initial,final):
    if initial!=profile(puk_attempts=0) or final!=initial:
        raise ValueError('exhausted card must stay unchanged despite physical PUK-pattern input')
    if any(statuses(text,ins) for ins in (0x20,0x24,0x26,0x28,0x2c)) or 'Location Updating Accept acknowledged' in text:
        raise ValueError('permanently blocked UI unexpectedly attempted credentials or registered')
    if re.search(r'GSM service (?:establish|uplink) sapi=0 pd=(?:03|05) ',text):
        raise ValueError('permanent credential-pattern control unexpectedly started a call or mobility service')
    items=[(7+i,'Keypad '+str(i)) for i in range(1,9)]+[(16,'Left Softkey / Menu')]
    for start in (20,28):
        items.extend((start+i,'Keypad '+str(d)) for i,d in enumerate((2,4,6,8)))
        items.append((start+4,'Left Softkey / Menu'))
    contacts(text,'6210_puk_input',items+[(36,'End')])
    if text.count('6210_puk_input: permanent_complete=1 firmware_state_forcing=0')!=1:
        raise ValueError('permanent physical-input control incomplete')

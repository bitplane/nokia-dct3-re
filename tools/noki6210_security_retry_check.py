"""6210 physical wrong-PIN recovery and complete final SIM-state contract."""
import re
import hashlib

from tools.make_sim_card_profile import make_profile
from tools.radio_call_lifecycle_common import require_ordered
from tools.sim_security_trace_check import statuses

KEYS=[(8+i,'Keypad 9') for i in range(4)]+[(12,'Left Softkey / Menu')]+[
    (16+i,'Keypad '+str(i+1)) for i in range(4)]+[(20,'Left Softkey / Menu'),(40,'Left Softkey / Menu')]
FRAMES={
    '6210_pin_retry_prompt.png':'bb528795a76fc5e4cf6fc34dcd42a039cd21dbb5b86c7eaf8d3e779b572143d5',
    '6210_pin_retry_rejected.png':'6276f518b52dee0ea6bd7953e5e036fa9297fcc7f226bee0d859704658495a60',
    '6210_pin_retry_idle.png':'570411100c4ce1b58be8303a8c912a08b698eaa6328605d110d54615be409eca',
}


def check_frames(directory,wrong_only=False):
    from PIL import Image
    for name,digest in FRAMES.items():
        if wrong_only and name=='6210_pin_retry_idle.png':
            continue
        with Image.open(directory/name) as image:
            if image.size!=(96,60) or hashlib.sha256(image.convert('RGB').tobytes()).hexdigest()!=digest:
                raise ValueError('6210 PIN retry frame differs: '+name)


def check(text, card):
    if statuses(text,0x20)!=[(0x9804,2,10),(0x9000,3,10)]:
        raise ValueError('6210 must reject one PIN then reset retries on correct authentication')
    for state,fraction in ((1,'000000000'),(0,'150000000')):
        observed=re.findall(rf'6210_pin_retry_input: key=(.*?) state={state} t=(\d+)\.{fraction}',text)
        if observed!=[(key,str(second)) for second,key in KEYS]:
            raise ValueError('6210 wrong/correct PIN physical contact sequence differs')
    require_ordered(text,[
        ('wrong PIN confirm',r'key=Left Softkey / Menu state=1 t=12\.'),
        ('rejected VERIFY',r'SIM status ins=20 sw=9804'),
        ('correct PIN confirm',r'key=Left Softkey / Menu state=1 t=20\.'),
        ('accepted VERIFY',r'SIM status ins=20 sw=9000'),
        ('registration',r'LAPDm Location Updating Accept acknowledged nr=1'),
        ('Menu',r'key=Left Softkey / Menu state=1 t=40\.'),
        ('complete',r'6210_pin_retry_input: complete=1 firmware_state_forcing=0'),
    ],'6210 wrong-PIN recovery')
    expected=bytearray(make_profile(True)+bytes.fromhex('00f110')+b'\xff'*43)
    expected[1604:1609]=bytes.fromhex('00f1100001')
    expected[1610]=0
    if card!=bytes(expected):
        raise ValueError('6210 retry recovery changed unrelated card data or credentials')


def check_wrong(text,card):
    if statuses(text,0x20)!=[(0x9804,2,10)]:
        raise ValueError('6210 cold predecessor must contain one failed PIN only')
    for state,fraction in ((1,'000000000'),(0,'150000000')):
        observed=re.findall(rf'6210_pin_retry_input: key=(.*?) state={state} t=(\d+)\.{fraction}',text)
        if observed!=[(key,str(second)) for second,key in KEYS[:5]]:
            raise ValueError('wrong-only physical PIN sequence differs')
    if 'wrong_complete=1 firmware_state_forcing=0' not in text or 'Location Updating Accept acknowledged' in text:
        raise ValueError('wrong-only predecessor completed authentication or did not finish')
    expected=bytearray(make_profile(True)+bytes.fromhex('00f110')+b'\xff'*43)
    expected[3516]=2
    if card!=bytes(expected):
        raise ValueError('failed PIN must persist exactly one retry and no other card change')


def check_cold(text,initial,final):
    expected=bytearray(make_profile(True)+bytes.fromhex('00f110')+b'\xff'*43)
    expected[3516]=2
    if initial!=bytes(expected):
        raise ValueError('cold recovery must start from the exact failed-PIN card')
    if statuses(text,0x20)!=[(0x9000,3,10)]:
        raise ValueError('cold recovery must authenticate once and reset retries')
    require_ordered(text,[
        ('physical confirm',r'6210_security_physical: action=confirm'),
        ('VERIFY',r'SIM status ins=20 sw=9000'),
        ('registration',r'LAPDm Location Updating Accept acknowledged nr=1'),
        ('Menu',r'6210_security_physical: action=menu'),
    ],'6210 cold retry recovery')
    expected[3516]=3
    expected[1604:1609]=bytes.fromhex('00f1100001')
    expected[1610]=0
    if final!=bytes(expected):
        raise ValueError('cold PIN recovery mutated unrelated card bytes or credentials')

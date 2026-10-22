"""Own NHM-3 ordinary PIN1 change, distinct from PUK-based replacement."""
import re
from tools.make_sim_card_profile import make_profile
from tools.noki6250_pin_retry_check import registered_card
from tools.noki6250_pin2_change_check import actions as pin2_actions,FRAMES
from tools.noki6210_puk_check import contacts,check_rgb_frame
from tools.sim_security_trace_check import statuses
from tools.radio_call_lifecycle_common import require_ordered


def actions():
    items=[(second,key) for second,key in pin2_actions() if second<44]
    items += [(44,'Scroll Down'),(46,'Left Softkey / Menu')]
    for start,digits in ((50,'1234'),(58,'2468'),(66,'2468')):
        items += [(start+i,'Keypad '+digit) for i,digit in enumerate(digits)]+[(start+4,'Left Softkey / Menu')]
    return items+[(76,'End'),(78,'Left Softkey / Menu')]


def check(text,initial,final):
    expected=bytearray(registered_card())
    expected[3484:3492]=b'2468\xff\xff\xff\xff'
    if initial!=make_profile(True) or final!=bytes(expected):
        raise ValueError('PIN1 change must install2468 with only expected location changes')
    for ins in (0x20,0x24):
        if statuses(text,ins)!=[(0x9000,3,10)] or statuses(text,ins,2)!=[(0x9000,3,10)]:
            raise ValueError('PIN1 change requires one successful startup VERIFY and CHANGE')
        if re.findall(rf'header cla=a0 ins={ins:02x} p1=(\w\w) p2=(\w\w) p3=(\w\w)',text)!=[('00','01','10' if ins==0x24 else '08')]:
            raise ValueError('PIN1 command selectors differ')
    if re.search(r'header cla=a0 ins=(?:04|26|28|2c|44|dc) ',text):
        raise ValueError('PIN1 change attempted unrelated card operations')
    contacts(text,'6250_pin_change',actions())
    require_ordered(text,[
        ('PIN1',r'header cla=a0 ins=20 p1=00 p2=01 p3=08'),
        ('registered',r'LAPDm Location Updating Accept acknowledged'),
        ('Menu',r'6250_pin_change: key=Left Softkey / Menu state=1 t=20\.'),
        ('confirm',r'6250_pin_change: key=Left Softkey / Menu state=1 t=70\.'),
        ('CHANGE CHV1',r'header cla=a0 ins=24 p1=00 p2=01 p3=10'),
        ('result',r'SIM status ins=24 sw=9000'),
        ('End',r'6250_pin_change: key=End state=1 t=76\.'),
        ('Menu',r'6250_pin_change: key=Left Softkey / Menu state=1 t=78\.'),
        ('complete',r'6250_pin_change: complete=1 firmware_state_forcing=0'),
    ],'6250 PIN1 change')


def check_frames(directory):
    for second in (26,31,34,39):
        check_rgb_frame(directory/f'6250_pin_change_{second}.png',FRAMES[second])
    for second,digest in (
            (44,'621bceac0c909a4e3c2c13dcaa521f9a12ff7b2d2a2da76e8b3512ef6fe787d4'),
            (46,'7b3ce41fbe18a12c87a89ef7b9371fa1752aa424685d97020a95c3ae998c65b1'),
            (54,'62945c4432fa8606c71b4241bd6dd00a53a76c8c924f912d695f3601a428a0a0'),
            (62,'4e806a12415a2eb857ed76b4b28a4fa3d11373368e94a3b1bc22150270bcefca'),
            (70,'41dc6e07aad8c509e4767b9dd3f75f1b3fbca86c9f0927b2473cdfb4ad8e2dea'),
            (76,FRAMES[76]),(78,FRAMES[78])):
        check_rgb_frame(directory/f'6250_pin_change_{second}.png',digest)

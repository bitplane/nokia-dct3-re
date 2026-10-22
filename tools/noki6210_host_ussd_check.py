"""Own NPE-3 host-originated USSD and physical recovery checks."""
import re
from tools.noki6210_puk_check import profile,contacts,check_rgb_frame
from tools.sim_security_trace_check import statuses
from tools.radio_call_lifecycle_common import require_ordered

FRAMES={
    '6210_host_ussd_notice.png':'7e4fc26263a8a9b59133e080439b2652c0233dfbf5f019da480a3049d5bac184',
    '6210_host_ussd_key_26.png':'570411100c4ce1b58be8303a8c912a08b698eaa6328605d110d54615be409eca',
    '6210_host_ussd_key_30.png':'ba918ba6f01fa8f6c28d0e3eb0d7a6e65cd8518a3263e58b21c4542d72b37fcc',
}


def check_frames(directory):
    for name,digest in FRAMES.items():
        check_rgb_frame(directory/name,digest)


def check_notification(text,card):
    if card!=profile(3,True):
        raise ValueError('host USSD altered card data beyond the ordinary registration update')
    if statuses(text,0x20)!=[(0x9000,3,10)] or any(statuses(text,ins) for ins in (4,0x24,0x26,0x28,0x2c,0x44,0xdc)):
        raise ValueError('host USSD changed card credentials, validity or records')
    items=[(8+i,'Keypad '+d) for i,d in enumerate('1234')]
    items += [(12,'Left Softkey / Menu'),(26,'Right Softkey / C'),(30,'Left Softkey / Menu')]
    contacts(text,'6210_host_ussd_input',items)
    admissions=re.findall(r'incoming ussd id=(\d+) result=accepted t=([\d.]+)',text)
    if len(admissions)!=1 or admissions[0][0]!='1' or not 20<=float(admissions[0][1])<24:
        raise ValueError('host USSD must be admitted once after ready and before its capture')
    if re.findall(r'incoming ussd state id=1 epoch=(\d+) phase=(\w+)',text)!=[('1','queued'),('1','delivered')]:
        raise ValueError('host notification epoch or delivery phases differ')
    if text.count('gsm_ss: network_initiated operation=notify ')!=1:
        raise ValueError('host USSD notification is absent or duplicated')
    require_ordered(text,[
        ('PIN1',r'SIM status ins=20 sw=9000'),
        ('registered',r'gsm_call_adapter: network registered=1 arfcn=35\b'),
        ('admitted',r'incoming ussd id=1 result=accepted'),
        ('queued',r'incoming ussd state id=1 epoch=1 phase=queued'),
        ('notification',r'gsm_ss: network_initiated operation=notify transaction=0b invoke=1 dcs=0f'),
        ('REGISTER',r'GSM service downlink kind=32 sapi=0 pd=0b message=3b length=29'),
        ('result',r'GSM service uplink sapi=0 pd=0b message=3a length=8 data=8b3a05a203020101'),
        ('RELEASE COMPLETE',r'GSM service downlink kind=27 sapi=0 pd=0b message=2a length=2'),
        ('RR release',r'LAPDm service Channel Release acknowledged'),
        ('delivered',r'incoming ussd state id=1 epoch=1 phase=delivered'),
        ('Back',r'key=Right Softkey / C state=1 t=26\.'),
        ('Menu',r'key=Left Softkey / Menu state=1 t=30\.'),
        ('complete',r'6210_host_ussd: complete=1 firmware_state_forcing=0'),
    ],'6210 host USSD notification')

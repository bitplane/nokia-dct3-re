"""Own NPE-3 host-originated USSD and physical recovery checks."""
import re
import json
from tools.noki6210_puk_check import profile,contacts,check_rgb_frame
from tools.sim_security_trace_check import statuses
from tools.radio_call_lifecycle_common import require_ordered

FRAMES={
    '6210_host_ussd_notice.png':'7e4fc26263a8a9b59133e080439b2652c0233dfbf5f019da480a3049d5bac184',
    '6210_host_ussd_key_26.png':'570411100c4ce1b58be8303a8c912a08b698eaa6328605d110d54615be409eca',
    '6210_host_ussd_key_30.png':'ba918ba6f01fa8f6c28d0e3eb0d7a6e65cd8518a3263e58b21c4542d72b37fcc',
}
REQUEST_FRAMES={
    '6210_host_ussd_request.png':'a9b28a01923ad30d55f3a2bc92a5c614c5d8768ddc084c4ddd7aa99cd57cbd2c',
    '6210_host_ussd_key_26.png':'56e84d7e18e85c3f9bd87176b27dcaa6417b296f3f7db4adfdd1d95e929d0429',
    '6210_host_ussd_key_30.png':'e897dc6745cd4f3d461886dc3573b7d5dc12296a661db0f0fc01e80d83dcc6a5',
    '6210_host_ussd_key_32.png':'5112fef2eba583654abafd995f36920d6632c0a7b884c08a78809ca7ecda057b',
    '6210_host_ussd_key_36.png':'570411100c4ce1b58be8303a8c912a08b698eaa6328605d110d54615be409eca',
    '6210_host_ussd_key_38.png':'ba918ba6f01fa8f6c28d0e3eb0d7a6e65cd8518a3263e58b21c4542d72b37fcc',
}


def check_frames(directory,request=False):
    for name,digest in (REQUEST_FRAMES if request else FRAMES).items():
        check_rgb_frame(directory/name,digest)


def check_card_authentication(text,card):
    if card!=profile(3,True):
        raise ValueError('host USSD altered card data beyond the ordinary registration update')
    if statuses(text,0x20)!=[(0x9000,3,10)] or any(statuses(text,ins) for ins in (4,0x24,0x26,0x28,0x2c,0x44,0xdc)):
        raise ValueError('host USSD changed card credentials, validity or records')
    if re.findall(r'header cla=a0 ins=20 p1=00 p2=(\w\w) p3=08',text)!=['01']:
        raise ValueError('host USSD must authenticate PIN1 exactly once')


def check_admission(text,terminal,restored=False):
    admissions=re.findall(r'incoming ussd id=(\d+) result=accepted t=([\d.]+)',text)
    if len(admissions)!=1 or admissions[0][0]!='1' or not 20<=float(admissions[0][1])<24:
        raise ValueError('host USSD must be admitted once after ready and before its capture')
    phases=[('1','queued'),('2','queued'),('2',terminal)] if restored else [('1','queued'),('1',terminal)]
    if re.findall(r'incoming ussd state id=1 epoch=(\d+) phase=(\w+)',text)!=phases:
        raise ValueError('host USSD epoch or terminal phases differ')


def check_notification(text,card):
    check_card_authentication(text,card)
    items=[(8+i,'Keypad '+d) for i,d in enumerate('1234')]
    items += [(12,'Left Softkey / Menu'),(26,'Right Softkey / C'),(30,'Left Softkey / Menu')]
    contacts(text,'6210_host_ussd_input',items)
    check_admission(text,'delivered')
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


def check_request(text,card,console,restored=False):
    check_card_authentication(text,card)
    check_admission(text,'answered',restored=restored)
    epoch=2 if restored else 1
    if restored:
        require_ordered(text,[
            ('Answer',r'key=Left Softkey / Menu state=1 t=26\.'),
            ('saved',r'6210_ussd_restore: saved t=28\.000000000'),
            ('restored',r'6210_ussd_restore: architecture=pass'),
            ('republished',r'incoming ussd state id=1 epoch=2 phase=queued'),
            ('digit',r'key=Keypad 2 state=1 t=30\.'),
        ],'6210 pending USSD restoration')
        if text.count('6210_ussd_restore: architecture=pass')!=1:
            raise ValueError('pending USSD requires exactly one verified architectural restore')
    items=[(8+i,'Keypad '+d) for i,d in enumerate('1234')]
    items += [(12,'Left Softkey / Menu'),(26,'Left Softkey / Menu'),(30,'Keypad 2'),
              (32,'Left Softkey / Menu'),(36,'End'),(38,'Left Softkey / Menu')]
    contacts(text,'6210_host_ussd_input',items)
    if text.count('gsm_ss: network_initiated operation=request ')!=1:
        raise ValueError('host USSD request is absent or duplicated')
    received=[json.loads(line.split(': ',1)[1]) for line in console.splitlines() if line.startswith('host_ussd_reply: ')]
    expected={'type':'incoming_ussd_response','epoch':epoch,'request_id':1,'dcs':15,'data':'32','empty':False}
    if (received!=[expected] or type(received[0].get('empty')) is not bool or
            any(type(received[0].get(key)) is not int for key in ('epoch','request_id','dcs'))):
        raise ValueError('host did not receive the exact correlated packed USSD reply')
    replies=re.findall(r'incoming ussd reply id=(\d+) epoch=(\d+) dcs=(\w+) data=(\w*) empty=(\d)',text)
    if replies!=[('1',str(epoch),'0f','32','0')]:
        raise ValueError('adapter USSD reply differs or is duplicated')
    require_ordered(text,[
        ('PIN1',r'SIM status ins=20 sw=9000'),
        ('registered',r'gsm_call_adapter: network registered=1 arfcn=35\b'),
        ('admitted',r'incoming ussd id=1 result=accepted'),
        ('queued',r'incoming ussd state id=1 epoch=1 phase=queued'),
        ('request',r'gsm_ss: network_initiated operation=request transaction=0b invoke=1 dcs=0f'),
        ('REGISTER',r'GSM service downlink kind=32 sapi=0 pd=0b message=3b length=26'),
        ('Answer',r'key=Left Softkey / Menu state=1 t=26\.'),
        ('reply digit',r'key=Keypad 2 state=1 t=30\.'),
        ('Send',r'key=Left Softkey / Menu state=1 t=32\.'),
        ('ReturnResult',r'GSM service uplink sapi=0 pd=0b message=3a length=21 data=8b3a12a210020101300b02013c300604010f040132'),
        ('RELEASE COMPLETE',r'GSM service downlink kind=27 sapi=0 pd=0b message=2a length=2'),
        ('RR release',r'LAPDm service Channel Release acknowledged'),
        ('host reply',rf'incoming ussd reply id=1 epoch={epoch} dcs=0f data=32 empty=0'),
        ('answered',rf'incoming ussd state id=1 epoch={epoch} phase=answered'),
        ('End',r'key=End state=1 t=36\.'),
        ('Menu',r'key=Left Softkey / Menu state=1 t=38\.'),
        ('complete',r'6210_host_ussd: complete=1 firmware_state_forcing=0'),
    ],'6210 host USSD physical reply')

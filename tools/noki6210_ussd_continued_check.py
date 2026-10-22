"""Own NPE-3 physical continued laboratory USSD contract."""
from pathlib import Path
from tools.noki6210_host_ussd_check import check_card_authentication
from tools.noki6210_puk_check import contacts,check_rgb_frame
from tools.radio_ussd_trace_check import verify
from tools.radio_call_lifecycle_common import require_ordered


FRAMES={
    31:'4a4b97798911bc347c6d597024111f63c9f9d66237535c1b9217ded73a8528b5',
    36:'56e84d7e18e85c3f9bd87176b27dcaa6417b296f3f7db4adfdd1d95e929d0429',
    40:'e897dc6745cd4f3d461886dc3573b7d5dc12296a661db0f0fc01e80d83dcc6a5',
    42:'c252d27cc7d919b72eb9b07aefce56c4f9bb16ded669b1937cfe9871ea3e53fb',
    46:'570411100c4ce1b58be8303a8c912a08b698eaa6328605d110d54615be409eca',
    48:'ba918ba6f01fa8f6c28d0e3eb0d7a6e65cd8518a3263e58b21c4542d72b37fcc',
}


def check_frames(directory):
    for second,digest in FRAMES.items():
        check_rgb_frame(Path(directory)/f'6210_host_ussd_key_{second}.png',digest)


def check(text,card):
    check_card_authentication(text,card)
    items=[(8+i,'Keypad '+d) for i,d in enumerate('1234')]
    items += [(12,'Left Softkey / Menu'),(26,'Keypad *'),(27,'Keypad 1'),
              (28,'Keypad 2'),(29,'Keypad 3'),(30,'Keypad #'),(31,'Send'),
              (36,'Left Softkey / Menu'),(40,'Keypad 2'),
              (42,'Left Softkey / Menu'),(46,'End'),(48,'Left Softkey / Menu')]
    contacts(text,'6210_host_ussd_input',items)
    verify(text,Path('.'),'continued-reply',require_frame=False)
    if text.count('gsm_ss: request=ussd ')!=1 or text.count('gsm_ss: continued_result ')!=1:
        raise ValueError('continued USSD request/result is absent or duplicated')
    require_ordered(text,[
        ('registered',r'LAPDm Location Updating Accept acknowledged'),
        ('dial Send',r'key=Send state=1 t=31\.'),
        ('request',r'gsm_ss: request=ussd '),
        ('Answer',r'key=Left Softkey / Menu state=1 t=36\.'),
        ('digit',r'key=Keypad 2 state=1 t=40\.'),
        ('reply Send',r'key=Left Softkey / Menu state=1 t=42\.'),
        ('result',r'gsm_ss: continued_result transaction=1b invoke=2 dcs=0f packed_length=1 empty=0'),
        ('RR release',r'LAPDm service Channel Release acknowledged'),
        ('End',r'key=End state=1 t=46\.'),
        ('Menu',r'key=Left Softkey / Menu state=1 t=48\.'),
        ('complete',r'6210_host_ussd: complete=1 firmware_state_forcing=0'),
    ],'6210 continued USSD')

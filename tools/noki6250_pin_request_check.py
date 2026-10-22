"""Own NHM-3 PIN-request contacts, CHV selectors and complete card transitions."""
import re
from tools.make_sim_card_profile import make_profile
from tools.noki6250_pin_retry_check import registered_card
from tools.noki6250_pin2_change_check import actions as change_actions,FRAMES
from tools.noki6210_puk_check import contacts,check_rgb_frame
from tools.sim_security_trace_check import statuses,ENABLED_OFFSET
from tools.radio_call_lifecycle_common import require_ordered


def actions(*,enable=False,cold=False):
    if cold:
        return [(14+i,'Keypad '+digit) for i,digit in enumerate('1234')]+[(18,'Left Softkey / Menu'),(28,'Left Softkey / Menu')]
    items=[(second,key) for second,key in change_actions() if second<36 and (not enable or second>=20)]
    items += [(36,'Left Softkey / Menu')]
    items += [(38+i,'Keypad '+digit) for i,digit in enumerate('1234')]
    return items+[(42,'Left Softkey / Menu'),(44,'Scroll Up' if enable else 'Scroll Down'),
                  (46,'Left Softkey / Menu'),(50,'End'),(52,'Left Softkey / Menu')]


def check(text,initial,final,*,enable=False,cold=False):
    if enable and cold:
        raise ValueError('cold authentication and setting enable are distinct phases')
    expected_initial=bytearray(registered_card() if enable or cold else make_profile(True))
    if enable:expected_initial[ENABLED_OFFSET]=0
    expected_final=bytearray(registered_card())
    expected_final[ENABLED_OFFSET]=int(enable or cold)
    if initial!=bytes(expected_initial) or final!=bytes(expected_final):
        raise ValueError('PIN-request phase changed unrelated card data or lost its inherited state')
    expected_verify=[] if enable else [(0x9000,3,10)]
    if (statuses(text,0x20)!=expected_verify or statuses(text,0x20,2)!=expected_verify or
            re.findall(r'header cla=a0 ins=20 p1=00 p2=(\w\w) p3=08',text)!=([] if enable else ['01'])):
        raise ValueError('PIN-request boot authentication differs')
    instruction=0x28 if enable else 0x26
    for ins in (0x26,0x28):
        wanted=[] if cold or ins!=instruction else [(0x9000,3,10)]
        if statuses(text,ins)!=wanted or statuses(text,ins,2)!=wanted:
            raise ValueError('PIN-request enable/disable result differs')
    headers=re.findall(r'header cla=a0 ins=(26|28) p1=(\w\w) p2=(\w\w) p3=(\w\w)',text)
    if headers!=([] if cold else [(f'{instruction:02x}','00','01','08')]):
        raise ValueError('PIN-request command selectors differ')
    if not cold and re.findall(rf'SIM status ins={instruction:02x} sw=9000 chv=3/3 puk=10/10 enabled=(\d+)',text)!=[str(int(enable))]:
        raise ValueError('PIN-request response reports the wrong enabled state')
    if re.search(r'header cla=a0 ins=(?:04|24|2c|44|dc) ',text):
        raise ValueError('PIN-request phase attempted unrelated credentials or record changes')
    contacts(text,'6250_pin_request',actions(enable=enable,cold=cold))
    order=[] if enable else [('PIN1',r'header cla=a0 ins=20 p1=00 p2=01 p3=08')]
    order += [('registered',r'LAPDm Location Updating Accept acknowledged'),
              ('Menu',rf'6250_pin_request: key=Left Softkey / Menu state=1 t={28 if cold else 20}\.')]
    if not cold:
        order += [('setting',r'6250_pin_request: key=Left Softkey / Menu state=1 t=36\.'),
                  ('confirm',r'6250_pin_request: key=Left Softkey / Menu state=1 t=46\.'),
                  ('command',rf'header cla=a0 ins={instruction:02x} p1=00 p2=01 p3=08'),
                  ('result',rf'SIM status ins={instruction:02x} sw=9000'),
                  ('End',r'6250_pin_request: key=End state=1 t=50\.'),
                  ('Menu',r'6250_pin_request: key=Left Softkey / Menu state=1 t=52\.')]
    order += [('complete',r'6250_pin_request: complete=1 firmware_state_forcing=0')]
    require_ordered(text,order,'6250 PIN request')


def check_frames(directory,*,enable=False,cold=False):
    startup='9f39b5343904e4e1ae99b2afec9ec948c5aa62c27818c10834114b5486424b38'
    check_rgb_frame(directory/'6250_pin_request_boot.png',
        'bb528795a76fc5e4cf6fc34dcd42a039cd21dbb5b86c7eaf8d3e779b572143d5' if not enable else
        'b917509b5f70a8add9573f2b78a4afb775e3d248d02af2f642afb8f189d0bd4e')
    check_rgb_frame(directory/'6250_pin_request_startup.png',FRAMES[76] if enable else startup)
    if cold:
        check_rgb_frame(directory/'6250_pin_request_28.png',FRAMES[78])
        return
    for second in (26,31,34):
        digest=('e90348d1cd348ec11590a0eba30b35e99c4f52088bb252b201c8969e0f6ed369'
                if enable and second==34 else FRAMES[second])
        check_rgb_frame(directory/f'6250_pin_request_{second}.png',digest)
    on='797e5f11aec3001d8999b9e56e0be104d3040eb68c789f2e858558b14bfc42a4'
    off='2060203a6888b4ae5f498f445cfd2cdbdb4cb226d87d9904eecc2bbec0d04a33'
    for second,digest in (
            (36,'92c20eaa80c910bc82701b9400de5e737c806c6c65c60d6bd75781c712fa00e6'),
            (42,off if enable else on),(44,on if enable else off),
            (46,'68213026f591d36b653dd652371f16db41c08b7c40f4b0bc19885352ae45dc4e' if enable else
                '74639c82713fe62e18e5d8560583fde8c92fdfd17e5e55140cd0285c7959a521'),
            (50,FRAMES[76]),(52,FRAMES[78])):
        check_rgb_frame(directory/f'6250_pin_request_{second}.png',digest)

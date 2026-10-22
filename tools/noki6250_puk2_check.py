"""Own NHM-3 PUK2 recovery after physically exhausted PIN2."""
import re
from tools.noki6250_pin_retry_check import registered_card
from tools.noki6250_pin2_change_check import actions,FRAMES as CHANGE_FRAMES
from tools.noki6210_puk_check import contacts,check_rgb_frame
from tools.sim_security_trace_check import statuses
from tools.radio_call_lifecycle_common import require_ordered

FRAMES={**{second:CHANGE_FRAMES[second] for second in (26,31,34,39,45)},
        47:'e71767f126d0131b950ce770072638de16a2ca8979efa4df184bda8d09157bd8',
        58:CHANGE_FRAMES[54],66:CHANGE_FRAMES[62],
        74:'b358d01988adefc3dfc51a5352b485d1802d16f9e5f600e047c723a43cf514a6',
        80:CHANGE_FRAMES[76],82:CHANGE_FRAMES[78]}


def check_frames(directory,*,rejected=False,puk_attempts=10):
    for second,digest in FRAMES.items():
        if rejected and second==74:
            digest=('6357f9b5bc04027166bc3529fa4abd4ca429af4c953db90b97e6697f44984152'
                    if puk_attempts==1 else
                    'c1371c4cdca6b0f72c9c1dcc6e602de981330f67719714bc5b124e1a3b4fcfcd')
        if rejected and puk_attempts==1 and second in (80,82):
            digest='6e9133cb23ceaf7021771d33e2e49fa09dcf41c76ebac0f18c767078ae9e0d9f'
        check_rgb_frame(directory/f'6250_pin2_key_{second}.png',digest)


def check(text,initial,final,*,rejected=False,puk_attempts=10):
    if type(puk_attempts) is not int or not 1<=puk_attempts<=10:
        raise ValueError('PUK2 transaction requires one to ten initial attempts')
    expected=bytearray(registered_card())
    expected[3492:3500]=b'8765\xff\xff\xff\xff'
    expected[3517]=0
    expected[3519]=puk_attempts
    if initial!=bytes(expected):
        raise ValueError('PUK2 recovery requires the complete physically exhausted predecessor card')
    if rejected:
        expected[3519]-=1
    else:
        expected[3492:3500]=b'2468\xff\xff\xff\xff'
        expected[3517]=3
        expected[3519]=10
    if final!=bytes(expected):
        raise ValueError('PUK2 recovery changed unrelated card data or failed to install2468')
    result=(0x9840 if puk_attempts==1 else 0x9804) if rejected else 0x9000
    verifies=[(0x9000,3,10)]*(1 if rejected else 2)
    pin2_verifies=[(0x9000,0,puk_attempts)]+([] if rejected else [(0x9000,3,10)])
    if (statuses(text,0x20)!=verifies or statuses(text,0x20,2)!=pin2_verifies or
            statuses(text,0x2c)!=[(result,3,10)] or
            statuses(text,0x2c,2)!=[(result,0 if rejected else 3,puk_attempts-1 if rejected else 10)]):
        raise ValueError('PUK2 recovery requires PIN1, one UNBLOCK and new-PIN2 verification')
    if (re.findall(r'header cla=a0 ins=20 p1=00 p2=(\w\w) p3=08',text)!=(['01'] if rejected else ['01','02']) or
            re.findall(r'header cla=a0 ins=2c p1=00 p2=(\w\w) p3=10',text)!=['02']):
        raise ValueError('PUK2 selectors differ')
    if any(statuses(text,ins) for ins in (4,0x24,0x26,0x28,0x44,0xdc)):
        raise ValueError('PUK2 recovery unexpectedly changed other card state')
    items=[(second,key) for second,key in actions() if second<50]
    for start,digits in ((50,'99999999' if rejected else '87654321'),(62,'2468'),(70,'2468')):
        items += [(start+i,'Keypad '+digit) for i,digit in enumerate(digits)]+[(start+len(digits),'Left Softkey / Menu')]
    items += [(80,'End'),(82,'Left Softkey / Menu')]
    contacts(text,'6250_pin2_input',items)
    order=[
        ('PIN1',r'header cla=a0 ins=20 p1=00 p2=01 p3=08'),
        ('registered',r'LAPDm Location Updating Accept acknowledged'),
        ('confirm',r'6250_pin2_input: key=Left Softkey / Menu state=1 t=74\.'),
        ('UNBLOCK CHV2',r'header cla=a0 ins=2c p1=00 p2=02 p3=10'),
        ('result',rf'SIM status ins=2c sw={result:04x}'),
    ]
    if not rejected:
        order += [('new PIN2 VERIFY',r'header cla=a0 ins=20 p1=00 p2=02 p3=08'),
                  ('verified',r'SIM status ins=20 sw=9000')]
    order += [
        ('End',r'6250_pin2_input: key=End state=1 t=80\.'),
        ('Menu',r'6250_pin2_input: key=Left Softkey / Menu state=1 t=82\.'),
        ('complete',r'6250_pin2_input: complete=1 firmware_state_forcing=0'),
    ]
    require_ordered(text,order,'6250 PUK2 recovery')


def check_permanent(text,initial,final):
    expected=bytearray(registered_card())
    expected[3492:3500]=b'8765\xff\xff\xff\xff'
    expected[3517]=expected[3519]=0
    if initial!=bytes(expected) or final!=initial:
        raise ValueError('permanent PUK2 cold boot must preserve the complete exhausted card')
    if (statuses(text,0x20)!=[(0x9000,3,10)] or
            statuses(text,0x20,2)!=[(0x9000,0,0)] or
            re.findall(r'header cla=a0 ins=20 p1=00 p2=(\w\w) p3=08',text)!=['01']):
        raise ValueError('permanent PUK2 cold boot requires independent PIN1 authentication')
    if re.search(r'header cla=a0 ins=(?:04|24|26|28|2c|44|dc) ',text):
        raise ValueError('permanent PUK2 menu unexpectedly attempted credentials or changed records')
    items=[(second,key) for second,key in actions() if second<50]
    items += [(50,'End'),(52,'Left Softkey / Menu')]
    contacts(text,'6250_pin2_input',items)
    require_ordered(text,[
        ('PIN1',r'header cla=a0 ins=20 p1=00 p2=01 p3=08'),
        ('registered',r'LAPDm Location Updating Accept acknowledged'),
        ('PIN2 setting',r'6250_pin2_input: key=Left Softkey / Menu state=1 t=47\.'),
        ('End',r'6250_pin2_input: key=End state=1 t=50\.'),
        ('Menu',r'6250_pin2_input: key=Left Softkey / Menu state=1 t=52\.'),
        ('complete',r'6250_pin2_input: complete=1 firmware_state_forcing=0'),
    ],'6250 permanent PUK2 cold menu')


def check_permanent_frames(directory):
    for second in (26,31,34,39,45):
        check_rgb_frame(directory/f'6250_pin2_key_{second}.png',FRAMES[second])
    # The cold PIN2 selection is an unlabelled notice, not a qualified editor.
    for second,digest in ((47,'94dc65fd0c276b263ed5f95d7c721922b1ae10fb4a9ad6c074f8b1b3e5b48add'),
                          (50,CHANGE_FRAMES[76]),(52,CHANGE_FRAMES[78])):
        check_rgb_frame(directory/f'6250_pin2_key_{second}.png',digest)

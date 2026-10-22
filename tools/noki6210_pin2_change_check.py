"""6210 physical PIN2 change without modifying PIN1 or unrelated card records."""
import re
from tools.noki6210_puk_check import profile,contacts,check_rgb_frame
from tools.sim_security_trace_check import statuses
from tools.radio_call_lifecycle_common import require_ordered

FRAMES={
    '6210_pin2_key_42.png':'37a1412d064a412b4cf999958a499be95a6f43050b3e8cb05532756ec1206ccc',
    '6210_pin2_key_47.png':'edbaeb28d8bbd3faba13de0a6655978463f25470aece4de26e66fa668ea240f7',
    '6210_pin2_key_54.png':'61714b4807d53f261a32a5bf50701212b0113b0a82cdcd4e4f6bb0401b8c5e07',
    '6210_pin2_key_62.png':'36429ed5f0ebac7a5af828695a0f7539affe1dfa938bd77531524f08c49b89fb',
    '6210_pin2_key_70.png':'e0c53bcdff6dbb772704f3c48057211a891cd2869d35cc7923d4d5d210e94630',
    # This Messages icon is at a different phase from the earlier security gate.
    '6210_security_then_menu.png':'5a3c0ca03c73478aa8c645a8a263e5edb04cc833639c893dc638ac18c300ff12',
}


def check_frames(directory,rejected=False,blocked=False):
    for name,digest in FRAMES.items():
        if rejected and name=='6210_pin2_key_70.png':
            digest='c1371c4cdca6b0f72c9c1dcc6e602de981330f67719714bc5b124e1a3b4fcfcd'
        if blocked and name=='6210_pin2_key_70.png':
            digest='f0bcd18b7cdbc4a9f63d55c4046dfc695dab6944776f99a1f983993d6156ad2a'
        check_rgb_frame(directory/name,digest)


def settings_contacts():
    items=[(7+i,'Keypad '+str(i)) for i in range(1,5)]+[(12,'Left Softkey / Menu')]
    items += [(20,'Left Softkey / Menu')]+[(22+i,'Scroll Down') for i in range(3)]+[(26,'Left Softkey / Menu')]
    items += [(28+i,'Scroll Down') for i in range(5)]+[(34,'Left Softkey / Menu')]
    items += [(36+i,'Scroll Down') for i in range(4)]+[(42,'Left Softkey / Menu')]
    items += [(44+i,'Scroll Down') for i in range(2)]+[(47,'Left Softkey / Menu')]
    return items


def check(text,card,old_pin='5678',new_pin='8765',rejected=False,remaining=2):
    if remaining not in (0,1,2):
        raise ValueError('rejected PIN2 remaining attempts must be 0..2')
    result=(0x9804 if remaining else 0x9840) if rejected else 0x9000
    if statuses(text,0x20)!=[(0x9000,3,10)] or statuses(text,0x24,2)!=[(result,remaining if rejected else 3,10)]:
        raise ValueError('6210 must authenticate PIN1 once and change PIN2 once')
    items=settings_contacts()
    for start,digits in ((50,old_pin),(58,new_pin),(66,new_pin)):
        items.extend((start+i,'Keypad '+d) for i,d in enumerate(digits))
        items.append((start+4,'Left Softkey / Menu'))
    items += [(76,'End'),(78,'Left Softkey / Menu')]
    contacts(text,'6210_pin2_input',items)
    require_ordered(text,[
        ('PIN1',r'SIM status ins=20 sw=9000'),
        ('registration',r'LAPDm Location Updating Accept acknowledged nr=1'),
        ('confirm PIN2',r'key=Left Softkey / Menu state=1 t=70\.'),
        ('CHANGE CHV2',r'header cla=a0 ins=24 p1=00 p2=02 p3=10'),
        ('result',rf'SIM status ins=24 sw={result:04x}'),
        ('Menu',r'key=Left Softkey / Menu state=1 t=78\.'),
        ('complete',r'6210_pin2_input: complete=1 firmware_state_forcing=0'),
    ],'6210 PIN2 change')
    expected=bytearray(profile(3,True))
    if rejected:
        expected[3517]=remaining
    else:
        expected[3492:3500]=new_pin.encode('ascii').ljust(8,b'\xff')
    if card!=bytes(expected):
        raise ValueError('PIN2 change altered PIN1 or unrelated card data')


def puk2_contacts(rejected=False):
    items=settings_contacts()
    for start,digits in ((50,'99999999' if rejected else '87654321'),(62,'2468'),(70,'2468')):
        items.extend((start+i,'Keypad '+d) for i,d in enumerate(digits))
        items.append((start+len(digits),'Left Softkey / Menu'))
    return items+[(80,'End'),(82,'Left Softkey / Menu')]


def check_puk2(text,initial,card,rejected=False,puk_attempts=10):
    if puk_attempts not in range(1,11):
        raise ValueError('PUK2 predecessor allowance must be 1..10')
    expected=bytearray(profile(3,True))
    expected[3517]=0
    expected[3519]=puk_attempts
    if initial!=bytes(expected):
        raise ValueError('PUK2 input is not the exact physically exhausted PIN2 card')
    result=(0x9804 if puk_attempts>1 else 0x9840) if rejected else 0x9000
    verify=[(0x9000,3,10)]*(1 if rejected else 2)
    if (statuses(text,0x20)!=verify or statuses(text,0x24) or
            statuses(text,0x2c,2)!=[(result,0 if rejected else 3,puk_attempts-1 if rejected else 10)]):
        raise ValueError('PUK2 recovery must authenticate PIN1 and unblock CHV2 exactly once')
    if rejected and 'header cla=a0 ins=20 p1=00 p2=02 p3=08' in text:
        raise ValueError('rejected PUK2 must not VERIFY a replacement PIN2')
    contacts(text,'6210_pin2_input',puk2_contacts(rejected))
    order=[
        ('VERIFY PIN1',r'header cla=a0 ins=20 p1=00 p2=01 p3=08'),
        ('PIN1',r'SIM status ins=20 sw=9000'),
        ('registration',r'LAPDm Location Updating Accept acknowledged nr=1'),
        ('confirmation',r'key=Left Softkey / Menu state=1 t=74\.'),
        ('UNBLOCK CHV2',r'header cla=a0 ins=2c p1=00 p2=02 p3=10'),
        ('result',rf'SIM status ins=2c sw={result:04x}'),
    ]
    if not rejected:
        order += [('VERIFY new PIN2',r'header cla=a0 ins=20 p1=00 p2=02 p3=08'),
                  ('verified',r'SIM status ins=20 sw=9000')]
    order += [
        ('Menu',r'key=Left Softkey / Menu state=1 t=82\.'),
        ('complete',r'6210_pin2_input: complete=1 firmware_state_forcing=0'),
    ]
    require_ordered(text,order,'6210 physical PUK2 recovery')
    if rejected:
        expected[3519]=puk_attempts-1
    else:
        expected[3517]=3
        expected[3519]=10
        expected[3492:3500]=b'2468\xff\xff\xff\xff'
    if card!=bytes(expected):
        raise ValueError('PUK2 recovery changed unrelated card data')


def check_puk2_frames(directory,rejected=False,permanent=False):
    frames={
        '6210_pin2_key_42.png':FRAMES['6210_pin2_key_42.png'],
        '6210_pin2_key_47.png':'e71767f126d0131b950ce770072638de16a2ca8979efa4df184bda8d09157bd8',
        '6210_pin2_key_58.png':FRAMES['6210_pin2_key_54.png'],
        '6210_pin2_key_66.png':FRAMES['6210_pin2_key_62.png'],
        '6210_pin2_key_74.png':'b358d01988adefc3dfc51a5352b485d1802d16f9e5f600e047c723a43cf514a6',
        '6210_security_then_menu.png':FRAMES['6210_security_then_menu.png'],
    }
    if rejected:
        frames['6210_pin2_key_74.png']='c1371c4cdca6b0f72c9c1dcc6e602de981330f67719714bc5b124e1a3b4fcfcd'
    if permanent:
        frames['6210_pin2_key_74.png']='6357f9b5bc04027166bc3529fa4abd4ca429af4c953db90b97e6697f44984152'
    for name,digest in frames.items():
        check_rgb_frame(directory/name,digest)


def check_permanent_puk2(text,initial,card):
    expected=bytearray(profile(3,True))
    expected[3517]=expected[3519]=0
    if initial!=bytes(expected) or card!=initial:
        raise ValueError('permanent PUK2 input control changed the exhausted card')
    if (statuses(text,0x20)!=[(0x9000,3,10)] or
            any(statuses(text,ins) for ins in (0x24,0x26,0x28,0x2c)) or
            'header cla=a0 ins=20 p1=00 p2=02' in text or
            'header cla=a0 ins=2c' in text):
        raise ValueError('permanent PUK2 control entered a credential transaction')
    if (re.search(r'GSM service (?:establish|uplink) sapi=\d+ pd=(?:03|09) ',text) or
            re.search(r'GSM service (?:establish|uplink) sapi=\d+ pd=05 message=24 ',text)):
        raise ValueError('permanent PUK2 input control started call or SMS service')
    contacts(text,'6210_pin2_input',puk2_contacts())
    require_ordered(text,[
        ('PIN1',r'SIM status ins=20 sw=9000'),
        ('registration',r'LAPDm Location Updating Accept acknowledged nr=1'),
        ('Settings',r'key=Left Softkey / Menu state=1 t=47\.'),
        ('End',r'key=End state=1 t=80\.'),
        ('Menu',r'key=Left Softkey / Menu state=1 t=82\.'),
        ('complete',r'6210_pin2_input: complete=1 firmware_state_forcing=0'),
    ],'6210 permanent PUK2 input control')


def check_permanent_puk2_frames(directory):
    frames={
        '6210_pin2_key_42.png':FRAMES['6210_pin2_key_42.png'],
        '6210_pin2_key_47.png':'6357f9b5bc04027166bc3529fa4abd4ca429af4c953db90b97e6697f44984152',
        '6210_pin2_key_58.png':'2d344a4cb647173525e721e94e08de1b49aa83454937036947cfd1ef0b8dad72',
        '6210_pin2_key_66.png':'d9d0c8fceb5a66f15e4b28cd9ac78b0d8bad769f7bdb4b580ce1661e74957303',
        '6210_pin2_key_74.png':'55b5c9d6cbd20f2dc9b19decc5f4eebefa7998bd90eefcf80ffddfccdd37832b',
        '6210_security_then_menu.png':FRAMES['6210_security_then_menu.png'],
    }
    for name,digest in frames.items():
        check_rgb_frame(directory/name,digest)

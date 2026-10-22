"""Own NHM-3 physical Settings/PIN2 change, not inherited menu positions."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.noki6250_pin_retry_check import registered_card
from tools.noki6210_puk_check import contacts,check_rgb_frame
from tools.sim_security_trace_check import statuses
from tools.radio_call_lifecycle_common import require_ordered
from tools.run_noki6250_pin_retry_cold import read_evidence
from tools.noki6250_coherent_registration_check import verify as registration
from tools.run_sip_handset_gate import verify_handset_health

FRAMES={
    26:'38a4d1113ebe06085ae023c6047fb3b9d5fc9c8e231bba72c66aa0f5b3664cb3',
    31:'e226221f547a2acc95d0f1b1b80d3e5559badc779ae3117876cdc567c60d2eda',
    34:'c32821f591ac508b1d1153a10658736dbebb763eb19a4649663c2fd6a1d4fbc1',
    39:'a1127cc4455f7882e8f2705f1c6871514aba1d9cec67bf2fc79ff93331bd9d44',
    45:'c4a4e2f4eff1819bf8fced20119997f9508db516096fcc7596749090b52996b7',
    47:'edbaeb28d8bbd3faba13de0a6655978463f25470aece4de26e66fa668ea240f7',
    54:'61714b4807d53f261a32a5bf50701212b0113b0a82cdcd4e4f6bb0401b8c5e07',
    62:'36429ed5f0ebac7a5af828695a0f7539affe1dfa938bd77531524f08c49b89fb',
    70:'e0c53bcdff6dbb772704f3c48057211a891cd2869d35cc7923d4d5d210e94630',
    76:'0b52e9f7b6dd5e60916d48d527423c857f49cee2654ebd9ca83fc2664efe1f35',
    78:'2c2a6aa19024c3f6bf1d3797a8f7c38edb950243ec17fe2241793a5ef029ac24',
}


def actions(old='5678',new='8765'):
    items=[(8+i,'Keypad '+digit) for i,digit in enumerate('1234')]
    items += [(12,'Left Softkey / Menu'),(20,'Left Softkey / Menu')]
    for times,select in ((range(22,25),26),(range(28,32),34),(range(36,40),42),(range(44,46),47)):
        items += [(second,'Scroll Down') for second in times]+[(select,'Left Softkey / Menu')]
    for start,digits in ((50,old),(58,new),(66,new)):
        items += [(start+i,'Keypad '+digit) for i,digit in enumerate(digits)]+[(start+4,'Left Softkey / Menu')]
    return items+[(76,'End'),(78,'Left Softkey / Menu')]


def check(text,card,*,old='5678',new='8765',rejected=False,stored='5678',initial_retries=3):
    if any(len(value)!=4 or not value.isascii() or not value.isdigit() for value in (old,new,stored)):
        raise ValueError('qualified PIN2 changes require four ASCII digits')
    if initial_retries not in (1,2,3):
        raise ValueError('qualified PIN2 changes require one to three initial attempts')
    expected=bytearray(registered_card())
    expected[3492:3500]=(stored if rejected else new).encode('ascii')+b'\xff'*4
    expected[3517]=initial_retries-1 if rejected else 3
    if card!=bytes(expected):
        raise ValueError('PIN2 change altered unrelated card contents or did not install the expected value')
    result=(0x9840 if initial_retries==1 else 0x9804) if rejected else 0x9000
    if (statuses(text,0x20)!=[(0x9000,3,10)] or
            statuses(text,0x20,2)!=[(0x9000,initial_retries,10)] or
            statuses(text,0x24)!=[(result,3,10)] or
            statuses(text,0x24,2)!=[(result,initial_retries-1 if rejected else 3,10)]):
        raise ValueError('NHM-3 requires successful PIN1 authentication and one CHANGE CHV2')
    if re.findall(r'header cla=a0 ins=20 p1=00 p2=(\w\w) p3=08',text)!=['01'] or \
            re.findall(r'header cla=a0 ins=24 p1=00 p2=(\w\w) p3=10',text)!=['02']:
        raise ValueError('PIN2 change addressed incorrect CHV selectors')
    if any(statuses(text,ins) for ins in (4,0x26,0x28,0x2c,0x44,0xdc)):
        raise ValueError('PIN2 change unexpectedly changed card validity, mode or records')
    contacts(text,'6250_pin2_input',actions(old,new))
    require_ordered(text,[
        ('PIN1',r'SIM status ins=20 sw=9000'),
        ('registered',r'LAPDm Location Updating Accept acknowledged'),
        ('Menu',r'6250_pin2_input: key=Left Softkey / Menu state=1 t=20\.'),
        ('confirmation',r'6250_pin2_input: key=Left Softkey / Menu state=1 t=70\.'),
        ('CHANGE CHV2',r'header cla=a0 ins=24 p1=00 p2=02 p3=10'),
        ('result',rf'SIM status ins=24 sw={result:04x}'),
        ('End',r'6250_pin2_input: key=End state=1 t=76\.'),
        ('Menu again',r'6250_pin2_input: key=Left Softkey / Menu state=1 t=78\.'),
        ('complete',r'6250_pin2_input: complete=1 firmware_state_forcing=0'),
    ],'6250 PIN2 change')


def check_frames(directory,*,rejected=False,initial_retries=3):
    for second,digest in FRAMES.items():
        if rejected and second==70:
            digest=('f0bcd18b7cdbc4a9f63d55c4046dfc695dab6944776f99a1f983993d6156ad2a'
                    if initial_retries==1 else
                    'c1371c4cdca6b0f72c9c1dcc6e602de981330f67719714bc5b124e1a3b4fcfcd')
        check_rgb_frame(directory/f'6250_pin2_key_{second}.png',digest)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run',type=Path)
    args=parser.parse_args()
    try:
        verify_handset_health((args.run/'console.log').read_text(errors='replace'))
        text=read_evidence(args.run/'error.log')
        card=(args.run/'nvram/nhm3hle/sim_card').read_bytes()
        check(text,card)
        registration(text,card,require_host=False)
        check_frames(args.run/'snap')
    except (OSError,ValueError,RuntimeError) as error:
        parser.exit(1,f'6250 PIN2 change FAIL: {error}\n')
    print('6250 physical PIN2 change PASS; research HLE only')


if __name__=='__main__':main()

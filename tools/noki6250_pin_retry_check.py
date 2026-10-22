"""Own NHM-3 physical incorrect-PIN recovery and exact card contract."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.noki6250_coherent_registration_check import verify as registration
from tools.noki6210_puk_check import contacts,check_rgb_frame
from tools.make_sim_card_profile import make_profile
from tools.sim_security_trace_check import statuses
from tools.radio_call_lifecycle_common import require_ordered
from tools.run_sip_handset_gate import verify_handset_health


def registered_card():
    card=bytearray(make_profile(True)+bytes.fromhex('00f110')+b'\xff'*43)
    card[1604:1609]=bytes.fromhex('00f1100001')
    card[1610]=0
    return bytes(card)


FRAMES={
    'error':'c1371c4cdca6b0f72c9c1dcc6e602de981330f67719714bc5b124e1a3b4fcfcd',
    'idle':'0b52e9f7b6dd5e60916d48d527423c857f49cee2654ebd9ca83fc2664efe1f35',
    'menu':'2c2a6aa19024c3f6bf1d3797a8f7c38edb950243ec17fe2241793a5ef029ac24',
}


def check(text,card):
    if card!=registered_card():
        raise ValueError('NHM-3 retry changed credentials or unrelated card contents')
    if statuses(text,0x20)!=[(0x9804,2,10),(0x9000,3,10)]:
        raise ValueError('NHM-3 requires one failed PIN and one successful retry reset')
    if re.findall(r'header cla=a0 ins=20 p1=00 p2=(\w\w) p3=08',text)!=['01','01']:
        raise ValueError('NHM-3 retry must address CHV1 twice, not PIN2')
    if any(statuses(text,ins) for ins in (4,0x24,0x26,0x28,0x2c,0x44,0xdc)):
        raise ValueError('NHM-3 retry unexpectedly changed card configuration or records')
    items=[(second,'Keypad 9') for second in range(8,12)]
    items += [(12,'Left Softkey / Menu')]+[(18+i,'Keypad '+d) for i,d in enumerate('1234')]
    items += [(22,'Left Softkey / Menu'),(34,'Left Softkey / Menu')]
    contacts(text,'6250_pin_retry',items)
    require_ordered(text,[
        ('wrong submit',r'6250_pin_retry: key=Left Softkey / Menu state=1 t=12\.'),
        ('rejected',r'SIM status ins=20 sw=9804'),
        ('correct submit',r'6250_pin_retry: key=Left Softkey / Menu state=1 t=22\.'),
        ('accepted',r'SIM status ins=20 sw=9000'),
        ('registered',r'LAPDm Location Updating Accept acknowledged'),
        ('Menu',r'6250_pin_retry: key=Left Softkey / Menu state=1 t=34\.'),
        ('complete',r'6250_pin_retry: complete=1 firmware_state_forcing=0'),
    ],'6250 PIN retry')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run',type=Path)
    args=parser.parse_args()
    try:
        verify_handset_health((args.run/'console.log').read_text(errors='replace'))
        text=(args.run/'error.log').read_text(errors='replace')
        card=(args.run/'nvram/nhm3hle/sim_card').read_bytes()
        check(text,card)
        registration(text,card,require_host=False)
        for name,digest in FRAMES.items():
            check_rgb_frame(args.run/f'snap/6250_pin_retry_{name}.png',digest)
    except (OSError,ValueError,RuntimeError) as error:
        parser.exit(1,f'6250 PIN retry FAIL: {error}\n')
    print('6250 physical wrong-PIN recovery and registration PASS; research HLE only')


if __name__=='__main__':main()

"""Own NSB-6 cold date-editor screen-busy Toolkit response acceptance."""
import argparse
import hashlib
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.sim_toolkit_trace_check import require_in_order
from tools.noki8890_registration_check import verify as verify_registration

EVENTS = (
    'read-binary fid=6fae offset=0 length=1 first=03',
    'header cla=a0 ins=10 p1=00 p2=00 p3=09',
    'SIM status ins=10 sw=9000',
    'proactive DISPLAY TEXT ready',
    '8890_clock_physical: key=Keypad 1',
    'SIM completion ins=f2 sw=9116',
    'header cla=a0 ins=12 p1=00 p2=00 p3=16',
    'header cla=a0 ins=14 p1=00 p2=00 p3=0d',
    'terminal-response data=81030121800202828103022001',
    'SIM status ins=14 sw=9000',
    '8890_clock_physical: key=Menu',
)
IDLE = '03cd1654572ac2cbe001a2fda57cdc3ead7b8d578d81efcaffe2af3350e0d6bc'


def verify_protocol(text):
    if '[LUA ERROR]' in text:
        raise ValueError('NSB-6 physical clock fixture failed')
    require_in_order(text.replace('[:sim_card] ', ''), EVENTS)


def verify(text, frames, storage):
    from PIL import Image
    verify_protocol(text)
    verify_registration(text)
    if len(storage) < 1611 or storage[1604:1609] != bytes.fromhex('00f1100001') or storage[1610] != 0:
        raise ValueError('persisted EF_LOCI lacks updated laboratory location')
    with Image.open(frames / '8890_date_after.png') as frame:
        if frame.size != (84, 48) or hashlib.sha256(frame.convert('L').tobytes()).hexdigest() != IDLE:
            raise ValueError('missing reviewed NSB-6 date-editor recovery frame')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    args = parser.parse_args()
    try:
        run = args.run_directory
        verify((run / 'error.log').read_text(errors='replace'), run / 'snap',
               (run / 'nvram/nsb6hle/sim_card').read_bytes())
    except (OSError, ValueError) as error:
        parser.exit(1, f'8890 Toolkit busy FAIL: {error}\n')
    print('8890 Toolkit screen-busy response and physical date-editor recovery PASS')


if __name__ == '__main__':
    main()

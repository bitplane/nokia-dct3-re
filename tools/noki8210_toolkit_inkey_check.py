"""Own NSM-3 physical GET INKEY, following successful DISPLAY TEXT."""
import argparse
import hashlib
from pathlib import Path
import sys
if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.dct3_toolkit_check import display_text_events
from tools.sim_toolkit_trace_check import require_in_order
from tools.noki8210_registration_check import verify as verify_registration
from tools.noki8210_staged_check import verify as verify_stage

EVENTS = display_text_events('8210')[:-1] + (
    'proactive GET INKEY ready', 'SIM status ins=14 sw=9115',
    'header cla=a0 ins=12 p1=00 p2=00 p3=15',
    '8210_toolkit_physical: action=inkey_5',
    '8210_toolkit_physical: action=inkey_confirm',
    'header cla=a0 ins=14 p1=00 p2=00 p3=10',
    'terminal-response data=8103022200020282810301000d020435',
    'SIM status ins=14 sw=9000',
)
FRAMES = {
    'display': '0c609d0fc7f1f59534f7e29ccaa995d441ff52f701b76093661b2458380ff558',
    'inkey': '90351a3228bbcbd52db78482a153a49a3988869afdf35dcf0d74af02d43a0458',
    'inkey_complete': '08737c08772c99df2b41fc8560587820c2c209f5f74c12893a657bd6f623e555',
}


def verify_protocol(text):
    if '[LUA ERROR]' in text:
        raise ValueError('physical GET INKEY fixture failed')
    require_in_order(text.replace('[:sim_card] ', ''), EVENTS)


def main():
    from PIL import Image
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    run = args.log.parent
    try:
        text = args.log.read_text(errors='replace')
        verify_protocol(text)
        verify_stage(text, runtime=True, selftest=True, base_record=True)
        verify_registration(text, (run / 'nvram/nsm3hle/sim_card').read_bytes())
        for phase, digest in FRAMES.items():
            with Image.open(run / f'snap/8210_toolkit_{phase}.png') as frame:
                if frame.size != (84, 48) or hashlib.sha256(frame.convert('L').tobytes()).hexdigest() != digest:
                    raise ValueError('unreviewed GET INKEY frame: ' + phase)
    except (OSError, ValueError) as error:
        parser.exit(1, f'8210 GET INKEY FAIL: {error}\n')
    print('8210 physical GET INKEY and registered idle PASS')


if __name__ == '__main__':
    main()

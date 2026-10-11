"""Own NSM-2 DISPLAY TEXT, physical dismissal and registered idle acceptance."""
import argparse
import hashlib
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.dct3_toolkit_check import display_text_events, verify_display_text, verify_interactive, verify_menu
from tools.radio_registration_trace_check import verify as verify_registration
from tools.noki8850_ussd_check import IDLE
from tools.sim_toolkit_sms_trace_check import verify as verify_sms
from tools.sim_toolkit_call_trace_check import verify as verify_call, verify_decline
from tools.sim_toolkit_trace_check import require_in_order

EVENTS = display_text_events('8850')
DISPLAY = '0c609d0fc7f1f59534f7e29ccaa995d441ff52f701b76093661b2458380ff558'


def verify_protocol(text, *, interactive=False, menu=False, sms=False, call=False, call_decline=False):
    if call_decline:
        verify_menu(text, '8850', selection_status='911c')
        verify_decline(text, '8850')
        return
    if call:
        verify_menu(text, '8850', selection_status='911c')
        verify_call(text)
        require_in_order(text, ('8850_toolkit_interactive: action=call_accept',
                               'GSM service uplink sapi=0 pd=03 message=05',
                               'GSM outgoing request id=1 digits=5551234',
                               '8850_toolkit_interactive: action=menu_exit',
                               'GSM service uplink sapi=0 pd=03 message=25'))
        return
    if sms:
        verify_menu(text, '8850', selection_status='9124')
        verify_sms(text, cp=0x39, message_reference=1)
        return
    if menu:
        verify_menu(text, '8850')
    elif interactive:
        verify_interactive(text, '8850')
    else:
        verify_display_text(text, '8850')


def verify(text, frames, storage, *, interactive=False, menu=False, sms=False, call=False, call_decline=False):
    from PIL import Image
    verify_protocol(text, interactive=interactive, menu=menu, sms=sms, call=call, call_decline=call_decline)
    verify_registration(text, 'nsm2')
    if len(storage) < 1611 or storage[1604:1609] != bytes.fromhex('00f1100001') or storage[1610] != 0:
        raise ValueError('persisted EF_LOCI lacks updated laboratory location')
    expected = (('display', DISPLAY), ('after_dismiss', IDLE))
    if interactive or menu or sms or call or call_decline:
        expected = (
            ('display', DISPLAY),
            ('inkey', '90351a3228bbcbd52db78482a153a49a3988869afdf35dcf0d74af02d43a0458'),
            ('get_input', 'c26d453971ea775fd7bdee10235bba195eb70254ce522fda3fbc6cab13bc09f9'),
            ('interactive_idle', '08737c08772c99df2b41fc8560587820c2c209f5f74c12893a657bd6f623e555'),
        )
    if menu or sms or call or call_decline:
        expected = expected[:-1] + (
            ('menu_entry', 'd79ec540f09fcf78dd5da15fc355030a99855022c105a6587f6d73a5574fa119'),
            ('menu_items', '81615371eea5c88138ed06095c244d9235578d760f9a251716bf310f9daa00a9'),
            ('menu_result', '81615371eea5c88138ed06095c244d9235578d760f9a251716bf310f9daa00a9'),
            ('menu_idle', 'd6d4b05af24a06c42a97e31c3134f7e497e149f4420f87f3d243613f06926300'),
        )
    if sms:
        expected += (('menu_network_result', '81615371eea5c88138ed06095c244d9235578d760f9a251716bf310f9daa00a9'),)
    if call or call_decline:
        expected = tuple((phase, '85b3309e03fcf99e90d3645b92124413531a3e97bdcd5fc096a629daf579a92e'
                          if phase == 'menu_result' else digest) for phase, digest in expected)
        expected += (('menu_network_result', '81615371eea5c88138ed06095c244d9235578d760f9a251716bf310f9daa00a9'
                      if call_decline else '90679abc2694f759ce89182ad0b397099755488389728b95074066a0037d12b4'),)
    for phase, digest in expected:
        with Image.open(frames / f'8850_toolkit_{phase}.png') as frame:
            if frame.size != (84, 48) or hashlib.sha256(frame.convert('L').tobytes()).hexdigest() != digest:
                raise ValueError('missing reviewed NSM-2 Toolkit frame: ' + phase)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--interactive', action='store_true')
    mode.add_argument('--menu', action='store_true')
    mode.add_argument('--sms', action='store_true')
    mode.add_argument('--call', action='store_true')
    mode.add_argument('--call-decline', action='store_true')
    args = parser.parse_args()
    try:
        run = args.run_directory
        verify((run / 'error.log').read_text(errors='replace'), run / 'snap',
               (run / 'nvram/nsm2hle/sim_card').read_bytes(), interactive=args.interactive, menu=args.menu, sms=args.sms, call=args.call, call_decline=args.call_decline)
    except (OSError, ValueError) as error:
        parser.exit(1, f'8850 Toolkit FAIL: {error}\n')
    print('8850 physical ' + ('Toolkit call refusal' if args.call_decline else 'Toolkit call CC/RR' if args.call else 'Toolkit SMS CP/RP/RR' if args.sms else 'SET UP MENU/ENVELOPE' if args.menu else 'DISPLAY TEXT/GET INKEY/GET INPUT' if args.interactive else 'DISPLAY TEXT') +
          ', terminal responses and registered idle PASS')


if __name__ == '__main__':
    main()

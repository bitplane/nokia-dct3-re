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
INPUT_EVENTS = EVENTS[:-1] + (
    'proactive GET INPUT ready', 'SIM status ins=14 sw=911a',
    'header cla=a0 ins=12 p1=00 p2=00 p3=1a',
    '8210_toolkit_physical: action=input_4',
    '8210_toolkit_physical: action=input_2',
    '8210_toolkit_physical: action=input_confirm',
    'header cla=a0 ins=14 p1=00 p2=00 p3=11',
    'terminal-response data=8103032300020282810301000d03043432',
    'SIM status ins=14 sw=9000',
)
INPUT_FRAMES = {key: FRAMES[key] for key in ('display', 'inkey')}
INPUT_FRAMES.update({
    'get_input': 'c26d453971ea775fd7bdee10235bba195eb70254ce522fda3fbc6cab13bc09f9',
    'input_complete': FRAMES['inkey_complete'],
})
MENU_EVENTS = INPUT_EVENTS[:-1] + (
    'proactive SET UP MENU ready', 'SIM status ins=14 sw=9128',
    'header cla=a0 ins=12 p1=00 p2=00 p3=28',
    'terminal-response data=810304250002028281030100',
    'SIM status ins=14 sw=9000',
    '8210_toolkit_physical: action=menu',
    '8210_toolkit_physical: action=menu_last',
    '8210_toolkit_physical: action=menu_open',
    '8210_toolkit_physical: action=menu_select',
    'header cla=a0 ins=c2 p1=00 p2=00 p3=09',
    'envelope data=d30702020181100101',
    'menu selection item=1 accepted', 'SIM status ins=c2 sw=9000',
)
MENU_FRAMES = {key: INPUT_FRAMES[key] for key in ('display', 'inkey', 'get_input')}
MENU_FRAMES.update({
    'menu_entry': '9c849d8f8a691029f0f4b029d4566a2f62b5234a989ccec3f2c5ab73dd9beb97',
    'menu_items': '81615371eea5c88138ed06095c244d9235578d760f9a251716bf310f9daa00a9',
    'menu_result': '81615371eea5c88138ed06095c244d9235578d760f9a251716bf310f9daa00a9',
})


def verify_protocol(text, *, get_input=False, menu=False, sms=False, call=False, call_decline=False):
    if '[LUA ERROR]' in text:
        raise ValueError('physical GET INKEY fixture failed')
    require_in_order(text.replace('[:sim_card] ', ''),
                     MENU_EVENTS[:-1] if sms or call or call_decline else MENU_EVENTS if menu else INPUT_EVENTS if get_input else EVENTS)
    if call_decline:
        from tools.sim_toolkit_call_trace_check import verify_decline
        verify_decline(text, '8210', action_namespace='toolkit_physical')
    if call:
        from tools.sim_toolkit_call_trace_check import verify as verify_call
        verify_call(text)
        require_in_order(text, ('8210_toolkit_physical: action=call_accept',
                               'GSM service uplink sapi=0 pd=03 message=05',
                               'GSM outgoing request id=1 digits=5551234',
                               '8210_toolkit_physical: action=menu_exit',
                               'GSM service uplink sapi=0 pd=03 message=25'))
    if sms:
        from tools.sim_toolkit_sms_trace_check import verify as verify_sms
        verify_sms(text, cp=0x39, message_reference=1)
        require_in_order(text, ('LAPDm service Channel Release acknowledged nr=2',
                               '8210_toolkit_physical: action=menu_exit'))


def main():
    from PIL import Image
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--get-input', action='store_true')
    parser.add_argument('--menu', action='store_true')
    network = parser.add_mutually_exclusive_group()
    network.add_argument('--sms', action='store_true')
    network.add_argument('--call', action='store_true')
    network.add_argument('--call-decline', action='store_true')
    args = parser.parse_args()
    run = args.log.parent
    try:
        text = args.log.read_text(errors='replace')
        verify_protocol(text, get_input=args.get_input, menu=args.menu, sms=args.sms, call=args.call, call_decline=args.call_decline)
        verify_stage(text, runtime=True, selftest=True, base_record=True)
        verify_registration(text, (run / 'nvram/nsm3hle/sim_card').read_bytes())
        expected = dict(MENU_FRAMES if args.menu or args.sms or args.call or args.call_decline else INPUT_FRAMES if args.get_input else FRAMES)
        if args.sms or args.call or args.call_decline:
            expected.update({
                'menu_network_result': MENU_FRAMES['menu_items'],
                'menu_idle': 'd6d4b05af24a06c42a97e31c3134f7e497e149f4420f87f3d243613f06926300',
            })
        if args.call or args.call_decline:
            expected.update({
                'menu_result': '85b3309e03fcf99e90d3645b92124413531a3e97bdcd5fc096a629daf579a92e',
                'menu_network_result': MENU_FRAMES['menu_items'] if args.call_decline else '90679abc2694f759ce89182ad0b397099755488389728b95074066a0037d12b4',
            })
        for phase, digest in expected.items():
            with Image.open(run / f'snap/8210_toolkit_{phase}.png') as frame:
                if frame.size != (84, 48) or hashlib.sha256(frame.convert('L').tobytes()).hexdigest() != digest:
                    raise ValueError('unreviewed GET INKEY frame: ' + phase)
    except (OSError, ValueError) as error:
        parser.exit(1, f'8210 GET INKEY FAIL: {error}\n')
    print('8210 physical ' + ('Toolkit call refusal' if args.call_decline else 'Toolkit call CC/RR' if args.call else 'Toolkit SMS CP/RP/RR' if args.sms else 'SET UP MENU/ENVELOPE' if args.menu else 'GET INPUT' if args.get_input else 'GET INKEY') + ' and laboratory registration PASS')


if __name__ == '__main__':
    main()

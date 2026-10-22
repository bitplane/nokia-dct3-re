"""Exact NSB-7 architecture/protocol/pixel restoration and physical continuation."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb7_radio_check import contract
from tools.nsb7_security_check import check_trace as check_security
from tools.nsb7_netmonitor_check import IDLE_HASH, frame
from tools.nsb7_menu_check import FRAMES
from tools.radio_state_roundtrip import verify_roundtrip


def check_architecture_snapshot(text,scenario):
    if f'state_roundtrip: result=pass scenario=nsb7_{scenario} ' not in text:
        raise ValueError('state scenario marker differs')
    if '[LUA ERROR]' in text or 'nsb7_state: FAIL' in text:
        raise ValueError('idle state fixture failed')
    rows = re.findall(r'nsb7_state: event=(saved|restored) pc=([0-9a-f]{8}) '
                      r'sp=([0-9a-f]{8}) ram=([0-9a-f]{8}) cpu=([0-9a-f,]+) t=([0-9.]+)', text)
    if len(rows) != 2 or [row[0] for row in rows] != ['saved', 'restored']:
        raise ValueError('missing architectural snapshots')
    if any(not re.fullmatch(r'[0-9a-f]{8}(?:,[0-9a-f]{8}){36}', row[4]) for row in rows):
        raise ValueError('incomplete ARM/banked register inventory')
    if rows[0][1:] != rows[1][1:]:
        raise ValueError('CPU/RAM/time restoration differs')


def check_architecture(text,scenario,tokens):
    check_architecture_snapshot(text,scenario)
    verify_roundtrip(text, tokens, 'NSB-7 '+scenario)


def check(text, call=False, sms=False, incoming_call=False, card=None, pin=False, host=False):
    if host and not ((call or incoming_call) and pin):
        raise ValueError('host-call restoration requires call state and pre-save PIN authentication')
    if pin:
        from tools.nsb7_pin_services_check import check_authentication
        check_authentication(text.split('nsb7_state: event=saved', 1)[0])
    if sum((bool(call), bool(sms), bool(incoming_call))) > 1:
        raise ValueError('choose one state scenario')
    scenario = 'incoming_call' if incoming_call else 'call' if call else 'sms' if sms else 'idle'
    tokens = ('TX packet', 'RX enqueue', 'GSM service', 'sim_device:')
    if call or incoming_call:
        tokens += ('radio_l1:',)
    if sms or (host and (call or incoming_call)):
        tokens += ('ccont_rtc:', 'ccont_analog:')
    check_architecture(text,scenario,tokens)
    if incoming_call:
        from tools.nsb7_pin_services_check import check_incoming
        if card is None:
            raise ValueError('incoming-call restoration requires actual SIM storage')
        if host:
            from tools.nsb7_host_incoming_call_check import check as check_host
            from tools.nsb7_pin_services_check import check_source
            check_source(card)
            check_host(text,restored=True)
        else:
            check_incoming(text, card)
        if not re.search(r'GSM service downlink kind=\d+ sapi=0 pd=03 message=0f[^\n]*[\s\S]*'
                         r'nsb7_state: event=saved[\s\S]*'
                         r'state_replay: phase=restored event=end[\s\S]*'
                         r'nsb7_call_input: key=End state=1', text):
            raise ValueError('incoming call must connect before save and physically release after replay')
        return
    if sms:
        saved = text.index('nsb7_state: event=saved')
        restored = text.index('state_replay: phase=restored event=begin')
        end = text.index('state_replay: phase=restored event=end')
        rp = 'GSM service uplink sapi=3 pd=09 message=01 length=5 data=8901020240'
        if 'GSM service uplink sapi=3 pd=09 message=04' not in text[:saved] or rp in text[:saved] or \
                rp not in text[restored:end]:
            raise ValueError('save must straddle incoming CP/RP acknowledgement completion')
        if 'nsb7_state_sms: complete' not in text or \
                not re.search(r'nsb7_sms_input: key=Menu state=1 t=' + ('26' if pin else '23') + r'\.', text[end:]):
            raise ValueError('missing post-load SMS reading')
        return
    if call:
        if host:
            from tools.nsb7_host_outgoing_call_check import check as check_host
            check_host(text,restored=True)
        else:
            from tools.nsb7_outgoing_call_check import check as check_call
            check_call(text)
        if not re.search(r'GSM service uplink sapi=0 pd=03 message=0f[^\n]*[\s\S]*'
                         r'nsb7_state: event=saved[\s\S]*'
                         r'state_replay: phase=restored event=end[\s\S]*'
                         r'nsb7_outgoing_call_input: key=End state=1', text):
            raise ValueError('call must connect before save and physically release after replay')
        return
    if not re.search(r'state_replay: phase=restored event=end[^\n]*\n[\s\S]*'
                     r'nsb7_state_physical: key=Menu state=1[\s\S]*'
                     r'nsb7_key_decode: key=19 observer_writes=0[\s\S]*'
                     r'nsb7_state_physical: key=Menu state=0', text):
        raise ValueError('missing post-load physical Menu decode')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('run', type=Path)
    parser.add_argument('source', type=Path, help='own input storage run')
    scenario = parser.add_mutually_exclusive_group()
    scenario.add_argument('--call', action='store_true', help='established outgoing call restoration')
    scenario.add_argument('--sms', action='store_true', help='incoming CP/RP completion and physical reading')
    scenario.add_argument('--incoming-call', action='store_true', help='PIN-authenticated connected incoming call')
    scenario.add_argument('--host-call', action='store_true', help='PIN-authenticated established outgoing host-call restoration')
    scenario.add_argument('--host-incoming-call', action='store_true', help='PIN-authenticated connected incoming host-call restoration')
    parser.add_argument('--pin', action='store_true', help='require physical PIN/cold registration before save')
    args = parser.parse_args()
    args.call = args.call or args.host_call
    args.incoming_call = args.incoming_call or args.host_incoming_call
    host = args.host_call or args.host_incoming_call
    args.pin = args.pin or host
    try:
        contract(args.image.read_bytes())
        text = (args.run / 'error.log').read_text()
        check_security(text)
        card = (args.run / 'nvram/nsb7hle/sim_card').read_bytes()
        check(text, call=args.call, sms=args.sms, incoming_call=args.incoming_call, card=card, pin=args.pin, host=host)
        if args.call or args.incoming_call:
            kind = 'incoming_call' if args.incoming_call else 'call'
            from PIL import Image
            with Image.open(args.run / f'snap/nsb7_state_{kind}_reference.png') as reference, \
                    Image.open(args.run / f'snap/nsb7_state_{kind}_restored.png') as restored:
                if reference.size != (84, 48) or restored.size != reference.size or \
                        reference.convert('RGB').tobytes() != restored.convert('RGB').tobytes():
                    raise ValueError('connected-call reference/restored pixels differ')
            final_hash=IDLE_HASH
            if args.host_call:
                from tools.nsb7_host_outgoing_call_check import FRAMES as outgoing_frames
                final_hash=outgoing_frames['nsb7_outgoing_call_final.png']
            elif args.host_incoming_call:
                from tools.nsb7_host_incoming_call_check import FRAMES as incoming_frames
                final_hash=incoming_frames['nsb7_call_final.png']
            frame(args.run / f'snap/nsb7_state_{kind}_final.png', final_hash)
            if host:
                if args.incoming_call:
                    from tools.nsb7_host_incoming_call_check import FRAMES as host_frames
                    ready,active='nsb7_call_ready.png','nsb7_call_Send.png'
                else:
                    from tools.nsb7_host_outgoing_call_check import FRAMES as host_frames
                    ready,active='nsb7_host_outgoing_ready.png','nsb7_host_outgoing_connected.png'
                frame(args.run/'snap'/ready,host_frames[ready])
                for phase in ('reference','restored'):
                    frame(args.run/f'snap/nsb7_state_{kind}_{phase}.png',host_frames[active])
        elif args.sms:
            from tools.nsb7_sms_check import check_read, check_read_frame
            from tools.radio_sms_acceptance_common import FIRST_SMS_DELIVER_BODY, SMS_NVRAM_OFFSET
            delivered = bytearray((args.source / 'nvram/nsb7hle/sim_card').read_bytes())
            record = b'\x03' + FIRST_SMS_DELIVER_BODY
            record += b'\xff' * (176-len(record))
            delivered[SMS_NVRAM_OFFSET:SMS_NVRAM_OFFSET+176] = record
            read_times = (26, 28, 30, 32) if args.pin else (23, 25)
            check_read(text, (args.run / 'nvram/nsb7hle/sim_card').read_bytes(), bytes(delivered),
                       read_times=read_times)
            check_read_frame(args.run / f'snap/nsb7_sms_read_{read_times[-1]}.png')
            from PIL import Image
            with Image.open(args.run / 'snap/nsb7_state_sms_reference.png') as reference, \
                    Image.open(args.run / 'snap/nsb7_state_sms_restored.png') as restored:
                if reference.size != (84, 48) or restored.size != reference.size or \
                        reference.convert('RGB').tobytes() != restored.convert('RGB').tobytes():
                    raise ValueError('SMS reference/restored pixels differ')
        else:
            for name in ('reference', 'restored'):
                frame(args.run / f'snap/nsb7_state_idle_{name}.png', IDLE_HASH)
            frame(args.run / 'snap/nsb7_state_idle_menu.png', FRAMES['nsb7_menu_26.png'])
        if not args.sms and (args.run / 'nvram/nsb7hle/sim_card').read_bytes() != (args.source / 'nvram/nsb7hle/sim_card').read_bytes():
            raise ValueError('state replay changed persistent SIM storage')
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-7 state: FAIL: {error}\n')
    print('PASS: exact ARM/RAM/time restore, radio replay, pixels and physical ' +
          ('End/clean idle after load' if args.call or args.incoming_call else 'SMS read after load' if args.sms else 'Menu after load'))


if __name__ == '__main__':
    main()

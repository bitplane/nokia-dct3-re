"""Physical NSM-5 PIN verification followed by own laboratory registration."""
import argparse
import hashlib
import re
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.sim_security_trace_check import validate
from tools.sim_toolkit_trace_check import require_in_order
from tools.radio_registration_trace_check import verify as registration

FRAMES = {
    'entered': '00da16bf71bbf2ea5cc5efd9080724b626b84e555c5d49305e685d092dee0546',
    'idle': '08737c08772c99df2b41fc8560587820c2c209f5f74c12893a657bd6f623e555',
    'menu': '36956cb40f917975e07505c1889210989c2915d153f4578dc29350e175b70f47',
    'recovered': '08737c08772c99df2b41fc8560587820c2c209f5f74c12893a657bd6f623e555',
}
LATE_FRAMES = dict(FRAMES, menu='c74ae44fbfc72442195e3c981e9632508bde61da87a70b7c567a2a70d9a7aa53')


def verify_authentication(text, card):
    if '[LUA ERROR]' in text:
        raise ValueError('physical PIN fixture failed')
    validate(text, card, 'verify', '1234')
    require_in_order(text, (
        '5210_pin_physical: key=Keypad 1', '5210_pin_physical: key=Keypad 2',
        '5210_pin_physical: key=Keypad 3', '5210_pin_physical: key=Keypad 4',
        '5210_pin_physical: key=Left Softkey / Menu',
        'SIM status ins=20 sw=9000',
    ))


def verify(text, card):
    verify_authentication(text, card)
    require_in_order(text, (
        'SIM status ins=20 sw=9000',
        'LAPDm Location Updating Accept acknowledged nr=1',
    ))
    registration(text, 'nsm5')


def verify_coherent_late(text, card, preserved=False):
    verify_authentication(text, card)
    require_in_order(text, (
        'TX packet type=57 payload=4 words=3 radio_phase=serving_bcch data=03050000',
        'RX enqueue type=8b payload=166',
        'SIM status ins=20 sw=9000',
        'LAPDm Location Updating Accept acknowledged nr=1',
        '5210_readiness_root: final current=00000000 queued=00000000 previous=00000000 context=00000000',
    ))
    if not re.search(r'RX enqueue type=8b payload=166 [^\n]*data=0010005600c4', text):
        raise ValueError('missing own coherent serving-carrier measurement')
    registration(text, 'nsm5', nsm5_pin=True, preserved=preserved)


def verify_pin_sms(text, card, state=False):
    from tools.radio_incoming_sms_trace_check import verify as sms_lifecycle
    verify_coherent_late(text, card)
    sms_lifecycle(text, card, 'nsm5', read=True)
    physical = '5210_pin_state_physical' if state else '5210_pin_physical'
    require_in_order(text, (
        'sim_device: update fid=6f3c record=1 length=176',
        'GSM service uplink sapi=3 pd=09 message=04 length=2 data=8904',
        'GSM service uplink sapi=3 pd=09 message=01 length=5 data=8901020240',
        'GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2',
        'LAPDm service Channel Release acknowledged nr=',
    ) + (('5210_pin_state: event=saved', '5210_pin_state: event=restored') if state else ()) + (
        physical + ': key=Left Softkey / Menu',
        physical + ': key=Left Softkey / Menu',
        'sim_device: update fid=6f3c record=1 length=176',
    ))


def verify_pin_outgoing_sms(text, rejected=False, silent=False, local_silence=False):
    from tools.radio_outgoing_sms_product_trace_check import CHECKPOINTS
    from tools.radio_call_lifecycle_common import require_ordered
    checkpoints = list(CHECKPOINTS)
    if local_silence:
        checkpoints[2] = ('local RP-silence SMS-SUBMIT', re.compile(
            r'gsm_sms_submit: cp=39 rp=01 smsc=1234567890 destination=5551234 '
            r'alphabet=0 user_length=1 outcome=3 status_report=0'))
    if silent:
        checkpoints = checkpoints[:4]
        if len(checkpoints[3][1].findall(text)) != 1:
            raise ValueError('silent SMS requires exactly one network CP-ACK')
        if re.search(r'GSM service downlink kind=(?:18|19) sapi=3', text):
            raise ValueError('network RP result during silent SMS')
        submission = re.search(r'gsm_sms_submit: .*t=([0-9.]+)', text)
        disconnect = re.search(r'TX packet type=1b .*data=0080015301.*t=([0-9.]+)', text)
        if not submission or not disconnect or not 60 <= float(disconnect[1]) - float(submission[1]) <= 80:
            raise ValueError('firmware DISC absent or outside RP-timeout window')
        require_ordered(text[disconnect.start():], (
            ('peer UA', re.compile(r'RX enqueue type=80 .*data=80[0-9a-f]{18}017301')),
            ('channel release', re.compile(r'TX packet type=02 .*radio_phase=release_channel_change')),
            ('resumed paging', re.compile(r'PCH no-identity fill')),
        ), 'NSM-5 RP-timeout clearing')
    elif rejected:
        checkpoints[4] = ('network RP-ERROR', re.compile(
            r'GSM service downlink kind=19 sapi=3 pd=09 message=01 length=7'))
        if re.search(r'GSM service downlink kind=18 sapi=3', text):
            raise ValueError('success RP-ACK in rejected SMS')
    require_ordered(text, checkpoints, 'NSM-5 PIN-enabled outgoing SMS')
    menu = '5210_pin_physical: key=Left Softkey / Menu'
    require_in_order(text, ('SIM status ins=20 sw=9000',) + (menu,) * 3 + (
        '5210_pin_physical: key=Keypad 2',) + (menu,) * 2 + tuple(
        '5210_pin_physical: key=Keypad ' + digit for digit in '5551234') + (
        menu,
        'GSM service uplink sapi=3 pd=09 message=01 length=27 data=390118000100069121436587090d11010781551532f40000a70161',
        f'gsm_sms_submit: cp=39 rp=01 smsc=1234567890 destination=5551234 alphabet=0 user_length=1 outcome={3 if local_silence else 0} status_report=0',
    ))
    if text.count('gsm_sms_submit:') != 1:
        raise ValueError('expected one PIN-enabled outgoing SMS')


def verify_pin_idle_state(text, call=False, sms=False, physical='5210_pin_state_physical'):
    entries = re.findall(r'5210_pin_state: event=(saved|restored) pc=([0-9a-f]{8}) '
                         r'sp=([0-9a-f]{8}) ram=([0-9a-f]{8}) t=([0-9.]+)', text)
    if len(entries) != 2 or [entry[0] for entry in entries] != ['saved', 'restored'] or entries[0][1:] != entries[1][1:]:
        raise ValueError('PIN idle time/PC/SP/RAM restoration differs or is incomplete')
    require_in_order(text, ('LAPDm Location Updating Accept acknowledged nr=1',
                           '5210_pin_state: event=saved', '5210_pin_state: event=restored',
                           '5210_pin_state: result=pass') +
                     ((physical + ': key=Left Softkey / Menu',) * 2 if sms else
                      () if call else (physical + ': key=Left Softkey / Menu',)) +
                     (() if sms else (physical + ': key=End',)) +
                     ('5210_pin_state: completed=true',))


def verify_late_boundary(text, card):
    verify_authentication(text, card)
    require_in_order(text, (
        'TX packet type=57 payload=4 words=3 radio_phase=serving_bcch data=03050000',
        'RX enqueue type=8b payload=166',
        '5210_measurement_entry: pc=0037019c',
        '5210_measurement_entry: pc=002e336c',
        '5210_measurement_entry: pc=002bd782',
        '5210_request_entry: pc=002e1f5e',
        '5210_request_entry: pc=002bd3d8 r0=00000005',
        '5210_request_entry: pc=002bd424',
        'SIM status ins=20 sw=9000',
    ))
    stops = re.findall(r'5210_pin_fixture: stopped_at=([0-9.]+)', text)
    if len(stops) != 1 or float(stops[0]) < 44:
        raise ValueError('delayed-readiness observation ended before the bounded run')
    verify_late_request_ownership(text)
    if ('LAPDm Location Updating Accept acknowledged' in text or
            'radio_phase=random_access' in text):
        raise ValueError('late-readiness boundary changed: review radio progress rather than retaining this negative gate')


def verify_late_request_ownership(text):
    selectors = list(re.finditer(
        r'5210_readiness_selector: argument=([0-9a-f]{8}) lr=([0-9a-f]{8}) '
        r'state=(\d+) eligible=(\d+) current=([0-9a-f]{8}) queued=([0-9a-f]{8}) t=([0-9.]+)', text))
    finals = re.findall(
        r'5210_readiness_root: final current=([0-9a-f]{8}) queued=([0-9a-f]{8}) '
        r'previous=([0-9a-f]{8}) context=([0-9a-f]{8}) writes=(\d+) selections=(\d+)', text)
    if len(finals) != 1:
        raise ValueError('missing unique final request-root observation')
    current, queued, previous, context, writes, selections = finals[0]
    if int(writes) > 80 or int(selections) > 64 or len(selectors) != int(selections):
        raise ValueError('request ownership observation is truncated')
    if not int(current, 16) or not int(queued, 16) or current == queued or not int(context, 16):
        raise ValueError('late queued/current request boundary changed')
    pin = text.index('SIM status ins=20 sw=9000')
    after = [entry for entry in selectors if entry.start() > pin]
    if len(after) != 1:
        raise ValueError('late selector lifecycle changed')
    argument, caller, state, eligible, selected_current, selected_queued, _ = after[0].groups()
    if (argument != '00000003' or caller != '00232069' or state != '6' or
            not int(eligible) or selected_current != current or selected_queued != queued):
        raise ValueError('late selector no longer preserves the eligible queued request')
    if not any(entry.start() < pin and entry.group(1) == '00000000' for entry in selectors):
        raise ValueError('missing pre-PIN argument-zero control')


def main():
    from PIL import Image
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--late-boundary', action='store_true',
                        help='verify the unresolved delayed-readiness boundary, not registration')
    mode.add_argument('--coherent-late', action='store_true',
                        help='verify delayed PIN with the own ARFCN 86 laboratory topology')
    mode.add_argument('--incoming-call', action='store_true',
                        help='verify physical Answer/End after coherent delayed-PIN registration')
    mode.add_argument('--incoming-sms', action='store_true',
                        help='verify SMS persistence and physical Read after coherent delayed PIN')
    mode.add_argument('--idle-state', action='store_true',
                        help='verify exact PIN-authenticated idle restoration and physical continuation')
    mode.add_argument('--call-state', action='store_true',
                        help='verify answered PIN-enabled call restoration and physical End')
    mode.add_argument('--sms-state', action='store_true',
                        help='restore delivered SMS before physical Read and persistent status update')
    mode.add_argument('--outgoing-call', action='store_true',
                        help='verify physical dialling after coherent delayed PIN')
    mode.add_argument('--outgoing-sms', action='store_true',
                        help='verify physical SMS composition after coherent delayed PIN')
    mode.add_argument('--outgoing-call-state', action='store_true',
                      help='restore a physically originated connected call before physical End')
    mode.add_argument('--rejected-sms', action='store_true')
    mode.add_argument('--silent-sms', action='store_true')
    mode.add_argument('--host-incoming-call', action='store_true')
    mode.add_argument('--silent-sms-state', action='store_true')
    mode.add_argument('--phonebook-save', action='store_true')
    mode.add_argument('--phonebook-read', action='store_true')
    args = parser.parse_args()
    if args.host_incoming_call:
        args.incoming_call = True
    if args.silent_sms_state:
        args.silent_sms = True
    try:
        run = args.run_directory
        text = (run / 'error.log').read_text(errors='replace')
        card = (run / 'nvram/noki5210_3/sim_card').read_bytes()
        if args.phonebook_read:
            verify_coherent_late(text, card, preserved=True)
        else:
            (verify_late_boundary if args.late_boundary else verify_coherent_late if args.coherent_late or args.incoming_call or args.incoming_sms or args.idle_state or args.call_state or args.sms_state or args.outgoing_call or args.outgoing_sms or args.outgoing_call_state or args.rejected_sms or args.silent_sms or args.phonebook_save else verify)(text, card)
        if args.phonebook_save or args.phonebook_read:
            from tools.sim_phonebook_check import validate_phonebook, validate_phonebook_storage
            if args.phonebook_save:
                validate_phonebook(text, card, b'Ada')
            else:
                validate_phonebook_storage(card, b'Ada')
                if 'update fid=6f3a' in text:
                    raise ValueError('cold read unexpectedly rewrote phonebook')
                require_in_order(text, ('SIM status ins=20 sw=9000',
                                       '5210_pin_physical: key=Right Softkey / C',
                                       '5210_pin_physical: key=Left Softkey / Menu',
                                       '5210_pin_physical: key=Left Softkey / Menu',
                                       '5210_pin_physical: key=Left Softkey / Menu'))
        if args.silent_sms:
            verify_pin_outgoing_sms(text, silent=True, local_silence=args.silent_sms_state)
            if args.silent_sms_state:
                # Recovery is End then Menu, independently checked below.
                verify_pin_idle_state(text, call=True, physical='5210_pin_physical')
                require_in_order(text, ('GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2',
                                       '5210_pin_state: event=saved', '5210_pin_state: event=restored',
                                       'data=0080015301'))
                with Image.open(run / 'snap/5210_pin_state_reference.png') as reference, \
                        Image.open(run / 'snap/5210_pin_state_restored.png') as restored:
                    if reference.size != (84, 48) or restored.size != reference.size or reference.convert('L').tobytes() != restored.convert('L').tobytes():
                        raise ValueError('SMS RP-wait restored pixels differ')
            require_in_order(text, ('TX packet type=1b', 'data=0080015301',
                                   'radio_phase=release_channel_change',
                                   '5210_pin_physical: key=End',
                                   '5210_pin_physical: key=Left Softkey / Menu'))
            timeout_hashes = set()
            for path in (run / 'snap').glob('5210_pin_sms_timeout_*.png'):
                with Image.open(path) as frame:
                    if frame.size != (84, 48):
                        raise ValueError('unexpected timeout frame geometry')
                    timeout_hashes.add(hashlib.sha256(frame.convert('L').tobytes()).hexdigest())
            if '62a7b5673d7356255226d07195925e82a5e1d6e59c38c2eb0854af6dc1749f1c' not in timeout_hashes:
                raise ValueError('reviewed Message sending failed pixels absent')
        if args.rejected_sms:
            verify_pin_outgoing_sms(text, rejected=True)
            require_in_order(text, ('LAPDm service Channel Release acknowledged nr=2',
                                   '5210_pin_physical: key=End',
                                   '5210_pin_physical: key=Left Softkey / Menu'))
        if args.outgoing_call_state:
            from tools.radio_5210_outgoing_call_trace_check import verify as outgoing_call
            from tools.radio_outgoing_call_trace_check import CONNECT_ACKNOWLEDGE, DISCONNECT
            outgoing_call(text, coherent_pin=True)
            require_in_order(text, ('SIM status ins=20 sw=9000',) + tuple(
                '5210_pin_physical: key=Keypad ' + digit for digit in '5551234') + (
                '5210_pin_physical: key=Send', '5210_pin_state: event=saved',
                '5210_pin_state: event=restored', '5210_pin_state_physical: key=End'))
            acknowledged = CONNECT_ACKNOWLEDGE.search(text)
            disconnected = DISCONNECT.search(text)
            saved = text.index('5210_pin_state: event=saved')
            restored = text.index('5210_pin_state: event=restored')
            if not acknowledged or not disconnected or not acknowledged.start() < saved < restored < disconnected.start():
                raise ValueError('outgoing call restoration must fall between Connect acknowledgment and Disconnect')
        if args.outgoing_sms:
            verify_pin_outgoing_sms(text)
        if args.outgoing_call:
            from tools.radio_5210_outgoing_call_trace_check import verify as outgoing_call
            require_in_order(text, ('SIM status ins=20 sw=9000',) + tuple(
                '5210_pin_physical: key=Keypad ' + digit for digit in '5551234') + (
                '5210_pin_physical: key=Send', '5210_pin_physical: key=End'))
            outgoing_call(text, coherent_pin=True)
        if args.idle_state or args.call_state or args.sms_state or args.outgoing_call_state:
            verify_pin_idle_state(text, call=args.call_state or args.outgoing_call_state, sms=args.sms_state)
            with Image.open(run / 'snap/5210_pin_state_reference.png') as reference, \
                    Image.open(run / 'snap/5210_pin_state_restored.png') as restored:
                if reference.size != (84, 48) or restored.size != reference.size or reference.convert('L').tobytes() != restored.convert('L').tobytes():
                    raise ValueError('PIN idle restored pixels differ')
                if args.idle_state and hashlib.sha256(reference.convert('L').tobytes()).hexdigest() != FRAMES['idle']:
                    raise ValueError('PIN idle reference is not registered idle')
        if args.incoming_sms or args.sms_state:
            verify_pin_sms(text, card, state=args.sms_state)
        if args.incoming_call:
            from tools.radio_5210_incoming_call_trace_check import verify as call_lifecycle
            require_in_order(text, ('SIM status ins=20 sw=9000',
                                   '5210_pin_physical: key=Send', '5210_pin_physical: key=End'))
            call_lifecycle(text, coherent_pin=True)
            if args.host_incoming_call:
                require_in_order(text, ('gsm_call_adapter: network registered=1 arfcn=86',
                                       'gsm_call_adapter: incoming id=1 result=accepted') + tuple(
                    f'gsm_call_adapter: incoming state id=1 epoch=1 phase={phase}'
                    for phase in ('queued', 'paging', 'alerting', 'connected', 'ended')))
        if args.call_state:
            from tools.radio_5210_incoming_call_trace_check import verify as call_lifecycle
            require_in_order(text, ('5210_pin_physical: key=Send',
                                   '5210_pin_state: event=saved',
                                   '5210_pin_state_physical: key=End'))
            call_lifecycle(text, coherent_pin=True, require_state_roundtrip=True)
        frames = ({'entered': FRAMES['entered'], 'idle': FRAMES['idle'],
                   **({'phonebook': 'e423343c5e8a3db00307d333ff04688ff10de205013682742c3282616768aba4'}
                      if args.phonebook_read else {})} if args.phonebook_save or args.phonebook_read else
                  {'entered': FRAMES['entered'], 'recovered': FRAMES['recovered'],
                   'sms_recovery_menu': 'b0482e752fafaa38a1adeaca7aafc462e1ac1a07d368e137fefd9cb985512a2c'} if args.silent_sms else
                  {'entered': FRAMES['entered'], 'recovered': FRAMES['recovered'],
                   'sms_rejected': '97376e427eaf1b6bafb9eedc2a657c78668ea96794ddeb3510da4d30010d5fb4',
                   'sms_recovery_menu': 'b0482e752fafaa38a1adeaca7aafc462e1ac1a07d368e137fefd9cb985512a2c'} if args.rejected_sms else
                  {'entered': FRAMES['entered'],
                   'sms_composed': 'f38afdd41fe1284a3f5f1e2e4ca3876c7ad9cc63e0ab6802be72d3a1f24bb3d4',
                   'sms_sent': '1eda22f78cde69cd4f9add1c90732b07254f3742ade7aede18cb818219712a78'} if args.outgoing_sms else
                  {'entered': FRAMES['entered'], 'idle': FRAMES['idle'], 'recovered': FRAMES['recovered']} if args.outgoing_call else
                  {'entered': FRAMES['entered'], 'recovered': FRAMES['recovered']} if args.call_state or args.outgoing_call_state else
                  {'entered': FRAMES['entered'], 'recovered': FRAMES['recovered'],
                   'state_menu': '19b7a982404067d7ff58d584949b7c6aa3d322cf64215f5877d604f5b6e2ef16'} if args.idle_state else
                  {'entered': FRAMES['entered'],
                   'sms_read': '6a101291e5215012ec718748cca30958cf55e688c0075678c34bcd30c2c9eea3'} if args.incoming_sms or args.sms_state else
                  {'entered': FRAMES['entered'], 'recovered': FRAMES['recovered'],
                   'ringing': '1617c0312a2bc1ed203df4e42874561ecbf1eca2c3e141b8faf6db2de8eee716'
                   if args.host_incoming_call else '1b60299914790655da2e596290642a5bc16581debf66626cc018f08823ce787b',
                   **({'host_idle': FRAMES['idle']} if args.host_incoming_call else {})}
                  if args.incoming_call else {} if args.late_boundary else
                  LATE_FRAMES if args.coherent_late else FRAMES)
        for phase, digest in frames.items():
            with Image.open(run / f'snap/5210_pin_{phase}.png') as frame:
                if frame.size != (84, 48) or hashlib.sha256(frame.convert('L').tobytes()).hexdigest() != digest:
                    raise ValueError('unexpected PIN/menu recovery pixels: ' + phase)
    except (OSError, ValueError) as error:
        parser.exit(1, f'5210 PIN registration FAIL: {error}\n')
    print('5210 PIN-enabled cold phonebook Detail and retained Ada/123 PASS; research HLE' if args.phonebook_read else
          '5210 PIN-enabled physical Ada/123 phonebook Save PASS; research HLE' if args.phonebook_save else
          '5210 PIN-enabled silent SMS timeout and physical recovery PASS; research HLE' if args.silent_sms else
          '5210 PIN-enabled rejected SMS and physical recovery PASS; research HLE' if args.rejected_sms else
          '5210 PIN-enabled outgoing-call restoration and physical release PASS; HLE signaling, not native speech' if args.outgoing_call_state else
          '5210 PIN-enabled physical outgoing SMS and Message sent PASS; research HLE' if args.outgoing_sms else
          '5210 PIN-enabled physical outgoing-call lifecycle PASS; HLE signaling, not native speech' if args.outgoing_call else
          '5210 PIN-enabled delivered-SMS restoration and persistent physical Read PASS; research HLE' if args.sms_state else
          '5210 PIN-enabled active-call restoration and physical release PASS; HLE signaling, not native speech' if args.call_state else
          '5210 PIN-authenticated exact idle restoration and physical continuation PASS; research HLE' if args.idle_state else
          '5210 PIN-enabled persistent incoming SMS and physical Read PASS; research HLE' if args.incoming_sms else
          '5210 PIN-enabled physical incoming-call lifecycle PASS; research HLE signaling, not native speech' if args.incoming_call else
          '5210 delayed PIN/measurement boundary reproduced; registration unresolved' if args.late_boundary else
          '5210 physical SIM PIN and subsequent laboratory registration PASS; research HLE')


if __name__ == '__main__':
    main()

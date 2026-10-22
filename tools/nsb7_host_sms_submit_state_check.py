"""Pending NSB-7 host SMS restoration, storage ownership and physical recovery."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb7_pin_services_check import check_authentication, check_sms
from tools.nsb7_host_sms_check import COMPOSER_HASH
from tools.nsb7_host_outgoing_call_check import check_clock
from tools.nsb7_pin_check import check_frame
from tools.nsb7_sms_check import check_read_frame
from tools.nsb7_radio_check import contract
from tools.nsb7_storage_check import inspect
from tools.radio_outgoing_host_sms_trace_check import verify as check_host
from tools.radio_call_lifecycle_common import require_ordered
from tools.run_sip_handset_gate import verify_handset_health


def check(text, initial, final):
    check_authentication(text.split('nsb7_sms_input:', 1)[0])
    check_sms(text, initial, final)
    check_host(text, octets=1)
    if '[LUA ERROR]' in text or 'nsb7_sms_submit_state: FAIL' in text:
        raise ValueError('SMS submit restoration fixture failed')
    states = re.findall(r'nsb7_sms_submit_state: event=(saved|restored) pc=([0-9a-f]{8}) '
                        r'sp=([0-9a-f]{8}) ram=([0-9a-f]{8}) cpu=([0-9a-f,]+) t=([0-9.]+)', text)
    if len(states) != 2 or text.count('nsb7_sms_submit_state: event=') != 2 or \
            [row[0] for row in states] != ['saved', 'restored'] or \
            states[0][1:] != states[1][1:] or states[0][5] != '48.000000000' or \
            any(not re.fullmatch(r'[0-9a-f]{8}(?:,[0-9a-f]{8}){36}', row[4]) for row in states) or \
            text.count('state_roundtrip: result=pass scenario=nsb7_sms_submit') != 1:
        raise ValueError('pending SMS must restore all37 ARM registers, RAM and exact time')
    if re.findall(r'gsm_call_adapter: sms request id=(\d+) epoch=(\d+) recipient=(\d+) alphabet=(\w+) octets=(\d+)', text) != [
            ('1', '1', '5551234', 'gsm7', '1'), ('1', '2', '5551234', 'gsm7', '1')]:
        raise ValueError('pending SMS must republish one unchanged request under the new epoch')
    if re.findall(r'gsm_call_adapter: sms state id=1 epoch=(\d+) phase=(\w+)', text) != [
            ('2', 'accepted'), ('2', 'ended')]:
        raise ValueError('pending SMS completed before load or under the old epoch')
    if re.findall(r'gsm_call_adapter: sms decision id=(\d+) outcome=(\d+) result=(\w+)', text) != [
            ('1', '1', 'rejected'), ('2', '1', 'rejected'),
            ('1', '0', 'accepted'), ('1', '0', 'rejected')]:
        raise ValueError('SMS host decision controls differ or a decision was accepted twice')
    saved_at = text.index('nsb7_sms_submit_state: event=saved')
    if re.search(r'GSM service downlink kind=(18|19) sapi=3', text[:saved_at]):
        raise ValueError('SMS save boundary was not waiting for the host result')
    require_ordered(text, [
        ('submit', r'gsm_sms_submit: cp=39 rp=01'),
        ('CP ACK', r'GSM service downlink kind=17 sapi=3 pd=09 message=04'),
        ('initial request', r'sms request id=1 epoch=1 recipient=5551234'),
        ('save', r'nsb7_sms_submit_state: event=saved'),
        ('load', r'nsb7_sms_submit_state: event=restored'),
        ('new request', r'sms request id=1 epoch=2 recipient=5551234'),
        ('stale rejection', r'gsm_call_adapter: stale epoch=1 current=2 type=outgoing_sms_decision id=1 result=rejected'),
        ('wrong ID', r'sms decision id=2 outcome=1 result=rejected'),
        ('accept', r'sms decision id=1 outcome=0 result=accepted'),
        ('duplicate rejection', r'sms decision id=1 outcome=0 result=rejected'),
        ('RP ACK', r'GSM service downlink kind=18 sapi=3 pd=09 message=01'),
        ('CP closure', r'GSM service uplink sapi=3 pd=09 message=04 length=2 data=3904'),
        ('RR closure', r'LAPDm service Channel Release acknowledged nr=2'),
        ('host ended', r'sms state id=1 epoch=2 phase=ended'),
        ('physical recovery', r'nsb7_sms_recovery: key=End state=1 t=78\.'),
        ('done', r'nsb7_sms_recovery: done t=88\.000000000'),
    ], 'NSB-7 pending outgoing SMS restore')
    expected = [(key, edge, f'{second}.{fraction}')
                for key, second in [('End', 78), ('Left Softkey / Menu', 82), ('End', 85)]
                for edge, fraction in [('1', '000000000'), ('0', '200000000')]]
    recovery = list(re.finditer(r'nsb7_sms_recovery: key=(.*?) state=([01]) t=([0-9.]+)', text))
    if [row.groups() for row in recovery] != expected:
        raise ValueError('post-load SMS recovery inputs differ')
    for prefix in ('nsb7_sms_reply_input', 'nsb7_sms_recovery'):
        inputs = list(re.finditer(rf'{prefix}: key=(.*?) state=([01]) t=([0-9.]+)', text))
        for index in range(0, len(inputs), 2):
            code = {'End':'0f', 'Left Softkey / Menu':'19', 'Scroll Down':'18', 'Keypad 2':'02'}[inputs[index][1]]
            if f'nsb7_key_decode: key={code} observer_writes=0' not in text[inputs[index].end():inputs[index+1].start()]:
                raise ValueError('SMS submit/recovery physical contact did not decode')
    if 'GSM outgoing request ' in text or 'GSM service downlink kind=19 sapi=3' in text:
        raise ValueError('unexpected call or RP-ERROR during accepted SMS restoration')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('run', type=Path)
    args = parser.parse_args()
    try:
        contract(args.image.read_bytes())
        text = (args.run/'error.log').read_text()
        storage = args.run/'nvram/nsb7hle'
        check(text, (args.run/'initial_sim_card.bin').read_bytes(), (storage/'sim_card').read_bytes())
        inspect((storage/'flash').read_bytes())
        check_clock(text, finish=88, control_values=('00',))
        verify_handset_health((args.run/'console.log').read_text())
        if not (args.run/'sta/nsb7hle/nsb7_sms_submit.sta').is_file():
            raise ValueError('actual pending-SMS snapshot absent')
        check_read_frame(args.run/'snap/nsb7_sms_read_29.png')
        check_frame(args.run/'snap/nsb7_sms_submit_saved.png', COMPOSER_HASH)
        # MAME's cached screen retains the cursor phase rendered before load;
        # this is not a claim of framebuffer replay across external decisions.
        check_frame(args.run/'snap/nsb7_sms_submit_restored.png',
                    '7ceafc970df5a3920ffa649254669ff2043b5bea1ed89fdee01dc270c49b624f')
        check_frame(args.run/'snap/nsb7_sms_submit_49.png',
                    'c02725ef822d34c6bbb905787f59bcf8ca3066ad39701658f7f6feaa9f91d22b')
        check_frame(args.run/'snap/nsb7_sms_submit_74.png', COMPOSER_HASH)
        check_frame(args.run/'snap/nsb7_sms_recovery_82.png',
                    '76728306449ad0f2ae8cbdbd1fc5e880d3dcff054c955e8b4a2aabc87db8e7e4')
        check_frame(args.run/'snap/nsb7_sms_recovery_final.png',
                    '29dcc2200f6f5c170afea80e751d1d3afefd83332964c4bdc0f2a843be5e2567')
    except (OSError, ValueError, RuntimeError) as error:
        parser.exit(1, f'NSB-7 pending host SMS restore: FAIL: {error}\n')
    print('PASS: pending SMS exact restore, fresh-epoch host decision, one submit/SMSS update and physical recovery')


if __name__ == '__main__':
    main()

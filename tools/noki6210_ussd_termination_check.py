"""Check network-owned SS termination, not a synthesized handset reply."""
import re
from tools.radio_call_lifecycle_common import require_ordered


def verify(text, events, *, restore=False):
    require_ordered(text, (
        ('PIN accepted', r'SIM status ins=20 sw=9000'),
        ('registered', r'gsm_call_adapter: network registered=1 arfcn=35\b'),
        ('admission', r'incoming ussd id=1 result=accepted'),
        ('request', r'gsm_ss: network_initiated operation=request'),
    ), '6210 authenticated termination')
    phases=[e for e in events if e.get('type')=='incoming_ussd_state']
    epoch=2 if restore else 1
    expected=[dict(type='incoming_ussd_state',request_id=1,epoch=1,phase='queued')]
    if restore:
        from tools.arm_architecture_snapshot_check import verify_roundtrip
        verify_roundtrip(text, prefix='6210_ussd_termination_state', scenario='npe3_ussd_termination')
        require_ordered(text, (
            ('request', r'gsm_ss: network_initiated operation=request'),
            ('dismissal', r'6210_host_ussd_input: key=End state=0 t=30\.150000000'),
            ('save', r'6210_ussd_termination_state: event=saved'),
            ('restore', r'6210_ussd_termination_state: event=restored'),
            ('termination', r'gsm_ss: network_terminated transaction=0b'),
        ), '6210 dismissed pending termination restore')
        expected.append(dict(type='incoming_ussd_state',request_id=1,epoch=2,phase='queued'))
        if text.count('6210_ussd_termination_restore: architecture=pass') != 1:
            raise ValueError('missing unique restored architecture check')
        epochs=[e['epoch'] for e in events if e.get('type')=='call_adapter_ready']
        if epochs != [1,2]:raise ValueError('restore epochs differ')
    expected.append(dict(type='incoming_ussd_state',request_id=1,epoch=epoch,phase='cancelled'))
    if phases != expected:
        raise ValueError('expected one correlated queued/cancelled lifecycle')
    if any(e.get('type')=='incoming_ussd_response' for e in events):
        raise ValueError('network termination fabricated a handset result')
    attempts=re.findall(r'incoming ussd termination id=(\d+) result=(\w+)',text)
    if attempts != [('1','rejected'),('999','rejected'),('1','accepted'),('1','rejected')]:
        raise ValueError('termination identity/duplicate controls differ')
    needles=['gsm_ss: network_initiated operation=request',
             'gsm_ss: network_terminated transaction=0b',
             'pd=0b message=2a length=2', 'pd=06 message=0d length=3',
             f'incoming ussd state id=1 epoch={epoch} phase=cancelled',
             '6210_host_ussd_input: key=Left Softkey / Menu state=1 t=56.000000000',
             '6210_host_ussd_input: key=Left Softkey / Menu state=0 t=56.150000000',
             '6210_host_ussd_termination: complete=1 firmware_state_forcing=0']
    if restore:
        needles.insert(1,'6210_ussd_termination_restore: architecture=pass')
    for needle in (needles[0],'gsm_ss: network_terminated transaction=0b',
                   'pd=0b message=2a length=2','pd=06 message=0d length=3'):
        if text.count(needle)!=1:
            raise ValueError('missing or duplicated transaction boundary: '+needle)
    position=0
    for needle in needles:
        found=text.find(needle,position)
        if found<0:
            raise ValueError('missing or unordered termination evidence: '+needle)
        position=found+len(needle)
    if 'gsm_ss: network_result' in text:
        raise ValueError('unexpected handset result on termination path')

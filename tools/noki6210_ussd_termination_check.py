"""Check network-owned SS termination, not a synthesized handset reply."""
import re


def verify(text, events, *, restore=False):
    phases=[e for e in events if e.get('type')=='incoming_ussd_state']
    epoch=2 if restore else 1
    expected=[dict(type='incoming_ussd_state',request_id=1,epoch=1,phase='queued')]
    if restore:
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

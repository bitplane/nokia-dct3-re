"""Verify real pending CHV1 command rollback, wire replay and single retry loss."""
import hashlib
import re
from tools.nse2_sim_check import CODE_ERROR_FRAME,verify_pin_input
from tools.nse2_sim_write_state_check import parse_wire_replay
from tools.sim_security_trace_check import ATTEMPTS_OFFSET,AOC_NVRAM_SIZE,statuses


def check_pin_state(text,wire,before,after,reference,restored):
    if len(before)!=AOC_NVRAM_SIZE or len(after)!=AOC_NVRAM_SIZE:
        raise ValueError('PIN replay requires complete card images')
    if verify_pin_input(before)!=3:
        raise ValueError('PIN state fixture requires three initial retries')
    expected=bytearray(before)
    expected[ATTEMPTS_OFFSET]=2
    if after!=expected:
        raise ValueError('PIN replay changed unrelated card storage or consumed retries twice')
    if any(error in text for error in ('nse2_sim_state: FAIL','LUA ERROR','Error in')):
        raise ValueError('PIN state observer failed')
    snapshots=re.findall(r'nse2_sim_state: phase=(saved|restored) (t=[^\n]+)',text)
    if len(snapshots)!=2 or [s[0] for s in snapshots]!=['saved','restored'] or snapshots[0][1]!=snapshots[1][1]:
        raise ValueError('PIN checkpoint architecture differs')
    fields=dict(x.split('=',1) for x in snapshots[0][1].split())
    if set(fields)!={'t','pc','sp','ram','cpu','card','simi','record','partial'} or not 7.66<float(fields['t'])<7.67:
        raise ValueError('PIN checkpoint timing/fields differ')
    if not re.fullmatch(r'[0-9a-f]{8}(?:,[0-9a-f]{8}){36}',fields['cpu']):
        raise ValueError('PIN checkpoint ARM registers incomplete')
    if fields['card']!='m_ins:20,m_tx_len:0,m_tx_expected:8,m_receiving_body:1,m_selected_file:6fad':
        raise ValueError('PIN card command is not pending before its body')
    if fields['simi']!='m_rx_count:1,m_rx_ready:0,m_iir:0,m_tx_ready_pending:1,m_uart_tx_count:0':
        raise ValueError('PIN controller checkpoint differs')
    if fields['record']!='0303' or fields['partial']:
        raise ValueError('PIN retries/body did not roll back')
    if re.findall(r'nse2_sim_state: committed=(reference|restored) record=([0-9a-f]+)',text)!=[('reference','0203'),('restored','0203')]:
        raise ValueError('PIN per-leg retry outcome differs')
    boundary=text.index('nse2_sim_state: phase=restored')
    for leg in (text[:boundary],text[boundary:]):
        if statuses(leg,0x20)!=[(0x9804,2,10)]:
            raise ValueError('PIN leg lacks exactly one wrong-code result')
    if text.count('nse2_pin_contact_replay: restored=1 physical_only=1')!=1:
        raise ValueError('PIN physical contact timeline was not restored')
    releases=re.findall(r'nse2_pin_contact: event=10 down=0 t=([0-9.]+) physical_only=1',text)
    if len(releases)!=2 or releases[0]!=releases[1] or not float(releases[0])>float(fields['t']):
        raise ValueError('PIN held-key release replay differs')
    events=parse_wire_replay(wire)['reference']
    prefix=[('rx',0x20)]+[('tx',v) for v in b'1111\xff\xff\xff\xff']+[('rx',0x98),('rx',4)]
    if [event[1:] for event in events[:11]]!=prefix:
        raise ValueError('PIN procedure/body/status wire prefix differs')
    count=str(len(events))
    if re.findall(r'nse2_sim_state: completed=1 reference=(\d+) restored=(\d+) sent_before_save=0',text)!=[(count,count)]:
        raise ValueError('PIN replay summary differs')
    if any(hashlib.sha256(pixels).hexdigest()!=CODE_ERROR_FRAME for pixels in (reference,restored)):
        raise ValueError('PIN reference/restored error frame differs')
    return dict(pending_pin_restored=True,wire_events=len(events),pin_attempts_remaining=2,
                double_retry_consumption=False,stable_long_run=False)

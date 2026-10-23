"""Verify pending CHV1 VERIFY/UNBLOCK rollback and whole-card replay outcomes."""
import hashlib
import re
from tools.nse2_sim_check import CODE_ERROR_FRAME,PIN_BLOCKED_FRAME,SECURITY_FRAME,verify_pin_input
from tools.nse2_sim_write_state_check import parse_wire_replay
from tools.sim_security_trace_check import ATTEMPTS_OFFSET,AOC_NVRAM_SIZE,CHV_OFFSET,statuses


def check_puk_state(text,wire,before,after,reference,restored):
    if len(before)!=AOC_NVRAM_SIZE or len(after)!=AOC_NVRAM_SIZE:
        raise ValueError('PUK replay requires complete card images')
    verify_pin_input(before,blocked=True)
    expected=bytearray(before)
    expected[CHV_OFFSET:CHV_OFFSET+8]=b'2468\xff\xff\xff\xff'
    expected[ATTEMPTS_OFFSET]=3
    expected[ATTEMPTS_OFFSET+2]=10
    if after!=expected:
        raise ValueError('PUK replay changed unrelated credentials or storage')
    if any(error in text for error in ('nse2_sim_state: FAIL','LUA ERROR','Error in')):
        raise ValueError('PUK observer failed')
    snapshots=re.findall(r'nse2_sim_state: phase=(saved|restored) (t=[^\n]+)',text)
    if len(snapshots)!=2 or [s[0] for s in snapshots]!=['saved','restored'] or snapshots[0][1]!=snapshots[1][1]:
        raise ValueError('PUK checkpoint architecture differs')
    fields=dict(x.split('=',1) for x in snapshots[0][1].split())
    if set(fields)!={'t','pc','sp','ram','cpu','card','simi','record','partial'} or not 18.26<float(fields['t'])<18.27:
        raise ValueError('PUK checkpoint timing/fields differ')
    if not re.fullmatch(r'[0-9a-f]{8}(?:,[0-9a-f]{8}){36}',fields['cpu']):
        raise ValueError('PUK checkpoint ARM registers incomplete')
    if fields['card']!='m_ins:2c,m_tx_len:0,m_tx_expected:10,m_receiving_body:1,m_selected_file:6fad':
        raise ValueError('PUK command is not pending before its body')
    if fields['simi']!='m_rx_count:1,m_rx_ready:0,m_iir:0,m_tx_ready_pending:1,m_uart_tx_count:0':
        raise ValueError('PUK controller checkpoint differs')
    old='0003'+before[CHV_OFFSET:CHV_OFFSET+16].hex()
    new='0303'+after[CHV_OFFSET:CHV_OFFSET+16].hex()
    if fields['record']!=old or fields['partial']:
        raise ValueError('PUK credentials/retries did not roll back')
    if re.findall(r'nse2_sim_state: committed=(reference|restored) record=([0-9a-f]+)',text)!=[('reference',new),('restored',new)]:
        raise ValueError('PUK per-leg credentials/retries differ')
    boundary=text.index('nse2_sim_state: phase=restored')
    for leg in (text[:boundary],text[boundary:]):
        if statuses(leg,0x2c)!=[(0x9000,3,10)]:
            raise ValueError('PUK leg lacks exactly one recovery result')
    if text.count('nse2_puk_contact_replay: restored=1 physical_only=1')!=1:
        raise ValueError('PUK contact timeline not restored')
    releases=re.findall(r'nse2_puk_contact: event=38 down=0 t=([0-9.]+) physical_only=1',text)
    if len(releases)!=2 or releases[0]!=releases[1] or not float(releases[0])>float(fields['t']):
        raise ValueError('PUK held-key release replay differs')
    events=parse_wire_replay(wire)['reference']
    prefix=[('rx',0x2c)]+[('tx',v) for v in b'123456782468\xff\xff\xff\xff']+[('rx',0x90),('rx',0)]
    if [event[1:] for event in events[:19]]!=prefix:
        raise ValueError('PUK procedure/body/status differs')
    count=str(len(events))
    if re.findall(r'nse2_sim_state: completed=1 reference=(\d+) restored=(\d+) sent_before_save=0',text)!=[(count,count)]:
        raise ValueError('PUK replay summary differs')
    if any(hashlib.sha256(p).hexdigest()!=SECURITY_FRAME for p in (reference,restored)):
        raise ValueError('PUK restored phone-code prompt differs')
    return dict(pending_puk_restored=True,wire_events=len(events),replacement_pin='2468',
                pin_attempts_remaining=3,puk_attempts_remaining=10,stable_long_run=False)


def check_pin_state(text,wire,before,after,reference,restored,*,last_retry=False,buffered_body=False,status_pending=False):
    if sum((last_retry,buffered_body,status_pending))>1:
        raise ValueError('PIN checkpoint modes are mutually exclusive')
    if len(before)!=AOC_NVRAM_SIZE or len(after)!=AOC_NVRAM_SIZE:
        raise ValueError('PIN replay requires complete card images')
    initial=1 if last_retry else 3
    remaining=initial-1
    status=0x9840 if last_retry else 0x9804
    if verify_pin_input(before)!=initial:
        raise ValueError('PIN state fixture initial retry count differs')
    expected=bytearray(before)
    expected[ATTEMPTS_OFFSET]=remaining
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
    expected_card='m_ins:20,m_tx_len:0,m_tx_expected:0,m_receiving_body:0,m_selected_file:6fad' if status_pending else 'm_ins:20,m_tx_len:0,m_tx_expected:8,m_receiving_body:1,m_selected_file:6fad'
    if fields['card']!=expected_card:
        raise ValueError('PIN card transaction phase differs')
    expected_simi='m_rx_count:0,m_rx_ready:0,m_iir:0,m_tx_ready_pending:0,m_uart_tx_count:5' if buffered_body else 'm_rx_count:1,m_rx_ready:0,m_iir:0,m_tx_ready_pending:1,m_uart_tx_count:0'
    if status_pending:
        expected_simi='m_rx_count:2,m_rx_ready:0,m_iir:0,m_tx_ready_pending:1,m_uart_tx_count:0'
    if fields['simi']!=expected_simi:
        raise ValueError('PIN controller checkpoint differs')
    expected_body='31313131ffffffff' if status_pending else '31313131ff' if buffered_body else ''
    if fields['record']!=f'{remaining if status_pending else initial:02x}03' or fields['partial']!=expected_body:
        raise ValueError('PIN checkpoint retries/body differ')
    if re.findall(r'nse2_sim_state: committed=(reference|restored) record=([0-9a-f]+)',text)!=[('reference',f'{remaining:02x}03'),('restored',f'{remaining:02x}03')]:
        raise ValueError('PIN per-leg retry outcome differs')
    boundary=text.index('nse2_sim_state: phase=restored')
    for index,leg in enumerate((text[:boundary],text[boundary:])):
        expected_status=[] if status_pending and index==1 else [(status,remaining,10)]
        if statuses(leg,0x20)!=expected_status:
            raise ValueError('PIN leg command-result execution count differs')
    if status_pending and statuses(text[text.index('nse2_sim_state: phase=saved'):],0x20):
        raise ValueError('committed PIN command executed again after its checkpoint')
    if text.count('nse2_pin_contact_replay: restored=1 physical_only=1')!=1:
        raise ValueError('PIN physical contact timeline was not restored')
    releases=re.findall(r'nse2_pin_contact: event=10 down=0 t=([0-9.]+) physical_only=1',text)
    if len(releases)!=2 or releases[0]!=releases[1] or not float(releases[0])>float(fields['t']):
        raise ValueError('PIN held-key release replay differs')
    events=parse_wire_replay(wire)['reference']
    prefix=([('tx',0xff)]*3 if buffered_body else [('rx',0x20)]+[('tx',v) for v in b'1111\xff\xff\xff\xff'])+[('rx',0x98),('rx',status&255)]
    if status_pending:
        prefix=[('rx',0x98),('rx',status&255)]
    if [event[1:] for event in events[:len(prefix)]]!=prefix:
        raise ValueError('PIN procedure/body/status wire prefix differs')
    count=str(len(events))
    if re.findall(rf'nse2_sim_state: completed=1 reference=(\d+) restored=(\d+) sent_before_save={8 if status_pending else 5 if buffered_body else 0}',text)!=[(count,count)]:
        raise ValueError('PIN replay summary differs')
    expected_frame=PIN_BLOCKED_FRAME if last_retry else CODE_ERROR_FRAME
    if any(hashlib.sha256(pixels).hexdigest()!=expected_frame for pixels in (reference,restored)):
        raise ValueError('PIN reference/restored error frame differs')
    return dict(pending_pin_restored=True,status_pending=status_pending,buffered_body_bytes=5 if buffered_body else 0,wire_events=len(events),pin_attempts_remaining=remaining,pin_blocked=last_retry,
                double_retry_consumption=False,stable_long_run=False)

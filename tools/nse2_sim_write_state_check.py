"""Check pending SIM UPDATE restoration with independent recorded wire replay."""
import hashlib
import re

from tools.nse2_sim_check import PHONEBOOK_SAVED_FRAME, SMS_SAVED_FRAME
from tools.sim_sms_draft import DRAFT, SMS_OFFSET

RECORD=b'A'+b'\xff'*17+bytes.fromhex('038121f3')+b'\xff'*10


def parse_wire_replay(wire):
    events={'reference':[],'restored':[]}
    for line in wire.splitlines():
        match=re.fullmatch(r'(reference|restored):(\d+):([0-9.]+):(tx|rx):([0-9a-f]{2})',line)
        if not match:
            raise ValueError('SIM wire artifact has an invalid event')
        phase,index,time,kind,value=match.groups()
        if int(index)!=len(events[phase])+1 or not 0<=float(time)<.5:
            raise ValueError('SIM wire order/window differs')
        if events[phase] and float(time)<float(events[phase][-1][0]):
            raise ValueError('SIM wire time goes backwards')
        events[phase].append((time,kind,int(value,16)))
    if not 0<len(events['reference'])<2048 or events['reference']!=events['restored']:
        raise ValueError('SIM wire replay is empty, saturated or divergent')
    return events


def check_write_state(text,wire,before,after,reference_pixels,restored_pixels,*,sms=False):
    record=DRAFT if sms else RECORD
    length=len(record)
    partial=60 if sms else 0
    fid='6f3c' if sms else '6f3a'
    offset=SMS_OFFSET if sms else 0
    free=b'\x00'+b'\xff'*175 if sms else b'\xff'*32
    if len(before)!=3570 or before[:1600]!=b'\xff'*1600:
        raise ValueError('fresh full laboratory card required')
    expected=bytearray(before)
    if before[offset:offset+length]!=free:
        raise ValueError('SIM input record is not free')
    expected[offset:offset+length]=record
    if after!=expected:
        raise ValueError('SIM replay modified unrelated persistent storage')
    if 'nse2_sim_state: FAIL' in text or 'LUA ERROR' in text or 'Error in' in text:
        raise ValueError('SIM state observer failed')
    requested=re.findall(rf'nse2_sim_state: requested=1 sent={partial} t=([0-9.]+)',text)
    snapshots=re.findall(r'nse2_sim_state: phase=(saved|restored) (t=[^\n]+)',text)
    if len(requested)!=1 or len(snapshots)!=2 or [s[0] for s in snapshots]!=['saved','restored'] or snapshots[0][1]!=snapshots[1][1]:
        raise ValueError('pending SIM checkpoint/architecture differs')
    fields=dict(x.split('=',1) for x in snapshots[0][1].split())
    low,high=(25.93,25.94) if sms else (27.57,27.6)
    if set(fields)!={'t','pc','sp','ram','cpu','card','simi','record','partial'} or not low<float(fields['t'])<high or not 0<=float(fields['t'])-float(requested[0])<.001:
        raise ValueError('SIM checkpoint is outside the released-contact/header window')
    card=dict(x.split(':') for x in fields['card'].split(','))
    simi=dict(x.split(':') for x in fields['simi'].split(','))
    if card!={'m_ins':'dc','m_tx_len':f'{partial:x}','m_tx_expected':f'{length:x}','m_receiving_body':'1','m_selected_file':fid}:
        raise ValueError('card APDU pending UPDATE state differs')
    if simi!={'m_rx_count':'0' if sms else '1','m_rx_ready':'0','m_iir':'0','m_tx_ready_pending':'1','m_uart_tx_count':'0'}:
        raise ValueError('SIMI pending queue/timer state differs')
    if fields['record']!=free.hex() or fields['partial']!=record[:partial].hex():
        raise ValueError('SIM stored record/partial body was not restored before commit')
    committed=re.findall(r'nse2_sim_state: committed=(reference|restored) record=([0-9a-f]+)',text)
    if committed!=[('reference',record.hex()),('restored',record.hex())]:
        raise ValueError('reference/restored card records did not commit identically')
    if not re.fullmatch(r'[0-9a-f]{8}(?:,[0-9a-f]{8}){36}',fields['cpu']):
        raise ValueError('SIM checkpoint ARM observation is incomplete')
    headers=re.findall(rf'sim_device: header cla=a0 ins=dc p1=01 p2=04 p3={length:02x} selected={fid} t=([0-9.]+)',text)
    if len(headers)!=1 or not 0<=float(fields['t'])-float(headers[0])<(.02 if sms else .001) or (not sms and abs(float(requested[0])-float(headers[0]))>1e-7):
        raise ValueError('SIM header was not delivered before the pending checkpoint')
    contact='nse2_sms_contact: event=22' if sms else 'nse2_phonebook_contact: event=16'
    released=re.findall(contact+r' down=0 t=([0-9.]+) physical_only=1',text)
    if len(released)!=1 or not float(released[0])<float(fields['t']):
        raise ValueError('SIM checkpoint still has a held physical Save contact')
    restored_at=text.index('nse2_sim_state: phase=restored')
    for leg in (text[:restored_at],text[restored_at:]):
        updates=re.findall(r'sim_device: update fid=([0-9a-f]+) record=(\d+) length=(\d+) t=([0-9.]+)',leg)
        if len(updates)!=1 or updates[0][:3]!=(fid,'1',str(length)) or not float(fields['t'])<float(updates[0][3])<(26 if sms else 27.7):
            raise ValueError('SIM leg lacks exactly one post-checkpoint record commit')
        if leg.count('SIM status ins=dc sw=9000')!=1:
            raise ValueError('SIM leg lacks one successful UPDATE result')
    events=parse_wire_replay(wire)
    tx=bytes(value for _,kind,value in events['reference'] if kind=='tx')
    rx=bytes(value for _,kind,value in events['reference'] if kind=='rx')
    valid=(tx==record[partial:] and rx==bytes.fromhex('9000')) if sms else (events['reference'][0][1:]==('rx',0xdc) and tx[:32]==record and rx[:3]==bytes.fromhex('dc9000'))
    if not valid:
        raise ValueError('pending UPDATE body/procedure/status differs')
    completion=re.findall(rf'nse2_sim_state: completed=1 reference=(\d+) restored=(\d+) sent_before_save={partial}',text)
    if completion!=[(str(len(events['reference'])),str(len(events['reference'])))]:
        raise ValueError('SIM replay summary differs from recorded traffic')
    expected_frame=SMS_SAVED_FRAME if sms else PHONEBOOK_SAVED_FRAME
    if any(hashlib.sha256(p).hexdigest()!=expected_frame for p in (reference_pixels,restored_pixels)):
        raise ValueError('reference/restored Saved frame differs')
    return dict(pending_sim_update_restored=True,partial_body_bytes=partial,wire_events=len(events['reference']),
                native_registration_verified=False,stable_long_run=False)

"""Check pending SIM UPDATE restoration with independent recorded wire replay."""
import hashlib
import re

from tools.nse2_sim_check import PHONEBOOK_SAVED_FRAME

RECORD=b'A'+b'\xff'*17+bytes.fromhex('038121f3')+b'\xff'*10


def check_write_state(text,wire,before,after,reference_pixels,restored_pixels):
    if len(before)!=3570 or before[:1600]!=b'\xff'*1600:
        raise ValueError('fresh full laboratory card required')
    expected=bytearray(before)
    expected[:32]=RECORD
    if after!=expected:
        raise ValueError('SIM replay modified unrelated persistent storage')
    if 'nse2_sim_state: FAIL' in text or 'LUA ERROR' in text or 'Error in' in text:
        raise ValueError('SIM state observer failed')
    requested=re.findall(r'nse2_sim_state: requested=1 sent=0 t=([0-9.]+)',text)
    snapshots=re.findall(r'nse2_sim_state: phase=(saved|restored) (t=[^\n]+)',text)
    if len(requested)!=1 or len(snapshots)!=2 or [s[0] for s in snapshots]!=['saved','restored'] or snapshots[0][1]!=snapshots[1][1]:
        raise ValueError('pending SIM checkpoint/architecture differs')
    fields=dict(x.split('=',1) for x in snapshots[0][1].split())
    if set(fields)!={'t','pc','sp','ram','cpu','card','simi','adn'} or not 27.57<float(fields['t'])<27.6 or not 0<=float(fields['t'])-float(requested[0])<.001:
        raise ValueError('SIM checkpoint is outside the released-contact/header window')
    card=dict(x.split(':') for x in fields['card'].split(','))
    simi=dict(x.split(':') for x in fields['simi'].split(','))
    if card!={'m_ins':'dc','m_tx_len':'0','m_tx_expected':'20','m_receiving_body':'1','m_selected_file':'6f3a'}:
        raise ValueError('card APDU state is not pending its full UPDATE body')
    if simi!={'m_rx_count':'1','m_rx_ready':'0','m_iir':'0','m_tx_ready_pending':'1','m_uart_tx_count':'0'}:
        raise ValueError('SIMI queued procedure/timer state differs')
    if fields['adn']!='ff'*32:
        raise ValueError('ADN was not restored to its erased precommit contents')
    committed=re.findall(r'nse2_sim_state: committed=(reference|restored) adn=([0-9a-f]+)',text)
    if committed!=[('reference',RECORD.hex()),('restored',RECORD.hex())]:
        raise ValueError('reference/restored card records did not commit identically')
    if not re.fullmatch(r'[0-9a-f]{8}(?:,[0-9a-f]{8}){36}',fields['cpu']):
        raise ValueError('SIM checkpoint ARM observation is incomplete')
    headers=re.findall(r'sim_device: header cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3a t=([0-9.]+)',text)
    if len(headers)!=1 or abs(float(requested[0])-float(headers[0]))>1e-7 or not float(headers[0])<=float(fields['t']):
        raise ValueError('SIM header was not delivered before the pending checkpoint')
    released=re.findall(r'nse2_phonebook_contact: event=16 down=0 t=([0-9.]+) physical_only=1',text)
    if len(released)!=1 or not float(released[0])<float(fields['t']):
        raise ValueError('SIM checkpoint still has a held physical Save contact')
    restored_at=text.index('nse2_sim_state: phase=restored')
    for leg in (text[:restored_at],text[restored_at:]):
        updates=re.findall(r'sim_device: update fid=([0-9a-f]+) record=(\d+) length=(\d+) t=([0-9.]+)',leg)
        if len(updates)!=1 or updates[0][:3]!=('6f3a','1','32') or not float(fields['t'])<float(updates[0][3])<27.7:
            raise ValueError('SIM leg lacks exactly one post-checkpoint ADN commit')
        if leg.count('SIM status ins=dc sw=9000')!=1:
            raise ValueError('SIM leg lacks one successful UPDATE result')
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
    tx=bytes(value for _,kind,value in events['reference'] if kind=='tx')
    rx=bytes(value for _,kind,value in events['reference'] if kind=='rx')
    if events['reference'][0][1:]!=('rx',0xdc) or tx[:32]!=RECORD or rx[:3]!=bytes.fromhex('dc9000'):
        raise ValueError('pending UPDATE body/procedure/status differs')
    completion=re.findall(r'nse2_sim_state: completed=1 reference=(\d+) restored=(\d+) sent_before_save=0',text)
    if completion!=[(str(len(events['reference'])),str(len(events['reference'])))]:
        raise ValueError('SIM replay summary differs from recorded traffic')
    if any(hashlib.sha256(p).hexdigest()!=PHONEBOOK_SAVED_FRAME for p in (reference_pixels,restored_pixels)):
        raise ValueError('reference/restored Saved frame differs')
    return dict(pending_sim_update_restored=True,wire_events=len(events['reference']),
                native_registration_verified=False,stable_long_run=False)

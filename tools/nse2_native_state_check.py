"""Check bounded native NSE-2 architectural restore, I/O replay and physical UI."""
import hashlib
import re

MENU_FRAME='601a551845a9a2dd5a2df5f54f2add55b8ce4ec2edcdab445fb886b84eb414fd'
DSP_REGISTERS=set('PC XPC A B T TRN RTN RC SP ST0 ST1 PMST AR0 AR1 AR2 AR3 AR4 AR5 AR6 AR7 BRC RSA REA BK IFR IMR SWWSR BSCR SWCR TIM PRD TCR IDLE ILLEGAL GENPC CURPC'.split())


def check_state(text,before,after,reference_pixels,restored_pixels):
    if len(before)!=3570 or after!=before:
        raise ValueError('native UI restoration changed the full SIM card')
    if 'nse2_state: FAIL' in text or 'LUA ERROR' in text or 'Error in' in text:
        raise ValueError('native state observer failed')
    snapshots=re.findall(r'nse2_state: phase=(saved|restored) (t=[^\n]+)',text)
    if len(snapshots)!=2 or [s[0] for s in snapshots]!=['saved','restored'] or snapshots[0][1]!=snapshots[1][1]:
        raise ValueError('native architectural snapshot differs or is incomplete')
    fields=dict(item.split('=',1) for item in snapshots[0][1].split())
    if set(fields)!={'t','pc','sp','ram','cpu','dsp','data','program'} or fields['t']!='18.500000000':
        raise ValueError('native checkpoint fields/time differ')
    if not re.fullmatch(r'[0-9a-f]{8}(?:,[0-9a-f]{8}){36}',fields['cpu']):
        raise ValueError('ARM register observation is incomplete')
    if any(not re.fullmatch('[0-9a-f]{8}',fields[name]) for name in ('pc','sp','ram','data','program')):
        raise ValueError('native snapshot field encoding differs')
    cpu=fields['cpu'].split(',')
    if cpu[15]!=fields['pc'] or cpu[13]!=fields['sp'] or not 0x200000<=int(fields['pc'],16)<0x400000 or not 0x100000<=int(fields['sp'],16)<0x110000:
        raise ValueError('native checkpoint lacks consistent own-ROM/RAM architecture')
    dsp=dict(item.split(':') for item in fields['dsp'].split(','))
    if set(dsp)!=DSP_REGISTERS or any(not re.fullmatch('[0-9a-f]+',v) for v in dsp.values()) or int(dsp['ILLEGAL'],16)!=0:
        raise ValueError('DSP register observation is incomplete or illegal')
    replay=re.findall(r'nse2_state_replay: reference=(\d+) restored=(\d+) window=2\.7 exact=1',text)
    if len(replay)!=1 or replay[0][0]!=replay[0][1] or not 0<int(replay[0][0])<16384:
        raise ValueError('native I/O replay is incomplete, empty or over capacity')
    if text.count('nse2_state: completed=1')!=1:
        raise ValueError('native UI replay did not complete once')
    contacts=re.findall(r'nse2_state_contact: phase=(reference|restored) down=([01])',text)
    if contacts!=[('reference','1'),('reference','0'),('restored','1'),('restored','0')]:
        raise ValueError('native restored UI lacks physical Menu continuation')
    if any(hashlib.sha256(pixels).hexdigest()!=MENU_FRAME for pixels in (reference_pixels,restored_pixels)):
        raise ValueError('native reference/restored Phone book frame differs')
    return dict(native_ui_restore=True,dsp_registers=36,arm_registers=37,
                io_replay_events=int(replay[0][0]),replay_seconds=2.7,
                native_registration_verified=False,stable_long_run=False)

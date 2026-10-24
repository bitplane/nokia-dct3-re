"""Check bounded native NSE-2 architectural restore, I/O replay and physical UI."""
import hashlib
import re

MENU_FRAME='601a551845a9a2dd5a2df5f54f2add55b8ce4ec2edcdab445fb886b84eb414fd'
DSP_REGISTERS=set('PC XPC A B T TRN RTN RC SP ST0 ST1 PMST AR0 AR1 AR2 AR3 AR4 AR5 AR6 AR7 BRC RSA REA BK IFR IMR SWWSR BSCR SWCR TIM PRD TCR IDLE ILLEGAL GENPC CURPC'.split())
# Widths/masks of the core's exported state interface, not inferred silicon
# validity for arbitrary combinations of these register values.
DSP_MASKS = dict.fromkeys(DSP_REGISTERS, 0xffff)
DSP_MASKS.update(A=0xffffffffff, B=0xffffffffff, XPC=0x7f,
                 GENPC=0x7fffff, CURPC=0x7fffff, IDLE=1, ILLEGAL=0,
                 BSCR=0xf006, SWCR=1)


def check_state(text,before,after,reference_pixels,restored_pixels,*,
                prefix='nse2',checkpoint='18.500000000',card_size=3570,menu_frame=None,
                ram_limit=0x110000):
    menu_frame = MENU_FRAME if menu_frame is None else menu_frame
    if not 0x100004 <= ram_limit <= 0x180000 or ram_limit % 4:
        raise ValueError('invalid backed ARM RAM limit')
    if len(before)!=card_size or after!=before:
        raise ValueError('native UI restoration changed the full SIM card')
    if prefix+'_state: FAIL' in text or 'LUA ERROR' in text or 'Error in' in text:
        raise ValueError('native state observer failed')
    line_start = r'^(?:\[luaengine\] )?' + re.escape(prefix)
    snapshots=re.findall(line_start+r'_state: phase=(saved|restored) (t=[^\r\n]+)\r?$',text,re.M)
    # NSE-2 also uses _state: t=... for model-owned periodic observations.
    # Count the save/load controls, not unrelated records in that namespace.
    if (len(snapshots)!=2 or text.count(prefix+'_state: phase') != 2
            or [s[0] for s in snapshots]!=['saved','restored'] or snapshots[0][1]!=snapshots[1][1]):
        raise ValueError('native architectural snapshot differs or is incomplete')
    field_pairs=[item.split('=',1) for item in snapshots[0][1].split()]
    fields=dict(field_pairs)
    if len(fields) != len(field_pairs):
        raise ValueError('duplicate native snapshot field')
    if set(fields)!={'t','pc','sp','ram','cpu','dsp','data','program'} or fields['t']!=checkpoint:
        raise ValueError('native checkpoint fields/time differ')
    if not re.fullmatch(r'[0-9a-f]{8}(?:,[0-9a-f]{8}){36}',fields['cpu']):
        raise ValueError('ARM register observation is incomplete')
    if any(not re.fullmatch('[0-9a-f]{8}',fields[name]) for name in ('pc','sp','ram','data','program')):
        raise ValueError('native snapshot field encoding differs')
    cpu=fields['cpu'].split(',')
    if cpu[15]!=fields['pc'] or cpu[13]!=fields['sp'] or not 0x200000<=int(fields['pc'],16)<0x400000 or not 0x100000<=int(fields['sp'],16)<ram_limit:
        raise ValueError('native checkpoint lacks consistent own-ROM/RAM architecture')
    dsp_pairs=[item.split(':') for item in fields['dsp'].split(',')]
    dsp=dict(dsp_pairs)
    if (len(dsp) != len(dsp_pairs) or set(dsp)!=DSP_REGISTERS
            or any(not re.fullmatch('[0-9a-f]+',v) for v in dsp.values())
            or any(int(value,16) & ~DSP_MASKS[name] for name,value in dsp.items())):
        raise ValueError('DSP register observation is incomplete or illegal')
    replay=re.findall(line_start+r'_state_replay: reference=(\d+) restored=(\d+) window=2\.7 exact=1\r?$',text,re.M)
    if (len(replay)!=1 or text.count(prefix+'_state_replay:') != 1
            or replay[0][0]!=replay[0][1] or not 0<int(replay[0][0])<16384):
        raise ValueError('native I/O replay is incomplete, empty or over capacity')
    if (text.count(prefix+'_state: completed') != 1
            or len(re.findall(line_start+r'_state: completed=1\r?$',text,re.M)) != 1):
        raise ValueError('native UI replay did not complete once')
    contacts=re.findall(line_start+r'_state_contact: phase=(reference|restored) down=([01])\r?$',text,re.M)
    if (text.count(prefix+'_state_contact:') != 4
            or contacts!=[('reference','1'),('reference','0'),('restored','1'),('restored','0')]):
        raise ValueError('native restored UI lacks physical Menu continuation')
    if any(hashlib.sha256(pixels).hexdigest()!=menu_frame for pixels in (reference_pixels,restored_pixels)):
        raise ValueError('native reference/restored Phone book frame differs')
    return dict(native_ui_restore=True,dsp_registers=36,arm_registers=37,
                io_replay_events=int(replay[0][0]),replay_seconds=2.7,
                native_registration_verified=False,stable_long_run=False)

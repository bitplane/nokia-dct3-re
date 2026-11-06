"""Qualify captured ROM6 initialization, own uploads and selected UI evidence."""
import argparse
import hashlib
from pathlib import Path
import re
import xml.etree.ElementTree as ET
try:
    from tools.nse2_native_state_check import DSP_REGISTERS,DSP_MASKS
except ModuleNotFoundError:
    from nse2_native_state_check import DSP_REGISTERS,DSP_MASKS

MCU_SHA1='c1a0fe95cedb89a92b19654208cc4855e1a4988e'
# Reviewed Messages title and Select/Exit softkeys; omit the animated icon.
MENU_TEXT_SHA256='d2c5ed836eacaf6c04f51bf842658288027d43a962ca097c322eff977c748408'
CALL_REGISTER_TEXT_SHA256='08bca0a0ad8ff2e4ea238d7f753968124e1468d00adf695d57345c8cf36b9915'


def check_composition(xml):
    machine=ET.fromstring(xml).find("machine[@name='nsm3r6']")
    if machine is None:
        raise ValueError('native machine absent from MAME configuration')
    cpu=machine.find("chip[@tag='dsp_c54x:cpu']")
    if cpu is None or cpu.get('clock')!='45500000':
        raise ValueError('unexpected configured native DSP clock')
    devices={entry.get('name') for entry in machine.findall('device_ref')}
    if 'nokia_dsp_c54x' not in devices or devices&{'nokia_dsp_hle','nokia_dsp_staged'}:
        raise ValueError('MAME native backend selection differs')


def check_trace(text, minimum_seconds=24):
    if text.count('rom6_native_composition: hle=0 staged=0 observer_writes=0')!=1:
        raise ValueError('unexpected native DSP composition')
    for marker in ('unsupported', '[lua error]', 'fail incomplete', 'native_suspended=1', 'runtime_hle_handoff'):
        if marker in text.lower():
            raise ValueError(f'native execution rejected: {marker}')
    if text.count('rom6_native_coverage: missing_program_reads=0 observer_writes=0')!=1 or 'rom6_native_unacquired_program:' in text:
        raise ValueError('native program coverage absent or uncaptured padding was read')
    for kind,count in (('reset',2),('upload',2),('resident',1)):
        if len(re.findall(r'rom6_native_entry: kind='+kind+r'\b',text))!=count:
            raise ValueError(f'unexpected native {kind} entry count')
    position=0
    for marker in ('rom6_native_verifier: result0=0000 result1=0006 observer_writes=0',
                   'rom6_native_upload: region=loader2 words=623 entry=0a00 observer_writes=0',
                   'rom6_native_upload: region=installed words=422 entry=2c75 observer_writes=0',
                   'rom6_native_entry: kind=resident',
                   'rom6_native_selftest: command=0d faults=00 flag=c4 observer_writes=0'):
        position=text.find(marker,position)
        if position<0:
            raise ValueError(f'missing ordered native initialization evidence: {marker}')
        position+=len(marker)
    states=re.findall(r'rom6_native_state: kind=(\w+) t=([0-9.]+) pc=[0-9a-f]+ illegal=(\d+) .*? hle=(\d+) observer_writes=0',text)
    if not states or any(illegal!='0' or hle!='0' for _,_,illegal,hle in states):
        raise ValueError('native state absent, illegal, or HLE-owned')
    if not any(kind=='terminal' and float(seconds)>=minimum_seconds for kind,seconds,_,_ in states):
        raise ValueError('native run did not reach the required terminal time')


def check_uploads(image,loader2,installed):
    if hashlib.sha1(image).hexdigest()!=MCU_SHA1:
        raise ValueError('not the pinned NSM-3 v5.31 MCU/PPM input')
    # These are payloads in this handset's MCU, not the acquired mask image.
    if len(loader2)!=623*2 or loader2!=image[0x11ac40:0x11ac40+623*2]:
        raise ValueError('native second loader differs from own flash')
    if len(installed)!=422*2 or installed!=image[0x115128:0x115128+422*2]:
        raise ValueError('native installed program differs from own flash')


def check_menu(frame,expected=MENU_TEXT_SHA256):
    if frame.size!=(84,48):
        raise ValueError('unexpected native menu geometry')
    frame=frame.convert('L')
    text=frame.crop((0,0,84,17)).tobytes()+frame.crop((0,39,84,48)).tobytes()
    if hashlib.sha256(text).hexdigest()!=expected:
        raise ValueError('native menu title/softkeys differ from reviewed pixels')


def check_state(text,reference,restored,reference_io,restored_io):
    samples=re.findall(r'nsm3r6_state: phase=(saved|restored|reference_end|restored_end) ([^\r\n]+)',text)
    if [label for label,_ in samples]!=['saved','reference_end','restored','restored_end']:
        raise ValueError('native replay snapshots missing or duplicated')
    states=dict(samples)
    for state in states.values():
        pairs=[item.split('=',1) for item in state.split()]
        fields=dict(pairs)
        if len(fields)!=len(pairs) or set(fields)!={'t','pc','sp','ram','cpu','dsp','data','program'}:
            raise ValueError('native replay snapshot fields incomplete')
        if not re.fullmatch(r'[0-9a-f]{8}(?:,[0-9a-f]{8}){36}',fields['cpu']):
            raise ValueError('native replay ARM registers incomplete')
        arm=fields['cpu'].split(',')
        if arm[15]!=fields['pc'] or arm[13]!=fields['sp'] or not 0x200000<=int(fields['pc'],16)<0x400000 or not 0x100000<=int(fields['sp'],16)<0x180000:
            raise ValueError('native replay ARM PC/SP inconsistent with own ROM/RAM')
        registers=[item.split(':') for item in fields['dsp'].split(',')]
        dsp=dict(registers)
        if (len(dsp)!=len(registers) or set(dsp)!=DSP_REGISTERS
                or any(not re.fullmatch('[0-9a-f]+',value) or int(value,16)&~DSP_MASKS[name] for name,value in dsp.items())):
            raise ValueError('native replay DSP registers incomplete or illegal')
        if any(not re.fullmatch('[0-9a-f]{8}',fields[name]) for name in ('pc','sp','ram','data','program')):
            raise ValueError('native replay RAM/program observation incomplete')
    if states['saved']!=states['restored'] or states['reference_end']!=states['restored_end']:
        raise ValueError('native replay architecture or endpoint differs')
    if not states['saved'].startswith('t=24.000000000 ') or not states['reference_end'].startswith('t=26.700000000 '):
        raise ValueError('native replay checkpoint or duration differs')
    replay=re.findall(r'nsm3r6_state_replay: reference=(\d+) restored=(\d+) window=2.7 streamed=1 endpoint=1',text)
    if len(replay)!=1 or replay[0][0]!=replay[0][1] or not 0<int(replay[0][0])<1048576:
        raise ValueError('native replay I/O stream is empty, mismatched or over capacity')
    count,last=0,-1.0
    with reference_io.open('rb') as left,restored_io.open('rb') as right:
        for line in left:
            match=re.fullmatch(rb'([0-9]+\.[0-9]{9}):[0-9a-f]{4}:[0-9a-f]{4}:[0-9a-f]{4}\n',line)
            if line!=right.readline() or not match or not last<=float(match[1])<=2.7:
                raise ValueError('native I/O sequence/timing differs or is malformed')
            last=float(match[1]); count+=1
        if right.read(1) or count!=int(replay[0][0]):
            raise ValueError('native I/O stream length differs from capture counts')
    contacts=re.findall(r'nsm3r6_state_contact: phase=(reference|restored) down=([01])',text)
    if contacts!=[('reference','1'),('reference','0'),('restored','1'),('restored','0')] or text.count('nsm3r6_state: completed=1')!=1:
        raise ValueError('native replay lacks physical continuation/completion')
    check_menu(reference)
    check_menu(restored)
    if reference.convert('L').tobytes()!=restored.convert('L').tobytes():
        raise ValueError('native reference/restored full frame differs')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run',type=Path)
    parser.add_argument('flash',type=Path)
    parser.add_argument('--minimum-seconds',type=float,default=24)
    parser.add_argument('--menu',type=Path)
    args=parser.parse_args()
    try:
        text=(args.run/'error.log').read_text(errors='replace')
        if (args.run/'console.log').exists(): text+='\n'+(args.run/'console.log').read_text(errors='replace')
        check_trace(text,args.minimum_seconds)
        check_uploads(args.flash.read_bytes(),(args.run/'rom6_native_loader2.be16').read_bytes(),
                      (args.run/'rom6_native_installed.be16').read_bytes())
        if args.menu:
            from PIL import Image
            with Image.open(args.menu) as frame: check_menu(frame)
    except (OSError,ValueError) as error:
        parser.exit(1,f'ROM6 native check FAIL: {error}\n')
    print('8210 native ROM6 own uploads and initialization PASS'+('; reviewed menu text verified' if args.menu else ''))


if __name__=='__main__':
    main()

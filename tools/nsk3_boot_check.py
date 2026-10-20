"""Qualify conservative NSK-3 cold failure presentation, not usable boot."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsk3_static_check import inspect,PROFILES
from tools.nsk1_phonebook_roundtrip import frame_check

FRAME='ed09afeeadae383b77840654453b183a05e10b2cad5ea5757e8c803cbd6b674b'
INITIALIZED_EEPROM='62d41265e12fe25a0b18d71b5cbf8b415e34ac02c008b01a33cf973e0d26d206'
NATIVE_EEPROM='f38a6e39f434ba557fd9e38419cef6bbcc5eead70211175091fb8254c55b7130'


def check_trace(trace,variant,native=False):
    rows=re.findall(r'nsk3_boot_sample: t=([0-9.]+) pc=([0-9a-f]+) sp=([0-9a-f]+) ctrl=([0-9a-f]+) clock=([0-9a-f]+) fiq=([0-9a-f]+) irq=([0-9a-f]+) shared=([0-9a-f,]+) writes=(\d+) observer_writes=0',trace)
    if (len(rows)!=16 or len(rows)!=trace.count('nsk3_boot_sample: ')
            or [float(row[0]) for row in rows]!=list(range(1,17))):
        raise ValueError('NSK-3 cold samples incomplete')
    shift=0 if variant=='boot4' else -0x10
    for row in rows:
        time,pc,sp,ctrl,clock,fiq,irq,shared,writes=row
        early=native and float(time)==1
        early2=native and float(time)==2
        pc_valid=(0x2ac3a0<=int(pc,16)<0x2ac3c0) if early else 0x2a02f6+shift<=int(pc,16)<=0x2a02fc+shift
        expected_shared=('ffff,ffff,ffff,ffff' if early else '423a,314b,2653,2c50' if early2 else '5589,6b2c,6b2c,6b2c') if native else '0000,ffff,ffff,ffff'
        expected_writes=('56' if early else '72' if early2 else '96') if native else '12'
        if (not pc_valid or sp!=('0010ca9c' if early else '0010c6fc')
                or (ctrl,clock,fiq,irq)!=('05','0c','02' if native else '00','00')
                or shared!=expected_shared or writes!=expected_writes):
            raise ValueError('NSK-3 supervisor/bootstrap boundary differs')
    if '[LUA ERROR]' in trace:
        raise ValueError('NSK-3 observer failed')


def check_upload_handoffs(trace,passes=2):
    """Validate complete uploads independently of the subsequent boot lifecycle."""
    if passes<1:
        raise ValueError('native NSK-3 upload count must be positive')
    rows=re.findall(r'nsk3_loader_handoff: index=(\d+) offset=([0-9a-f]+) value=([0-9a-f]+) mask=([0-9a-f]+) pc=([0-9a-f]+) t=([0-9.]+) observer_writes=0',trace)
    if len(rows)!=64*passes or len(rows)!=trace.count('nsk3_loader_handoff: '):
        raise ValueError('native NSK-3 handoffs incomplete')
    times=[]
    for i,row in enumerate(rows,1):
        index,offset,value,mask,pc,time=row
        odd=i%2==1
        expected_pc='0028cef6' if i%64==0 else ('0028cea8' if odd else '0028ceb8')
        if (int(index)!=i or value!='00000000' or pc!=expected_pc
                or offset!=('000100fc' if odd else '00010100')
                or mask!=('0000ffff' if odd else 'ffff0000')):
            raise ValueError('native NSK-3 alternating handoff differs')
        times.append(float(time))
    if times!=sorted(times) or any(times[i*64-1]>=times[i*64] for i in range(1,passes)):
        raise ValueError('native NSK-3 handoff ordering differs')
    for pc,offset,value,mask in (
            ('0028ce70','0010b840','00040000','ffff0000'),
            ('0028cf0a','0010b844','00000004','0000ffff'),
            ('0028cf0e','0010b844','00000000','ffff0000')):
        found=re.findall(f'nsk3_loader_state: offset={offset} value={value} mask={mask} pc={pc} t=([0-9.]+) observer_writes=0',trace)
        if len(found)!=passes:
            raise ValueError('native NSK-3 identity/verdict stores differ')
        for pass_index,time in enumerate(map(float,found)):
            if pc=='0028ce70' and time>=times[pass_index*64]:
                raise ValueError('native identity not accepted before upload')
            if pc!='0028ce70' and time<=times[pass_index*64+63]:
                raise ValueError('native verdict stored before upload ended')
    return times


def check_native_trace(trace):
    times=check_upload_handoffs(trace)
    native=re.findall(r'nsk3_native_sample: t=([0-9.]+) pc=([0-9a-f]+) releases=(\d+) handoffs=(\d+) observer_writes=0',trace)
    if native!=[(f'{i}.000000','408d','3','128') for i in range(1,17)] or len(native)!=trace.count('nsk3_native_sample: '):
        raise ValueError('native NSK-3 bounded continuation differs')
    resets=re.findall(r'nsk3_reset_request: command=([0-9a-f]+) pc=([0-9a-f]+) t=([0-9.]+) reason=([0-9a-f]+) observer_writes=0',trace)
    if (len(resets)!=1 or len(resets)!=trace.count('nsk3_reset_request: ')
            or resets[0][:2]!=('05','002ac696') or resets[0][3]!='04'
            or not times[63]<float(resets[0][2])<times[64]):
        raise ValueError('native NSK-3 early restart boundary differs')
    if re.search(r'illegal|unsupported|unhandled|fatal',trace,re.I):
        raise ValueError('native NSK-3 execution error observed')


def check_run(source,run,variant,native_roms=None):
    report,image=inspect(source,variant)
    trace=(run/'error.log').read_text()
    native=native_roms is not None
    if native:
        if variant!='boot4':raise ValueError('native instrument requires boot4 ABI')
        for name,digest in (('nse1_rom4_dsp_program.bin','a05a1e96a8c36ec5a47e1ea059d15afa54ca5739'),
                            ('nse1_rom4_dsp_data.bin','024c7f970f4ef754d3e90471de48a167515f930d')):
            if hashlib.sha1((native_roms/name).read_bytes()).hexdigest()!=digest:
                raise ValueError('native NSK-3 compatibility input differs')
        check_native_trace(trace)
        bootstrap=report['catalogue']['entries'][-1]
        start=int(bootstrap['descriptor'],16)-0x200000+12
        expected=image[start:start+372].hex()
        if not re.search(r'nsk3_boot_release: index=1 command=01 args=0100,0300,0000,8000,0001,0001,0200 payload='+expected+r' t=',trace):
            raise ValueError('native NSK-3 first release is not own bootstrap')
    check_trace(trace,variant,native)
    storage=run/'nvram'/('nsk3r4t' if native else ('noki6130' if variant=='boot4' else 'noki6130_1'))
    if (storage/'flash').read_bytes()!=image:
        raise ValueError('NSK-3 own executable flash changed')
    eeprom=(storage/'eeprom').read_bytes()
    if len(eeprom)!=8192 or hashlib.sha256(eeprom).hexdigest()!=(NATIVE_EEPROM if native else INITIALIZED_EEPROM):
        raise ValueError('NSK-3 reviewed erased-source EEPROM initialization differs')
    for time in (5,15):frame_check(run/f'snap/nsk3_boot_{time}.png',FRAME)
    return {'variant':variant,'frame':'CONTACT SERVICE','samples':16,
            'eeprom_sha256':hashlib.sha256(eeprom).hexdigest(),
            'usable_boot_proved':False,'native_compatibility':native,
            'bootstrap_upload_proved':native,'fitted_mask_proved':False}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path)
    parser.add_argument('run',type=Path)
    parser.add_argument('--variant',choices=PROFILES,required=True)
    parser.add_argument('--native-roms',type=Path,help='explicit NSE-1 ROM4 compatibility inputs')
    args=parser.parse_args()
    try:print(check_run(args.source,args.run,args.variant,args.native_roms))
    except (ValueError,OSError) as error:
        print(f'NSK-3 cold boundary failed: {error}',file=sys.stderr)
        return 1
    return 0


if __name__=='__main__':raise SystemExit(main())

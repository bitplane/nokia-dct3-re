"""Qualify wake-without-delivery evidence, not working NAM-2 idle/input."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nam2_c3_checkpoint_check import check_boot,check_pmm


def check_flow(trace):
    if re.search(r'LUA ERROR|Error running autoboot|Fatal error:',trace):
        raise ValueError('execution or observer error')
    for down,time in ((1,'8.95'),(0,'9.15')):
        if trace.count(f'nam2_clock_early_key: down={down} t={time} physical_only=1')!=1:
            raise ValueError('physical wake contact differs')
    wakes=re.findall(r'mad2_sleep: event=wake domain=IRQ fiq=008 irq=001 timer0=[0-9a-f]+ timer1=[0-9a-f]+ t=([0-9.]+) ctrl=0a',trace)
    if len(wakes)!=1 or abs(float(wakes[0])-8.95)>1e-6:
        raise ValueError('physical controller wake is absent or differs')
    rows=re.findall(r'nam2_wake_flow: index=(\d+) pc=([0-9a-f]{8}) lr=([0-9a-f]{8}) sp=([0-9a-f]{8}) cpsr=([0-9a-f]{8}) r0=([0-9a-f]{8}) ctrl=([0-9a-f]{2}) t=([0-9.]+) observer_writes=0',trace)
    if len(rows)!=64 or len(rows)!=trace.count('nam2_wake_flow:'):
        raise ValueError('bounded wake-flow census is incomplete')
    pcs=(0x2f85fa,0x2f860e,0x2f866a,0x2f8688)
    previous=8.95
    for index,(number,pc,lr,sp,cpsr,r0,ctrl,time) in enumerate(rows):
        status=int(cpsr,16);timestamp=float(time)
        if int(number)!=index+1 or int(pc,16)!=pcs[index%4] or ctrl!='0a':
            raise ValueError('supervisor continuation or controller state differs')
        if status&0xff!=0x3f or not 0x100000<=int(sp,16)<0x180000:
            raise ValueError('CPU is not unmasked Thumb System mode with own RAM stack')
        if not previous<timestamp<8.96:
            raise ValueError('wake continuation timing/order differs')
        previous=timestamp
    late=re.findall(r'nam2_idle_sample: t=19\.000000 pc=[0-9a-f]+ ctrl=([0-9a-f]+) fmask=([0-9a-f]+) imask=([0-9a-f]+) fiq=[0-9a-f]+ irq=([0-9a-f]+) nest=([0-9a-f]+)',trace)
    if late!=[('0a','08','a2','01','00')]:
        raise ValueError('late pending-key/nesting boundary differs')
    return {'physical_wake_seconds':8.95,'sampled_supervisor_iterations':16,
            'cpu_irq_fiq_masked':False,'mad2_control':'0x0a',
            'working_idle_or_late_input':False}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image',type=Path);parser.add_argument('run',type=Path)
    args=parser.parse_args()
    try:
        trace=(args.run/'error.log').read_text()
        check_boot(trace)
        check_pmm(args.image.read_bytes(),(args.run/'nvram/nam2c3_1/flash').read_bytes())
        print(check_flow(trace))
    except (OSError,ValueError) as error:
        print(f'NAM-2 wake-flow observation failed: {error}',file=sys.stderr);return 1
    print('Known wake-without-delivery boundary reproduced; not idle/input acceptance')
    return 0


if __name__=='__main__':raise SystemExit(main())

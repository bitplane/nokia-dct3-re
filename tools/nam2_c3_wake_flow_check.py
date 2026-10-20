"""Qualify wake-without-delivery evidence, not working NAM-2 idle/input."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nam2_c3_checkpoint_check import check_boot,check_pmm
from tools.nam2_v521_flash_contract import inspect


def check_accounting(trace,contract):
    if len(contract['stores'])!=2 or any(row['is_mad2_mmio'] for row in contract['stores']):
        raise ValueError('accounting contract is not the two RAM stores')
    rows=re.findall(r'nam2_sleep_accounting: index=(\d+) pc=([0-9a-f]{8}) base=([0-9a-f]{8}) target=([0-9a-f]{8}) value=([0-9a-f]{4}) mask=([0-9a-f]{8}) t=([0-9.]+) observer_writes=0',trace)
    if len(rows)!=16 or len(rows)!=trace.count('nam2_sleep_accounting:'):
        raise ValueError('bounded RAM accounting census is incomplete')
    expected_pc=int(contract['stores'][0]['address'],16)
    previous=0;timestamp=0
    add,reduce=(int(row['address'],16) for row in contract['stores'])
    targets={int(row['target'],16) for row in contract['stores']}
    for index,(number,pc,base,target,value,mask,time) in enumerate(rows,1):
        if int(number)!=index or int(pc,16)!=expected_pc or int(base,16)!=int(contract['base'],16) or int(target,16) not in targets or mask!='0000ffff':
            raise ValueError('sleep-accounting writer/base/byte lane differs')
        want=(previous+contract['increment'])&0xffff if expected_pc==add else previous%contract['divisor']
        if int(value,16)!=want or not timestamp<float(time):
            raise ValueError('sleep-accounting arithmetic or write order differs')
        previous=want;timestamp=float(time)
        expected_pc=reduce if int(pc,16)==add and want>=contract['divisor'] else add
    return {'bounded_ram_stores':16,'target':sorted(targets),'hardware_timer_write':False}


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
    parser.add_argument('--sleep-accounting',action='store_true')
    args=parser.parse_args()
    try:
        trace=(args.run/'error.log').read_text()
        source=args.image.read_bytes()
        contract=inspect(source)
        check_boot(trace)
        check_pmm(source,(args.run/'nvram/nam2c3_1/flash').read_bytes())
        print(check_flow(trace))
        if args.sleep_accounting:print(check_accounting(trace,contract['software_sleep_accounting_writers']))
    except (OSError,ValueError) as error:
        print(f'NAM-2 wake-flow observation failed: {error}',file=sys.stderr);return 1
    print('Known wake-without-delivery boundary reproduced; not idle/input acceptance')
    return 0


if __name__=='__main__':raise SystemExit(main())

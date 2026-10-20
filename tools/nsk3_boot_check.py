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


def check_trace(trace,variant):
    rows=re.findall(r'nsk3_boot_sample: t=([0-9.]+) pc=([0-9a-f]+) sp=([0-9a-f]+) ctrl=([0-9a-f]+) clock=([0-9a-f]+) fiq=([0-9a-f]+) irq=([0-9a-f]+) shared=([0-9a-f,]+) writes=(\d+) observer_writes=0',trace)
    if len(rows)!=16 or [float(row[0]) for row in rows]!=list(range(1,17)):
        raise ValueError('NSK-3 cold samples incomplete')
    shift=0 if variant=='boot4' else -0x10
    for row in rows:
        _,pc,sp,ctrl,clock,fiq,irq,shared,writes=row
        if (not 0x2a02f6+shift<=int(pc,16)<=0x2a02fc+shift
                or sp!='0010c6fc' or (ctrl,clock,fiq,irq)!=('05','0c','00','00')
                or shared!='0000,ffff,ffff,ffff' or writes!='12'):
            raise ValueError('NSK-3 supervisor/bootstrap boundary differs')
    if '[LUA ERROR]' in trace:
        raise ValueError('NSK-3 observer failed')


def check_run(source,run,variant):
    _,image=inspect(source,variant)
    trace=(run/'error.log').read_text()
    check_trace(trace,variant)
    storage=run/'nvram'/('noki6130' if variant=='boot4' else 'noki6130_1')
    if (storage/'flash').read_bytes()!=image:
        raise ValueError('NSK-3 own executable flash changed')
    eeprom=(storage/'eeprom').read_bytes()
    if len(eeprom)!=8192 or hashlib.sha256(eeprom).hexdigest()!=INITIALIZED_EEPROM:
        raise ValueError('NSK-3 reviewed erased-source EEPROM initialization differs')
    for time in (5,15):frame_check(run/f'snap/nsk3_boot_{time}.png',FRAME)
    return {'variant':variant,'frame':'CONTACT SERVICE','samples':16,
            'eeprom_sha256':hashlib.sha256(eeprom).hexdigest(),
            'usable_boot_proved':False,'dsp_completion_proved':False}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path)
    parser.add_argument('run',type=Path)
    parser.add_argument('--variant',choices=PROFILES,required=True)
    args=parser.parse_args()
    try:print(check_run(args.source,args.run,args.variant))
    except (ValueError,OSError) as error:
        print(f'NSK-3 cold boundary failed: {error}',file=sys.stderr)
        return 1
    return 0


if __name__=='__main__':raise SystemExit(main())

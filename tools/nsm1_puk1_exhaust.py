"""Physical NSM-1 PIN1/PUK1 exhaustion and permanent cold card rejection."""
import argparse
from pathlib import Path
import re
import subprocess
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsm1_puk2_exhaust import prepare,execute,check_link,check_run,stored
from tools.nsk3_puk2_exhaust import trace_text,compress_trace
from tools.nsm1_puk_check import recovery,transition
from tools.nsk3_pin_check import check_trace as check_pin
from tools.sim_security_trace_check import ATTEMPTS_OFFSET,AOC_NVRAM_SIZE,statuses
from tools.nsk1_phonebook_roundtrip import frame_check

REJECTED='19aea3a524806cfa5a9a69c445b4760be356da73c71a637b2b377db2dcfa1bde'


def check_permanent(run,previous):
    check_link(run,previous)
    before=stored(previous,'sim_card')
    if before[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET+4]!=bytes((0,3,0,10)) or stored(run,'sim_card')!=before:
        raise ValueError('permanent card changed or wrong credential domain')
    trace=trace_text(run)
    if any(statuses(trace,ins) for ins in (4,0x20,0x24,0x26,0x28,0x2c,0xd6,0xdc)):
        raise ValueError('permanent rejection admitted a credential/storage command')
    keys=(1,2,3,4,5,6,7,9,0x19)+(4,3,2,1,0x19)*2
    rows=re.findall(r'nsm1_puk_contact: key=([0-9a-f]+) down=([01]) ',trace)
    if rows!=[(f'{key:02x}',edge) for key in keys for edge in ('1','0')]:
        raise ValueError('cold rejection physical input control differs')
    for second in (5,24):frame_check(run/f'snap/nsm1_pin_{second}.png',REJECTED)


def chain(pin_runs,wrong,output,*,check_only=False,resume=False):
    if len(pin_runs)!=3:raise ValueError('requires all three physical PIN1 rejections')
    previous=None
    for count,run in zip((3,2,1),pin_runs):
        before=(run/'initial_sim_card.bin').read_bytes()
        if len(before)!=AOC_NVRAM_SIZE or before[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET+4]!=bytes((count,3,10,10)):
            raise ValueError('PIN1 seed extent or preceding retries differ')
        if previous is not None and (before!=stored(previous,'sim_card') or
                (run/'initial_eeprom.bin').read_bytes()!=stored(previous,'eeprom')):
            raise ValueError('physical PIN1 cold chain differs')
        check_pin(trace_text(run),False,prefix='nsm1',tx_limit=4096,attempts_before=count)
        if stored(run,'sim_card')!=transition(before,'exhaust'):
            raise ValueError('physical PIN1 changed unrelated card data')
        check_run(run)
        previous=run
    if (wrong/'initial_sim_card.bin').read_bytes()!=stored(previous,'sim_card') or \
            (wrong/'initial_eeprom.bin').read_bytes()!=stored(previous,'eeprom'):
        raise ValueError('first physical PUK1 rejection predecessor differs')
    recovery(trace_text(wrong),True)
    check_run(wrong)
    if stored(wrong,'sim_card')!=transition(stored(previous,'sim_card'),'wrong'):
        raise ValueError('first wrong PUK1 changed unrelated card data')
    if output.exists() and not (check_only or resume):raise ValueError('requires fresh output, --resume or --check-only')
    previous=wrong
    for remaining in range(8,-1,-1):
        run=output/f'remaining-{remaining}'
        if not check_only and not (resume and run.exists()):
            prepare(previous,run)
            execute(run,'nsm1_puk_wrong.lua')
        check_link(run,previous)
        before=stored(previous,'sim_card')
        recovery(trace_text(run),True,attempts_before=before[ATTEMPTS_OFFSET+2])
        if stored(run,'sim_card')!=transition(before,'wrong') or stored(run,'sim_card')[ATTEMPTS_OFFSET+2]!=remaining:
            raise ValueError('PUK1 exhaustion skipped a retry or changed unrelated card data')
        if not check_only:compress_trace(run)
        print(f'PASS: NSM-1 physical PUK1 rejection, remaining={remaining}, complete cold card checked',flush=True)
        previous=run
    frame_check(previous/'snap/nsm1_pin_24.png',REJECTED)
    run=output/'permanent-cold'
    if not check_only and not (resume and run.exists()):
        prepare(previous,run)
        execute(run,'nsm1_puk_wrong.lua')
    check_permanent(run,previous)
    if not check_only:compress_trace(run)
    print('PASS: NSM-1 permanent PUK1 rejection survives cold boot and physical input without card mutation',flush=True)
    return run


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('first','second','blocked','wrong','output'):parser.add_argument(name,type=Path)
    parser.add_argument('--check-only',action='store_true')
    parser.add_argument('--resume',action='store_true')
    args=parser.parse_args()
    try:
        chain([args.first.resolve(),args.second.resolve(),args.blocked.resolve()],args.wrong.resolve(),
              args.output.resolve(),check_only=args.check_only,resume=args.resume)
    except (OSError,EOFError,ValueError,RuntimeError,KeyError,subprocess.SubprocessError) as error:
        parser.exit(1,f'NSM-1 PUK1 exhaustion: FAIL: {error}\n')


if __name__=='__main__':main()

"""Physical NSK-3 PIN1 exhaustion, PUK recovery and cold new-PIN authentication."""
import argparse
from pathlib import Path
import subprocess
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsk3_cold_input import prepare
from tools.nsk3_pin_check import check_trace, PIN, PHONE_PROMPT
from tools.nsk3_puk_check import check_recovery, PUK
from tools.nsk3_puk2_exhaust import check_link, compress_trace, execute, stored, trace_text
from tools.nsk3_static_check import inspect
from tools.nsk3_boot_check import check_upload_handoffs
from tools.sim_security_trace_check import ATTEMPTS_OFFSET,AOC_NVRAM_SIZE,NVRAM_SIZE
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.make_sim_card_profile import make_profile


def check_attempt(run,previous,image,count):
    check_link(run,previous,image)
    before=stored(previous,'sim_card')
    if before[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET+4]!=bytes((count,3,10,10)):
        raise ValueError('requires the preceding PIN1 retry state')
    check_trace(trace_text(run),False,attempts_before=count)
    expected=bytearray(before)
    expected[ATTEMPTS_OFFSET]=count-1
    if stored(run,'sim_card')!=expected:
        raise ValueError('physical PIN1 rejection changed more than its retry counter')
    frame_check(run/'snap/nsk3_pin_5.png',PIN)


def chain(source,seed,output,*,check_only=False,resume=False):
    _,image=inspect(source,'boot4')
    check_trace(trace_text(seed),False)
    check_upload_handoffs(trace_text(seed),1)
    # Reuse the existing strict canonical laboratory-card serializer contract.
    expected=bytearray(make_profile(True).ljust(AOC_NVRAM_SIZE,b'\xff'))
    expected[NVRAM_SIZE:NVRAM_SIZE+3]=bytes.fromhex('00f110')
    expected[ATTEMPTS_OFFSET]=2
    if stored(seed,'sim_card')!=expected or stored(seed,'flash')!=image:
        raise ValueError('first physical PIN1 rejection or own executable differs')
    if output.exists() and not (check_only or resume):
        raise ValueError('requires fresh output, --resume or --check-only')

    def run_from(previous,name,script):
        run=output/name
        if not check_only and not (resume and run.exists()):
            prepare(source,previous,run)
            execute(run,script)
        return run

    previous=seed
    for count in (2,1):
        run=run_from(previous,f'remaining-{count-1}','nsk3_pin_input.lua')
        check_attempt(run,previous,image,count)
        if not check_only:compress_trace(run)
        print(f'PASS: physical PIN1 rejection, remaining={count-1}, exact cold card transition',flush=True)
        previous=run
    run=run_from(previous,'puk-recovery','nsk3_puk_input.lua')
    check_link(run,previous,image)
    check_recovery(trace_text(run),stored(run,'sim_card'),initial=stored(previous,'sim_card'))
    frame_check(run/'snap/nsk3_pin_5.png',PUK)
    frame_check(run/'snap/nsk3_pin_24.png',PHONE_PROMPT)
    if not check_only:compress_trace(run)
    previous=run
    run=run_from(previous,'new-pin-cold','nsk3_changed_pin_input.lua')
    check_link(run,previous,image)
    check_trace(trace_text(run),True,b'4321')
    if stored(run,'sim_card')!=stored(previous,'sim_card'):
        raise ValueError('cold new-PIN authentication changed persistent card')
    frame_check(run/'snap/nsk3_pin_5.png',PIN)
    frame_check(run/'snap/nsk3_pin_17.png',PHONE_PROMPT)
    if not check_only:compress_trace(run)
    print('PASS: physically exhausted PIN1, PUK recovery and cold new-PIN authentication',flush=True)
    return run


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','seed','output'):parser.add_argument(name,type=Path)
    parser.add_argument('--check-only',action='store_true')
    parser.add_argument('--resume',action='store_true')
    args=parser.parse_args()
    try:
        chain(args.source.resolve(),args.seed.resolve(),args.output.resolve(),
              check_only=args.check_only,resume=args.resume)
    except (OSError,EOFError,ValueError,KeyError,subprocess.SubprocessError) as error:
        parser.exit(1,f'NSK-3 PIN1 exhaustion: FAIL: {error}\n')


if __name__=='__main__':main()

"""Continue physical NSK-3 PUK1 retries from the qualified first rejection."""
import argparse
from pathlib import Path
import re
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsk3_cold_input import prepare
from tools.nsk3_puk_check import check_recovery
from tools.nsk3_puk2_exhaust import check_link, compress_trace, execute, stored, trace_text
from tools.nsk3_static_check import inspect
from tools.nsk3_boot_check import check_upload_handoffs
from tools.sim_security_trace_check import ATTEMPTS_OFFSET, statuses
from tools.nsk1_phonebook_roundtrip import frame_check

REJECTED='19aea3a524806cfa5a9a69c445b4760be356da73c71a637b2b377db2dcfa1bde'


def check_permanent(run, previous, image):
    check_link(run, previous, image)
    before=stored(previous, 'sim_card')
    if before[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET+4]!=bytes((0,3,0,10)) or stored(run,'sim_card')!=before:
        raise ValueError('permanently blocked card changed or wrong security domain')
    trace=trace_text(run)
    if any(statuses(trace,ins) for ins in (4,0x20,0x24,0x26,0x28,0x2c,0xd6,0xdc)):
        raise ValueError('permanent card admitted credential or storage mutation')
    keys=(1,2,3,4,5,6,7,9,0x19)+(4,3,2,1,0x19)*2
    contacts=re.findall(r'nsk3_puk_contact: key=([0-9a-f]+) down=([01]) ',trace)
    if contacts!=[(f'{key:02x}',edge) for key in keys for edge in ('1','0')]:
        raise ValueError('permanent rejection physical input control differs')
    if re.search(r'illegal|unsupported|unhandled|fatal|\[LUA ERROR\]',trace,re.I):
        raise ValueError('permanent rejection execution error')
    for time in (5,24):
        frame_check(run/f'snap/nsk3_pin_{time}.png',REJECTED)


def chain(source, seed, output, *, check_only=False, resume=False):
    _, image=inspect(source, 'boot4')
    check_recovery(trace_text(seed), stored(seed, 'sim_card'), True)
    check_upload_handoffs(trace_text(seed), 1)
    if stored(seed, 'flash')!=image:
        raise ValueError('seed executable differs from own image')
    if output.exists() and not (check_only or resume):
        raise ValueError('requires a fresh output, --resume or --check-only')
    previous=seed
    for remaining in range(8, -1, -1):
        run=output/f'remaining-{remaining}'
        if not check_only and not (resume and run.exists()):
            prepare(source, previous, run)
            execute(run, 'nsk3_wrong_puk_input.lua')
        check_link(run, previous, image)
        check_recovery(trace_text(run), stored(run, 'sim_card'), True,
                       stored(previous, 'sim_card'))
        if stored(run, 'sim_card')[ATTEMPTS_OFFSET+2]!=remaining:
            raise ValueError('PUK1 retry chain skipped a transition')
        if not check_only:
            compress_trace(run)
        print(f'PASS: physical wrong PUK1, remaining={remaining}, whole-card cold transition checked', flush=True)
        previous=run
    frame_check(previous/'snap/nsk3_pin_24.png',REJECTED)
    run=output/'permanent-cold'
    if not check_only and not (resume and run.exists()):
        prepare(source,previous,run)
        execute(run,'nsk3_wrong_puk_input.lua')
    check_permanent(run,previous,image)
    if not check_only:
        compress_trace(run)
    print('PASS: permanent PUK1 cold rejection, unchanged card and no credential admission',flush=True)
    return run


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'seed', 'output'):
        parser.add_argument(name, type=Path)
    parser.add_argument('--resume', action='store_true')
    parser.add_argument('--check-only', action='store_true')
    args=parser.parse_args()
    try:
        chain(args.source.resolve(), args.seed.resolve(), args.output.resolve(),
              check_only=args.check_only, resume=args.resume)
    except (OSError, EOFError, ValueError, KeyError, subprocess.SubprocessError) as error:
        parser.exit(1, f'NSK-3 PUK1 exhaustion: FAIL: {error}\n')


if __name__=='__main__':
    main()

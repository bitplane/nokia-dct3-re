"""Run native backend compare-edge conformance, not a physical SCU test."""
import argparse
from pathlib import Path
import subprocess
import tempfile

PASS='C54x compare boundary: PASS replacement=1 armed_restore=1 exact_time=1 ack_ports=1 mask_retains=1 periodic=1 raw_zero=1 cancellation=1 reset_abort=1 hagar_restore=1 mfi_clock_restore=1 mfi_irq_owner=1 reload_phase=1 mfi_request_restore=1 physical_latch_claim=0'
DONE='Fatal error: C54x compare boundary tests complete'


def check_result(text,code):
    lines=text.splitlines()
    if code!=3 or lines.count(PASS)!=1 or lines.count(DONE)!=1 or lines[-1:]!=[DONE]:
        raise ValueError('missing complete compare-boundary conformance result')
    if any(line.startswith('Fatal error:') and line!=DONE for line in lines):
        raise ValueError('unexpected compare-boundary failure')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mame',type=Path,required=True)
    parser.add_argument('--output-root',type=Path,default=Path('tmp'))
    args=parser.parse_args()
    args.output_root.mkdir(parents=True,exist_ok=True)
    run=Path(tempfile.mkdtemp(prefix='serial-compare-',dir=args.output_root.resolve()))
    command=[str(args.mame.resolve()),'tms54scu','-noreadconfig','-video','none',
        '-sound','none','-nothrottle','-joystickprovider','none','-midiprovider','none',
        '-keyboardprovider','none','-mouseprovider','none','-lightgunprovider','none','-seconds_to_run','1']
    print(f'Compare boundary evidence: {run}',flush=True)
    with (run/'console.log').open('w') as output:
        result=subprocess.run(command,cwd=run,stdout=output,stderr=subprocess.STDOUT,timeout=60)
    try:
        check_result((run/'console.log').read_text(),result.returncode)
    except ValueError as error:
        parser.exit(1,f'Compare boundary FAIL: {error}\n')
    print(PASS)


if __name__=='__main__': main()

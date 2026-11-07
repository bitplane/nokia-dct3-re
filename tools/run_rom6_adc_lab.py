"""Run explicit ADC-pin tone/silence candidates, never an RF/camping gate."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import subprocess

ROOT=Path(__file__).resolve().parents[1]
if __package__ in (None,''): sys.path.insert(0,str(ROOT))
from tools.run_rom6_native import prepare,execute
from tools.rom6_adc_lab_check import check


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--machine',choices=('nsm3adc','nsm3ad0'),required=True)
    parser.add_argument('--seconds',type=int,default=6)
    args=parser.parse_args()
    if not 3<=args.seconds<=30: parser.error('ADC-pin trials are bounded to 3..30 seconds')
    root=ROOT/'tmp/rom6-adc-lab'
    root.mkdir(exist_ok=True)
    run=Path(tempfile.mkdtemp(prefix=args.machine+'-',dir=root))
    print(f'ADC lab evidence: {run}',flush=True)
    try:
        prepare(run,(ROOT/'roms/noki8210/8210_5.31ppm_c.fls').read_bytes(),
                (ROOT/'roms/noki8210/8210 virgin eeprom 003d0000.fls').read_bytes(),machine=args.machine)
        directory,text=execute(run,'primary','rom6_adc_lab.lua',args.seconds,ROOT/'mame/mame',machine=args.machine)
        report=check(text,args.machine=='nsm3adc')
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        parser.exit(1,f'ADC lab rejected: {error}\n')
    report.update(machine=args.machine,seconds=args.seconds,native_initialization_verified=True,
                  mame_sha256=hashlib.sha256((ROOT/'mame/mame').read_bytes()).hexdigest())
    (run/'sample-prefix.json').write_text(json.dumps(report,indent=2)+'\n')
    if not report['transport_fault_free']:
        parser.exit(1,'ADC lab rejected: raw transport fault; no decoder qualification\n')
    print('ADC lab native initialization completed; receiver result is not yet qualified')


if __name__=='__main__': main()

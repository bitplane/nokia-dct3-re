"""Execute actual card FDN APDU/access/save-state conformance in MAME."""
import argparse
from pathlib import Path
import subprocess
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.sim_fdn_profile import extend
from tools.sim_security_trace_check import ENABLED_OFFSET,CHV_OFFSET,padded_pin


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run',type=Path);parser.add_argument('--base-card',type=Path,required=True)
    parser.add_argument('--mame',type=Path,default=Path('mame/mame'))
    args=parser.parse_args()
    try:
        base=bytearray(args.base_card.read_bytes())
        if base[:1600]!=b'\xff'*1600 or base[CHV_OFFSET:CHV_OFFSET+16]!=padded_pin('1234')+padded_pin('5678'):
            raise ValueError('blank ADN/default-credential laboratory base required')
        base[ENABLED_OFFSET]=1
        binary=args.mame.resolve();run=args.run.resolve();run.mkdir(parents=True,exist_ok=False)
        for model,readable in (('simfdnt',False),('simfdnrt',True)):
            directory=run/model;card=directory/'nvram'/model/'card'
            card.parent.mkdir(parents=True)
            initial=extend(bytes(base),readable);card.write_bytes(initial)
            command=[str(binary),model,'-noreadconfig','-video','none','-sound','none',
                     '-nothrottle','-seconds_to_run','4','-nvram_directory','nvram','-cfg_directory','cfg']
            result=subprocess.run(command,cwd=directory,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=90)
            (directory/'console.log').write_text(result.stdout)
            if result.returncode or f'SIM FDN conformance: PASS readable_invalid={int(readable)} ' not in result.stdout:
                raise ValueError(f'{model} executable conformance failed')
            expected=bytearray(initial);expected[3578:3610]=b'A'*32
            if card.read_bytes()!=expected:raise ValueError('conformance changed other persistent card bytes')
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        parser.exit(1,f'SIM FDN conformance failed: {error}\n')
    print('SIM FDN strict/readable-invalid APDU conformance and exact persistent record PASS; handset_claim=0')
    return 0


if __name__=='__main__':raise SystemExit(main())

"""Exhaust PIN2 through physical Settings input and predecessor-linked cold runs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.run_noki6210_acceptance import build_command,run_retained_security,events,check_registration
from tools.noki6210_pin2_change_check import check,check_frames
from tools.noki6210_puk_check import profile


def run_chain(root,output,mame):
    output.mkdir(parents=True,exist_ok=False)
    source=output/'wrong-1'
    with (output/'first-launch.log').open('w') as console:
        subprocess.run([sys.executable,str(root/'tools/run_noki6210_acceptance.py'),
                        str(source),'--scenario','pin2-reject','--coherent-cell','--mame',str(mame)],
                       cwd=root,stdout=console,stderr=subprocess.STDOUT,check=True,timeout=180)
    first=json.loads((source/'acceptance.json').read_text())
    if first.get('passed') is not True or first.get('scenario')!='pin2-reject':
        raise ValueError('physical PIN2 rejection predecessor is absent')
    text=events(source/'error.log')
    card=(source/'nvram/npe3hle/sim_card').read_bytes()
    check(text,card,old_pin='9999',rejected=True)
    check_registration(text,card,channel_header='1202')
    check_frames(source/'snap',rejected=True)
    launches=[first['launch']['invocation_id']]
    command=build_command(root,mame,'npe3hle','pin2_wrong_input',84)
    for attempt in (2,3):
        before=(source/'nvram/npe3hle/sim_card').read_bytes()
        expected=bytearray(profile(3,True))
        expected[3517]=4-attempt
        if before!=bytes(expected):
            raise ValueError('PIN2 predecessor differs from exact expected card')
        target=output/f'wrong-{attempt}'
        text,initial,launch=run_retained_security(source,target,command,
            root/'tools/noki6210_pin2_wrong_input.lua',84,dict(os.environ))
        if initial!=before or launch['invocation_id'] in launches:
            raise ValueError('cold PIN2 launch identity or initial card differs')
        launches.append(launch['invocation_id'])
        final=(target/'nvram/npe3hle/sim_card').read_bytes()
        check(text,final,old_pin='9999',rejected=True,remaining=3-attempt)
        check_registration(text,final,preserved_location=True,channel_header='1202')
        check_frames(target/'snap',rejected=True,blocked=attempt==3)
        source=target
    report={'passed':True,'launches':launches,'pin2_retries':0,
            'card_sha256':hashlib.sha256((source/'nvram/npe3hle/sim_card').read_bytes()).hexdigest(),
            'scope':'physical PIN2 exhaustion only; PUK2 recovery is not qualified'}
    (output/'acceptance.json').write_text(json.dumps(report,indent=2)+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory',type=Path)
    parser.add_argument('--mame',type=Path,default=Path('mame/mame'))
    args=parser.parse_args()
    try:
        run_chain(Path(__file__).resolve().parents[1],args.run_directory.resolve(),args.mame.resolve())
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        print(f'6210 PIN2 exhaustion FAIL: {error}',file=sys.stderr)
        return 1
    print(f'6210 PIN2 exhaustion PASS: {args.run_directory}')
    return 0


if __name__=='__main__':
    raise SystemExit(main())

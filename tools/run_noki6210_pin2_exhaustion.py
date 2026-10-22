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
from tools.noki6210_pin2_change_check import check,check_frames,check_puk2,check_puk2_frames
from tools.noki6210_puk_check import profile


def run_chain(root,output,mame,recover=False,wrong_puk_first=False):
    if wrong_puk_first and not recover:
        raise ValueError('wrong PUK2 control requires recovery')
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
    if recover:
        stages=([('wrong-puk2','puk2_wrong_input',88)] if wrong_puk_first else [])
        stages += [('puk2-recovery','puk2_input',88),('new-pin2-cold','puk2_reauth_input',84)]
        for name,script,seconds in stages:
            target=output/name
            before=(source/'nvram/npe3hle/sim_card').read_bytes()
            text,initial,launch=run_retained_security(source,target,command,
                root/f'tools/noki6210_{script}.lua',seconds,dict(os.environ))
            if initial!=before or launch['invocation_id'] in launches:
                raise ValueError('PUK2 recovery launch identity or initial card differs')
            launches.append(launch['invocation_id'])
            final=(target/'nvram/npe3hle/sim_card').read_bytes()
            if name in ('wrong-puk2','puk2-recovery'):
                rejected=name=='wrong-puk2'
                check_puk2(text,initial,final,rejected=rejected,
                           puk_attempts=9 if wrong_puk_first and not rejected else 10)
                check_puk2_frames(target/'snap',rejected=rejected)
            else:
                check(text,final,old_pin='2468',new_pin='5678')
                check_frames(target/'snap')
            check_registration(text,final,preserved_location=True,channel_header='1202')
            source=target
    report={'passed':True,'launches':launches,'pin2_retries':3 if recover else 0,
            'wrong_puk2_cold_recovery':wrong_puk_first,
            'card_sha256':hashlib.sha256((source/'nvram/npe3hle/sim_card').read_bytes()).hexdigest(),
            'scope':('physical PIN2 exhaustion, PUK2 recovery and cold replacement-PIN2 use' if recover
                     else 'physical PIN2 exhaustion only; this run does not exercise PUK2 recovery')}
    (output/'acceptance.json').write_text(json.dumps(report,indent=2)+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory',type=Path)
    parser.add_argument('--mame',type=Path,default=Path('mame/mame'))
    parser.add_argument('--recover',action='store_true',help='recover with PUK2 and cold-use the installed PIN2')
    parser.add_argument('--wrong-puk-first',action='store_true',help='consume one PUK2 attempt before cold recovery')
    args=parser.parse_args()
    if args.wrong_puk_first and not args.recover:
        parser.error('--wrong-puk-first requires --recover')
    label='6210 PUK2 recovery' if args.recover else '6210 PIN2 exhaustion'
    try:
        run_chain(Path(__file__).resolve().parents[1],args.run_directory.resolve(),args.mame.resolve(),args.recover,args.wrong_puk_first)
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        print(f'{label} FAIL: {error}',file=sys.stderr)
        return 1
    print(f'{label} PASS: {args.run_directory}')
    return 0


if __name__=='__main__':
    raise SystemExit(main())

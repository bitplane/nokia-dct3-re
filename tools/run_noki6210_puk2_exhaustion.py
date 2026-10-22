"""Physical PUK2 exhaustion with checked cold predecessors and restartable evidence."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.run_noki6210_acceptance import build_command,run_retained_security,events,check_output,check_registration
from tools.noki6210_staged_check import verify
from tools.noki6210_pin2_change_check import (check,check_frames,check_puk2,check_puk2_frames,
                                            check_permanent_puk2,check_permanent_puk2_frames)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def storage(path):
    return {str(p.relative_to(path/'nvram')):digest(p)
            for p in sorted((path/'nvram').rglob('*')) if p.is_file()}


def retained(source,target,command,script,seconds,resume):
    before=(source/'nvram/npe3hle/sim_card').read_bytes()
    hashes=storage(source)
    if not (resume and target.exists()):
        return run_retained_security(source,target,command,script,seconds,dict(os.environ))
    launch=json.loads((target/'launch.json').read_text())
    expected=list(command)
    expected[expected.index('-autoboot_script')+1]=str(script)
    expected[expected.index('-seconds_to_run')+1]=str(seconds)
    if (launch['command']!=expected or launch['run_directory']!=str(target) or
            launch['initial_storage']!=hashes or launch['source_card_sha256']!=hashlib.sha256(before).hexdigest() or
            launch['script_sha256']!=digest(script)):
        raise ValueError('retained PUK2 launch provenance differs')
    check_output((target/'console.log').read_text(errors='replace'))
    text=events(target/'error.log')
    verify(text,runtime=True,selftest=True)
    return text,before,launch


def run_chain(root,output,mame,resume=False):
    output.mkdir(parents=True,exist_ok=resume)
    blocked=output/'blocked'
    if not (resume and blocked.exists()):
        with (output/'blocked-launch.log').open('w') as console:
            subprocess.run([sys.executable,str(root/'tools/run_noki6210_pin2_exhaustion.py'),
                            str(blocked),'--mame',str(mame)],cwd=root,stdout=console,
                           stderr=subprocess.STDOUT,check=True,timeout=600)
    report=json.loads((blocked/'acceptance.json').read_text())
    if report.get('passed') is not True or report.get('pin2_retries')!=0:
        raise ValueError('physical PIN2 exhaustion predecessor is absent')
    launches=set(report['launches'])
    if len(launches)!=3:
        raise ValueError('PIN2 predecessor must have three distinct launches')
    observed=set()
    for attempt in (1,2,3):
        source=blocked/f'wrong-{attempt}'
        if attempt==1:
            first=json.loads((source/'acceptance.json').read_text())
            launch=first['launch']
            if (first.get('passed') is not True or first.get('scenario')!='pin2-reject' or
                    launch['input_sha256']['mame']!=digest(mame) or
                    launch['input_sha256']['script']!=digest(root/'tools/noki6210_pin2_wrong_input.lua')):
                raise ValueError('first PIN2 predecessor input provenance differs')
        else:
            _,_,launch=retained(blocked/f'wrong-{attempt-1}',source,
                build_command(root,mame,'npe3hle','pin2_wrong_input',84),
                root/'tools/noki6210_pin2_wrong_input.lua',84,True)
        if launch['invocation_id'] not in launches or launch['invocation_id'] in observed:
            raise ValueError('PIN2 predecessor launch chain differs')
        observed.add(launch['invocation_id'])
        text=events(source/'error.log')
        check_output((source/'console.log').read_text(errors='replace'))
        verify(text,runtime=True,selftest=True)
        card=(source/'nvram/npe3hle/sim_card').read_bytes()
        check(text,card,old_pin='9999',rejected=True,remaining=3-attempt)
        check_frames(source/'snap',rejected=True,blocked=attempt==3)
        check_registration(text,card,preserved_location=attempt>1,channel_header='1202')
    source=blocked/'wrong-3'
    command=build_command(root,mame,'npe3hle','puk2_wrong_input',88)
    for attempt in range(1,11):
        target=output/f'wrong-puk2-{attempt:02}'
        text,initial,launch=retained(source,target,command,
            root/'tools/noki6210_puk2_wrong_input.lua',88,resume)
        if launch['invocation_id'] in launches:
            raise ValueError('reused PUK2 launch identity')
        launches.add(launch['invocation_id'])
        card=(target/'nvram/npe3hle/sim_card').read_bytes()
        check_puk2(text,initial,card,rejected=True,puk_attempts=11-attempt)
        check_puk2_frames(target/'snap',rejected=True,permanent=attempt==10)
        check_registration(text,card,preserved_location=True,channel_header='1202')
        print(f'PUK2 wrong attempt {attempt}: exact cold transition PASS',flush=True)
        source=target
    # A correct-PUK pattern after permanent exhaustion must not restore PIN2.
    target=output/'permanent-cold'
    text,initial,launch=retained(source,target,command,
        root/'tools/noki6210_puk2_input.lua',88,resume)
    if launch['invocation_id'] in launches:
        raise ValueError('reused permanent-control launch identity')
    launches.add(launch['invocation_id'])
    card=(target/'nvram/npe3hle/sim_card').read_bytes()
    check_permanent_puk2(text,initial,card)
    check_permanent_puk2_frames(target/'snap')
    check_registration(text,card,preserved_location=True,channel_header='1202')
    (output/'acceptance.json').write_text(json.dumps({
        'passed':True,'launches':sorted(launches),'pin2_retries':0,'puk2_retries':0,
        'card_sha256':digest(target/'nvram/npe3hle/sim_card'),
        'scope':'permanent PUK2 loss; PIN1 registration and physical Menu remain usable'},indent=2)+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory',type=Path)
    parser.add_argument('--mame',type=Path,default=Path('mame/mame'))
    parser.add_argument('--resume',action='store_true')
    args=parser.parse_args()
    try:
        run_chain(Path(__file__).resolve().parents[1],args.run_directory.resolve(),args.mame.resolve(),args.resume)
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        print(f'6210 permanent PUK2 FAIL: {error}',file=sys.stderr)
        return 1
    print(f'6210 permanent PUK2 PASS: {args.run_directory}')
    return 0


if __name__=='__main__':
    raise SystemExit(main())

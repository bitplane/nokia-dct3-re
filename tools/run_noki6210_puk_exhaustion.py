"""Physical 6210 PUK exhaustion through exact predecessor-linked cold runs."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import uuid

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.run_noki6210_acceptance import build_command,events,check_output
from tools.noki6210_staged_check import verify
from tools.noki6210_puk_check import profile,check_blocked,check_wrong_puk,check_permanent,check_permanent_retry,check_frames,check_rgb_frame,PERMANENT_FRAME,PERMANENT_INPUT_FRAMES,FRAMES
from tools.noki6210_security_retry_check import check_frames as retry_frames
from tools.run_sip_handset_gate import verify_handset_health


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run_chain(root,output,mame,check_only=False,resume=False):
    if not check_only and not resume:
        output.mkdir(parents=True,exist_ok=False)
        command=[sys.executable,str(root/'tools/run_noki6210_acceptance.py'),
                 str(output/'blocked'),'--scenario','security-block','--coherent-cell','--mame',str(mame)]
        with (output/'blocked-launch.log').open('w') as console:
            subprocess.run(command,cwd=root,stdout=console,stderr=subprocess.STDOUT,check=True,timeout=180)
    source=output/'blocked'
    report=json.loads((source/'acceptance.json').read_text())
    if report.get('scenario')!='security-block' or report.get('passed') is not True:
        raise ValueError('physically PIN-exhausted predecessor acceptance is absent')
    check_output((source/'console.log').read_text(errors='replace'))
    verify_handset_health((source/'console.log').read_text(errors='replace'))
    check_blocked(events(source/'error.log'),(source/'nvram/npe3hle/sim_card').read_bytes())
    retry_frames(source/'snap',wrong_only=True)
    check_frames(source/'snap',('6210_pin_blocked.png',))
    launches={report['launch']['invocation_id']}
    for attempt in range(1,13):
        permanent=attempt>=11
        active=attempt==12
        target=output/('permanent-input' if active else 'permanent-cold' if permanent else f'wrong-{attempt:02}')
        script='puk_permanent_retry_input' if active else 'puk_permanent_input' if permanent else 'puk_wrong_input'
        seconds=41 if active else 18 if permanent else 37
        command=build_command(root,mame,'npe3hle',script,seconds)
        command.extend(['-joystickprovider','none','-midiprovider','none'])
        before=(source/'nvram/npe3hle/sim_card').read_bytes()
        expected=profile(puk_attempts=0 if permanent else 11-attempt)
        if before!=expected:
            raise ValueError(f'attempt {attempt}: predecessor retry/whole-card state differs')
        retained=check_only or (resume and target.exists())
        if not retained:
            target.mkdir()
            shutil.copytree(source/'nvram',target/'nvram')
            shutil.copytree(source/'cfg',target/'cfg')
            (target/'initial_sim_card.bin').write_bytes(before)
            launch={'invocation_id':str(uuid.uuid4()),'command':command,'machine':'npe3hle',
                    'source_directory':str(source),'source_card_sha256':digest(source/'nvram/npe3hle/sim_card'),
                    'mame_sha256':digest(mame),'script_sha256':digest(root/f'tools/noki6210_{script}.lua'),
                    'initial_storage':{str(p.relative_to(target/'nvram')):digest(p)
                                       for p in sorted((target/'nvram').rglob('*')) if p.is_file()}}
            (target/'launch.json').write_text(json.dumps(launch,indent=2)+'\n')
            with (target/'console.log').open('w') as console:
                subprocess.run(command,cwd=target,stdout=console,stderr=subprocess.STDOUT,
                               check=True,timeout=180)
        initial=(target/'initial_sim_card.bin').read_bytes()
        launch=json.loads((target/'launch.json').read_text())
        source_storage={str(p.relative_to(source/'nvram')):digest(p)
                        for p in sorted((source/'nvram').rglob('*')) if p.is_file()}
        if (initial!=before or launch['invocation_id'] in launches or launch['command']!=command or
                launch['source_directory']!=str(source) or
                launch['source_card_sha256']!=hashlib.sha256(before).hexdigest() or
                launch['initial_storage']!=source_storage or
                launch['mame_sha256']!=digest(mame) or
                launch['script_sha256']!=digest(root/f'tools/noki6210_{script}.lua')):
            raise ValueError('cold launch identity/input provenance differs')
        launches.add(launch['invocation_id'])
        check_output((target/'console.log').read_text(errors='replace'))
        verify_handset_health((target/'console.log').read_text(errors='replace'))
        text=events(target/'error.log')
        verify(text,runtime=True,selftest=True)
        final=(target/'nvram/npe3hle/sim_card').read_bytes()
        if active:
            check_permanent_retry(text,initial,final)
            for name,expected_hash in PERMANENT_INPUT_FRAMES.items():
                check_rgb_frame(target/'snap'/name,expected_hash)
        elif permanent:
            check_permanent(text,initial,final)
            check_rgb_frame(target/'snap/6210_puk_permanent.png',FRAMES['6210_puk_prompt.png'])
        else:
            check_wrong_puk(text,initial,final,attempt)
            check_frames(target/'snap',('6210_puk_prompt.png','6210_puk_new_pin.png','6210_puk_confirm.png'))
            if attempt==10:
                check_rgb_frame(target/'snap/6210_puk_result.png',PERMANENT_FRAME)
            else:
                check_frames(target/'snap',('6210_puk_result.png',))
        if (source/'nvram/npe3hle/sim_card').read_bytes()!=before:
            raise ValueError('cold continuation changed its predecessor')
        label='permanent physical-input control' if active else 'permanent cold observation' if permanent else f'physical wrong PUK {attempt}/10'
        print(f'PASS: 6210 {label}',flush=True)
        source=target
    return {'passed':True,'launches':len(launches),'wrong_puk_attempts':10,
            'permanent_card_sha256':hashlib.sha256(final).hexdigest(),
            'native_rf_or_speech':False}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output',type=Path)
    parser.add_argument('--mame',type=Path)
    parser.add_argument('--check-only',action='store_true')
    parser.add_argument('--resume',action='store_true',help='revalidate complete predecessors before continuing missing runs')
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]
    try:
        output=args.output.resolve()
        result=run_chain(root,output,(args.mame or root/'mame/mame').resolve(),args.check_only,args.resume)
        if not args.check_only:
            (output/'acceptance.json').write_text(json.dumps(result,indent=2)+'\n')
    except (OSError,ValueError,KeyError,RuntimeError,subprocess.CalledProcessError,subprocess.TimeoutExpired) as error:
        parser.exit(1,f'6210 PUK exhaustion: FAIL: {error}\n')
    print(f'PASS: 6210 cold PUK exhaustion {result}')


if __name__=='__main__':
    main()

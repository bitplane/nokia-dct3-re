"""Fresh 6210 FDN listed/unlisted/Off controls against the laboratory HLE peer."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.run_noki6210_acceptance import build_command,run_retained_security,check_registration
from tools.noki6210_fdn_dial_check import check,check_frames
from tools.noki6210_fdn_check import check as check_disable,check_frames as disable_frames
from tools.noki6210_fdn_record_check import expected_card
from tools.nsk3_phonebook_check import expected_record


def run_chain(root,output,mame):
    output.mkdir(parents=True,exist_ok=False)
    prepared=output/'prepared'
    with (output/'prepare-launch.log').open('w') as console:
        subprocess.run([sys.executable,str(root/'tools/run_noki6210_fdn_records.py'),
                        str(prepared),'--mame',str(mame)],cwd=root,stdout=console,
                       stderr=subprocess.STDOUT,check=True,timeout=600)
    report=json.loads((prepared/'acceptance.json').read_text())
    if report.get('passed') is not True or len(set(report['launches']))!=3:
        raise ValueError('fresh physical FDN save/cold-read preparation is absent')
    saved=prepared/'read'
    if (saved/'nvram/npe3hle/sim_card').read_bytes()!=expected_card(True):
        raise ValueError('dial controls require the exact physically saved A/12345 card')
    ids=set(report['launches'])
    phases=[]
    for mode,script,seconds in (('listed','fdn_dial',46),('denied','fdn_dial_unlisted',46),
                                ('disable','fdn_disable',66),('off','fdn_dial_unlisted',46)):
        source=output/'disable' if mode=='off' else saved
        target=output/mode
        command=build_command(root,mame,'npe3hle',script,seconds)
        before=(source/'nvram/npe3hle/sim_card').read_bytes()
        text,initial,launch=run_retained_security(source,target,command,
            root/f'tools/noki6210_{script}.lua',seconds,dict(os.environ))
        if initial!=before or launch['invocation_id'] in ids:
            raise ValueError('FDN dial control input or launch identity differs')
        ids.add(launch['invocation_id'])
        card=(target/'nvram/npe3hle/sim_card').read_bytes()
        if mode=='disable':
            check_disable(text,card,2,record=expected_record())
            disable_frames(target/'snap',2)
        else:
            check(text,initial,card,mode)
            check_frames(target/'snap',mode)
        check_registration(text,card,preserved_location=True,channel_header='1202')
        phases.append({'mode':mode,'launch':launch})
    (output/'acceptance.json').write_text(json.dumps({'passed':True,'launches':sorted(ids),
        'phases':phases,'native_dsp_complete':False,'speech_tested':False,
        'scope':'listed and Off-unlisted HLE call signaling; On-unlisted locally rejected'},indent=2)+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory',type=Path)
    parser.add_argument('--mame',type=Path,default=Path('mame/mame'))
    args=parser.parse_args()
    try:
        run_chain(Path(__file__).resolve().parents[1],args.run_directory.resolve(),args.mame.resolve())
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        print(f'6210 FDN dial controls FAIL: {error}',file=sys.stderr)
        return 1
    print(f'6210 FDN dial controls PASS: {args.run_directory}; native speech unproved')
    return 0


if __name__=='__main__':
    raise SystemExit(main())

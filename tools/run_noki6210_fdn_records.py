"""Physical 6210 FDN record lifecycles with exact cold-storage checks."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.run_noki6210_acceptance import build_command,run_retained_security,events,check_registration
from tools.noki6210_fdn_check import check as check_activation,check_frames as activation_frames
from tools.noki6210_fdn_record_check import check,check_frames


def run_chain(root,output,mame,edit=False,erase=False,wrong_write_first=False):
    if sum((edit,erase,wrong_write_first))>1:
        raise ValueError('record lifecycle qualification variants are separate')
    output.mkdir(parents=True,exist_ok=False)
    source=output/'activated'
    with (output/'activation-launch.log').open('w') as console:
        subprocess.run([sys.executable,str(root/'tools/run_noki6210_acceptance.py'),str(source),
                        '--scenario','fdn-enable','--coherent-cell','--mame',str(mame)],
                       cwd=root,stdout=console,stderr=subprocess.STDOUT,check=True,timeout=180)
    first=json.loads((source/'acceptance.json').read_text())
    if first.get('passed') is not True or first.get('scenario')!='fdn-enable':
        raise ValueError('physical FDN activation predecessor is absent')
    text=events(source/'error.log')
    check_activation(text,(source/'nvram/npe3hle/sim_card').read_bytes(),0)
    activation_frames(source/'snap',0)
    ids=[first['launch']['invocation_id']]
    command=build_command(root,mame,'npe3hle','fdn_write',86)
    stages=([('wrong','fdn_write_wrong',72)] if wrong_write_first else [])
    stages += [('write','fdn_write',86),('read','fdn_read',68)]
    if edit:
        stages += [('edit','fdn_edit',88),('edited-read','fdn_read',68)]
    if erase:
        stages += [('cancel','fdn_cancel',80),('erase','fdn_erase',80),('empty','fdn_empty',62)]
    for mode,script,seconds in stages:
        target=output/mode
        before=(source/'nvram/npe3hle/sim_card').read_bytes()
        text,initial,launch=run_retained_security(source,target,command,
            root/f'tools/noki6210_{script}.lua',seconds,dict(os.environ))
        if initial!=before or launch['invocation_id'] in ids:
            raise ValueError('FDN record predecessor or cold launch differs')
        ids.append(launch['invocation_id'])
        card=(target/'nvram/npe3hle/sim_card').read_bytes()
        check(text,initial,card,mode,pin2_attempts=2 if mode=='write' and wrong_write_first else 3)
        check_frames(target/'snap',mode)
        check_registration(text,card,preserved_location=True,channel_header='1202')
        source=target
    (output/'acceptance.json').write_text(json.dumps({'passed':True,'launches':ids,'edited':edit,'erased':erase,
        'rejected_write_cold_recovery':wrong_write_first,
        'scope':('physical FDN rejected PIN2 Add, cold correct-PIN2 save and cold Details' if wrong_write_first else
                 'physical FDN save, erase cancellation, confirmed erase and cold empty list' if erase else
                 'physical FDN activation, protected save/edit and cold Details' if edit else
                 'physical FDN activation, protected save and cold Details')+'; no dial-permission claim'},indent=2)+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory',type=Path)
    parser.add_argument('--mame',type=Path,default=Path('mame/mame'))
    lifecycle=parser.add_mutually_exclusive_group()
    lifecycle.add_argument('--edit',action='store_true',help='edit the saved number and cold-read it again')
    lifecycle.add_argument('--erase',action='store_true',help='cancel erase, confirm erase and cold-read the empty list')
    lifecycle.add_argument('--wrong-write-first',action='store_true',help='reject wrong-PIN2 Add then cold-retry correctly')
    args=parser.parse_args()
    try:
        run_chain(Path(__file__).resolve().parents[1],args.run_directory.resolve(),args.mame.resolve(),args.edit,args.erase,args.wrong_write_first)
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        print(f'6210 FDN records FAIL: {error}',file=sys.stderr)
        return 1
    print(f'6210 FDN records PASS: {args.run_directory}')
    return 0


if __name__=='__main__':
    raise SystemExit(main())

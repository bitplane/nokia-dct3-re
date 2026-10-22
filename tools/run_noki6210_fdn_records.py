"""Fresh physical 6210 FDN activation, protected save and cold Details readback."""
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


def run_chain(root,output,mame,edit=False):
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
    stages=[('write','fdn_write',86),('read','fdn_read',68)]
    if edit:
        stages += [('edit','fdn_edit',88),('edited-read','fdn_read',68)]
    for mode,script,seconds in stages:
        target=output/mode
        before=(source/'nvram/npe3hle/sim_card').read_bytes()
        text,initial,launch=run_retained_security(source,target,command,
            root/f'tools/noki6210_{script}.lua',seconds,dict(os.environ))
        if initial!=before or launch['invocation_id'] in ids:
            raise ValueError('FDN record predecessor or cold launch differs')
        ids.append(launch['invocation_id'])
        card=(target/'nvram/npe3hle/sim_card').read_bytes()
        check(text,initial,card,mode)
        check_frames(target/'snap',mode)
        check_registration(text,card,preserved_location=True,channel_header='1202')
        source=target
    (output/'acceptance.json').write_text(json.dumps({'passed':True,'launches':ids,'edited':edit,
        'scope':('physical FDN activation, protected save/edit and cold Details' if edit else
                 'physical FDN activation, protected save and cold Details')+'; no dial-permission claim'},indent=2)+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory',type=Path)
    parser.add_argument('--mame',type=Path,default=Path('mame/mame'))
    parser.add_argument('--edit',action='store_true',help='edit the saved number and cold-read it again')
    args=parser.parse_args()
    try:
        run_chain(Path(__file__).resolve().parents[1],args.run_directory.resolve(),args.mame.resolve(),args.edit)
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        print(f'6210 FDN records FAIL: {error}',file=sys.stderr)
        return 1
    print(f'6210 FDN records PASS: {args.run_directory}')
    return 0


if __name__=='__main__':
    raise SystemExit(main())

"""Qualify PIN1 checking prerequisite, physical change and cold new-PIN acceptance."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
from PIL import Image

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.run_nse2_native import prepare_native
from tools.process_runner import run_stage
from tools.nse2_pin1_change_check import check_change, check_request
from tools.nse2_sim_check import verify_security_attempt


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory',type=Path)
    parser.add_argument('--mame',type=Path)
    parser.add_argument('--request-lifecycle',action='store_true',help='physical On/cold-verify/Off/cold-bypass instead of CHANGE')
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]
    run=args.run_directory.resolve()
    mame=(args.mame or root/'mame/mame').resolve()
    common=[sys.executable,str(root/'tools/run_nse2_native.py')]
    options=['--mame',str(mame),'--record-checksum-only','--nominal-vbat','--insert-sim']
    try:
        run.mkdir(parents=True,exist_ok=False)
        seed=run/'seed'
        command=common+[str(seed)]+options+['--security-code','--phone-retry-zero','--seconds','18']
        with (run/'seed.log').open('w') as stream:
            run_stage(command,root,stream)
        source=seed/'nvram/nse2r4v/sim_card'
        base=source.read_bytes()
        results=[]
        stages=((False,'on'),(True,'off'),(False,'cold-off')) if args.request_lifecycle else ((False,'disabled'),(True,'change'))
        for enabled,name in stages:
            target=run/name
            report=prepare_native(root,target,record_checksum=True,nominal_vbat=True,insert_sim=True,phone_retry_zero=True)
            before=bytearray(base)
            if not args.request_lifecycle:
                before[3520]=int(enabled)
            (target/'initial_sim_card.bin').write_bytes(before)
            (target/'nvram/nse2r4v/sim_card').write_bytes(before)
            script=target/'physical_input.lua'
            if args.request_lifecycle:
                setting=f'_G.nse2_pin_request_mode={json.dumps(name)}'
                fixture=root/'tools/nse2_pin_request_fixture.lua'
            else:
                setting=f'_G.nse2_pin1_change_enabled={str(enabled).lower()}'
                fixture=root/'tools/nse2_pin1_change_fixture.lua'
            script.write_text(f'{setting}\ndofile({json.dumps(str(fixture))})\n')
            command=[str(arg).replace(str(seed),str(target)) for arg in json.loads((seed/'inputs.json').read_text())['command']]
            command[command.index('-seconds_to_run')+1]='30'
            command[command.index('-autoboot_script')+1]=str(script)
            report.update(command=command,card_source=str(source),source_sha256=hashlib.sha256(base).hexdigest(),
                          card_sha256=hashlib.sha256(before).hexdigest(),explicit_pin1_enabled=enabled,
                          mame_sha256=json.loads((seed/'inputs.json').read_text())['mame_sha256'])
            (target/'inputs.json').write_text(json.dumps(report,indent=2)+'\n')
            with (target/'console.log').open('w') as stream:
                run_stage(command,target,stream)
            console=(target/'console.log').read_text(errors='replace')
            if 'LUA ERROR' in console or 'Error in' in console:
                raise ValueError('PIN1 fixture failed')
            text=(target/'error.log').read_text(errors='replace')
            after=(target/'nvram/nse2r4v/sim_card').read_bytes()
            with Image.open(target/'snap/nse2_pin1_change_result.png') as frame:
                if frame.size!=(84,48):
                    raise ValueError('PIN1 frame geometry differs')
                result=(check_request(text,before,after,frame.convert('L').tobytes(),mode=name) if args.request_lifecycle
                        else check_change(text,before,after,frame.convert('L').tobytes(),enabled=enabled))
            with Image.open(target/'snap/nse2_security_result.png') as frame:
                verify_security_attempt(text,frame.convert('L').tobytes(),retry_zero=True)
            (target/'acceptance.json').write_text(json.dumps(result,indent=2)+'\n')
            results.append(result)
            print(f'NSE-2 PIN1 {name} PASS',flush=True)
            if args.request_lifecycle:
                source=target/'nvram/nse2r4v/sim_card'
                base=after
                if name=='on':
                    command=common+[str(run/'cold-on')]+options+['--pin-enabled','--pin-card',str(source),'--pin-action','verify','--seconds','14']
                    with (run/'cold-on.log').open('w') as stream:
                        run_stage(command,root,stream)
                    source=run/'cold-on/nvram/nse2r4v/sim_card'
                    if source.read_bytes()!=base:
                        raise ValueError('cold enabled PIN acceptance modified card storage')
        if args.request_lifecycle:
            (run/'acceptance.json').write_text(json.dumps(dict(stages=results,cold_pin_request_persistence=True,
                native_registration_verified=False),indent=2)+'\n')
            return
        changed=run/'change/nvram/nse2r4v/sim_card'
        command=common+[str(run/'cold')]+options+['--pin-enabled','--pin-card',str(changed),'--pin-action','reauth','--seconds','14']
        with (run/'cold.log').open('w') as stream:
            run_stage(command,root,stream)
        if (run/'cold/nvram/nse2r4v/sim_card').read_bytes()!=changed.read_bytes():
            raise ValueError('cold PIN1 acceptance modified card storage')
        (run/'acceptance.json').write_text(json.dumps(dict(stages=results,cold_new_pin_accepted=True,
            native_registration_verified=False),indent=2)+'\n')
        print('NSE-2 cold changed PIN1 PASS',flush=True)
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        parser.exit(1,f'NSE-2 PIN1 change FAIL: {error}\n')


if __name__=='__main__':
    main()

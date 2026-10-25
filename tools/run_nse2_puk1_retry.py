"""Qualify wrong PUK1 persistence, cold wait and reduced-retry recovery."""
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
from tools.nse2_puk1_check import check_wrong, check_cold, input_remaining


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory',type=Path)
    parser.add_argument('--mame',type=Path)
    parser.add_argument('--exhaust',action='store_true',help='exhaust all PUK1 retries and test cold permanent refusal')
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]
    run=args.run_directory.resolve()
    mame=(args.mame or root/'mame/mame').resolve()
    common=[sys.executable,str(root/'tools/run_nse2_native.py')]
    options=['--mame',str(mame),'--record-checksum-only','--nominal-vbat','--insert-sim','--pin-enabled']
    try:
        run.mkdir(parents=True,exist_ok=False)
        source=None
        for i in range(1,4):
            target=run/f'wrong-pin{i}'
            command=common+[str(target)]+options+['--pin-action','wrong','--seconds','14']
            if source:
                command+=['--pin-card',str(source)]
            with (run/f'wrong-pin{i}.log').open('w') as stream:
                run_stage(command,root,stream)
            source=target/'nvram/nse2r4v/sim_card'
        before=source.read_bytes()
        if input_remaining(before)!=10:
            raise ValueError('organic PIN blocking did not retain ten PUK1 attempts')
        if args.exhaust:
            from tools.nse2_puk1_exhaustion import exhaust
            checked=exhaust(root,run/'exhaust',source,mame)
            (run/'acceptance.json').write_text(json.dumps(checked,indent=2)+'\n')
            return
        target=run/'wrong-puk'
        report=prepare_native(root,target,record_checksum=True,nominal_vbat=True,insert_sim=True)
        (target/'initial_sim_card.bin').write_bytes(before)
        (target/'nvram/nse2r4v/sim_card').write_bytes(before)
        command=[str(arg).replace(str(run/'wrong-pin3'),str(target)) for arg in
                 json.loads((run/'wrong-pin3/inputs.json').read_text())['command']]
        command[command.index('-seconds_to_run')+1]='28'
        command[command.index('-autoboot_script')+1]=str(root/'tools/nse2_puk_wrong_fixture.lua')
        report.update(command=command,card_source=str(source),card_sha256=hashlib.sha256(before).hexdigest(),
                      mame_sha256=json.loads((run/'wrong-pin3/inputs.json').read_text())['mame_sha256'])
        (target/'inputs.json').write_text(json.dumps(report,indent=2)+'\n')
        with (target/'console.log').open('w') as stream:
            run_stage(command,target,stream)
        console=(target/'console.log').read_text(errors='replace')
        if 'LUA ERROR' in console or 'Error in' in console:
            raise ValueError('wrong PUK1 fixture failed')
        source=target/'nvram/nse2r4v/sim_card'
        after=source.read_bytes()
        with Image.open(target/'snap/nse2_puk_probe_1.png') as prompt, Image.open(target/'snap/nse2_puk_probe_5.png') as result:
            checked=check_wrong((target/'error.log').read_text(),before,after,prompt.convert('L').tobytes(),result.convert('L').tobytes())
        (target/'acceptance.json').write_text(json.dumps(checked,indent=2)+'\n')
        print('NSE-2 wrong PUK1 PASS; remaining=9',flush=True)
        target=run/'cold'
        command=common+[str(target)]+options+['--pin-card',str(source),'--pin-action','blocked','--puk-attempts','9','--seconds','14']
        with (run/'cold.log').open('w') as stream:
            run_stage(command,root,stream)
        with Image.open(target/'snap/nse2_7.png') as blocked, Image.open(target/'snap/nse2_pin_blocked.png') as prompt:
            check_cold((target/'error.log').read_text(),after,(target/'nvram/nse2r4v/sim_card').read_bytes(),
                       blocked.convert('L').tobytes(),prompt.convert('L').tobytes())
        source=target/'nvram/nse2r4v/sim_card'
        command=common+[str(run/'recover')]+options+['--pin-card',str(source),'--pin-action','puk','--puk-attempts','9','--seconds','28']
        with (run/'recover.log').open('w') as stream:
            run_stage(command,root,stream)
        source=run/'recover/nvram/nse2r4v/sim_card'
        command=common+[str(run/'reauth')]+options+['--pin-card',str(source),'--pin-action','reauth','--seconds','14']
        with (run/'reauth.log').open('w') as stream:
            run_stage(command,root,stream)
        if (run/'reauth/nvram/nse2r4v/sim_card').read_bytes()!=source.read_bytes():
            raise ValueError('cold new PIN authentication modified card storage')
        (run/'acceptance.json').write_text(json.dumps(dict(wrong_puk1_rejected=True,
            cold_retry_preserved=9,recovered_from_reduced_retries=True,cold_new_pin_verified=True,
            native_registration_verified=False),indent=2)+'\n')
        print('NSE-2 cold PUK1 retry/recovery/new PIN PASS',flush=True)
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        parser.exit(1,f'NSE-2 PUK1 retry FAIL: {error}\n')


if __name__=='__main__':
    main()

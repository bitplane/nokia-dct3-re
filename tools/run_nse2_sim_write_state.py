"""Qualify pending native SIM UPDATE replay and independent cold storage readback."""
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
from tools.nse2_sim_write_state_check import check_write_state
from tools.nse2_sim_check import verify_security_attempt, verify_phonebook_save, verify_sms_save, verify_sim_reads


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory',type=Path)
    parser.add_argument('--mame',type=Path)
    parser.add_argument('--sms-partial',action='store_true',help='restore after 60/176 SMS body bytes instead of before ADN body')
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]
    run=args.run_directory.resolve()
    mame=(args.mame or root/'mame/mame').resolve()
    common=[sys.executable,str(root/'tools/run_nse2_native.py')]
    options=['--mame',str(mame),'--record-checksum-only','--nominal-vbat','--insert-sim','--security-code','--phone-retry-zero']
    try:
        run.mkdir(parents=True,exist_ok=False)
        seed=run/'seed'
        with (run/'seed.log').open('w') as stream:
            run_stage(common+[str(seed)]+options+['--seconds','18'],root,stream)
        target=run/'restore'
        report=prepare_native(root,target,record_checksum=True,nominal_vbat=True,insert_sim=True,phone_retry_zero=True)
        source=seed/'nvram/nse2r4v/sim_card'
        before=source.read_bytes()
        (target/'initial_sim_card.bin').write_bytes(before)
        (target/'nvram/nse2r4v/sim_card').write_bytes(before)
        original=json.loads((seed/'inputs.json').read_text())
        command=[str(arg).replace(str(seed),str(target)) for arg in original['command']]
        command[command.index('-seconds_to_run')+1]='30'
        command[command.index('-autoboot_script')+1]=str(root/'tools'/('nse2_sms_write_state.lua' if args.sms_partial else 'nse2_sim_write_state.lua'))
        command+=['-state_directory',str(target/'sta')]
        report.update(command=command,card_source=str(source),card_sha256=hashlib.sha256(before).hexdigest(),
                      mame_sha256=original['mame_sha256'],save_contact_hold_seconds=.07,sms_partial=args.sms_partial)
        (target/'inputs.json').write_text(json.dumps(report,indent=2)+'\n')
        with (target/'console.log').open('w') as stream:
            run_stage(command,target,stream)
        console=(target/'console.log').read_text(errors='replace')
        if 'LUA ERROR' in console or 'Error in' in console:
            raise ValueError('pending SIM state fixture failed')
        text=(target/'error.log').read_text(errors='replace')
        source=target/'nvram/nse2r4v/sim_card'
        after=source.read_bytes()
        with Image.open(target/'snap/nse2_sim_state_reference.png') as reference, Image.open(target/'snap/nse2_sim_state_restored.png') as restored:
            if reference.size!=(84,48) or restored.size!=(84,48):
                raise ValueError('pending SIM result frame geometry differs')
            result=check_write_state(text,(target/'nse2_sim_state_events.log').read_text(),before,after,
                                     reference.convert('L').tobytes(),restored.convert('L').tobytes(),sms=args.sms_partial)
            save_check=verify_sms_save if args.sms_partial else verify_phonebook_save
            save_check(text.split('nse2_sim_state: phase=restored')[0],after,reference.convert('L').tobytes())
        with Image.open(target/'snap/nse2_security_result.png') as frame:
            verify_security_attempt(text,frame.convert('L').tobytes(),retry_zero=True)
        with Image.open(target/'snap/nse2_7.png') as frame:
            verify_sim_reads(text,frame.convert('L').tobytes())
        state=target/'sta/nse2r4v/nse2_sim_pending.sta'
        if not state.is_file() or state.stat().st_size==0:
            raise ValueError('pending SIM save artifact absent')
        result['state_sha256']=hashlib.sha256(state.read_bytes()).hexdigest()
        reload_options=['--sms-reload',str(source)] if args.sms_partial else ['--post-auth-menu','--phonebook-reload',str(source)]
        command=common+[str(run/'cold')]+options+reload_options+['--seconds','30']
        with (run/'cold.log').open('w') as stream:
            run_stage(command,root,stream)
        if (run/'cold/nvram/nse2r4v/sim_card').read_bytes()!=after:
            raise ValueError('cold readback changed restored card storage')
        result['cold_draft_readback' if args.sms_partial else 'cold_contact_readback']=True
        (run/'acceptance.json').write_text(json.dumps(result,indent=2)+'\n')
        print('NSE-2 pending SIM UPDATE replay and cold readback PASS',flush=True)
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        parser.exit(1,f'NSE-2 pending SIM state FAIL: {error}\n')


if __name__=='__main__':
    main()

"""Qualify pending physical SIM authentication replay and cold credentials/retries."""
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
from tools.nse2_pin_state_check import check_pin_state,check_puk_state
from tools.nse2_sim_check import verify_pin_boot,verify_puk_recovery


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory',type=Path)
    parser.add_argument('--mame',type=Path)
    modes=parser.add_mutually_exclusive_group()
    modes.add_argument('--last-retry',action='store_true',help='physically reach one retry, then restore its blocking VERIFY')
    modes.add_argument('--puk',action='store_true',help='physically block PIN, then restore successful UNBLOCK and cold-authenticate its new PIN')
    modes.add_argument('--buffered-body',action='store_true',help='restore five PIN body bytes buffered in SIMI before card delivery')
    modes.add_argument('--status-pending',action='store_true',help='restore committed PIN retries and queued unread status bytes')
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]
    run=args.run_directory.resolve()
    mame=(args.mame or root/'mame/mame').resolve()
    common=[sys.executable,str(root/'tools/run_nse2_native.py')]
    options=['--mame',str(mame),'--record-checksum-only','--nominal-vbat','--insert-sim','--pin-enabled','--seconds','14']
    try:
        run.mkdir(parents=True,exist_ok=False)
        seed=run/'seed'
        with (run/'seed.log').open('w') as stream:
            run_stage(common+[str(seed)]+options,root,stream)
        if args.last_retry or args.puk:
            for index in range(1,4 if args.puk else 3):
                predecessor=seed/'nvram/nse2r4v/sim_card'
                seed=run/f'wrong{index}'
                with (run/f'wrong{index}.log').open('w') as stream:
                    run_stage(common+[str(seed)]+options+['--pin-card',str(predecessor),'--pin-action','wrong'],root,stream)
        target=run/'restore'
        report=prepare_native(root,target,record_checksum=True,nominal_vbat=True,insert_sim=True)
        before=(seed/'nvram/nse2r4v/sim_card').read_bytes()
        (target/'initial_sim_card.bin').write_bytes(before)
        (target/'nvram/nse2r4v/sim_card').write_bytes(before)
        original=json.loads((seed/'inputs.json').read_text())
        command=[str(arg).replace(str(seed),str(target)) for arg in original['command']]
        command[command.index('-autoboot_script')+1]=str(root/'tools'/('nse2_puk_write_state.lua' if args.puk else 'nse2_pin_block_state.lua' if args.last_retry else 'nse2_pin_status_state.lua' if args.status_pending else 'nse2_pin_buffered_state.lua' if args.buffered_body else 'nse2_pin_write_state.lua'))
        if args.puk:
            command[command.index('-seconds_to_run')+1]='28'
        command+=['-state_directory',str(target/'sta')]
        report.update(command=command,card_sha256=hashlib.sha256(before).hexdigest(),
                      card_source=str(seed/'nvram/nse2r4v/sim_card'),mame_sha256=original['mame_sha256'],last_retry=args.last_retry,puk=args.puk,buffered_body=args.buffered_body,status_pending=args.status_pending)
        (target/'inputs.json').write_text(json.dumps(report,indent=2)+'\n')
        with (target/'console.log').open('w') as stream:
            run_stage(command,target,stream)
        console=(target/'console.log').read_text(errors='replace')
        if 'LUA ERROR' in console or 'Error in' in console:
            raise ValueError('PIN state observer failed')
        text=(target/'error.log').read_text(errors='replace')
        card=target/'nvram/nse2r4v/sim_card'
        after=card.read_bytes()
        def pixels(name):
            with Image.open(target/'snap'/name) as image:
                if image.size!=(84,48):
                    raise ValueError('PIN frame geometry differs')
                return image.convert('L').tobytes()
        inputs=(text,(target/'nse2_sim_state_events.log').read_text(),before,after,
                pixels('nse2_sim_state_reference.png'),pixels('nse2_sim_state_restored.png'))
        result=check_puk_state(*inputs) if args.puk else check_pin_state(*inputs,last_retry=args.last_retry,buffered_body=args.buffered_body,status_pending=args.status_pending)
        reference=text.split('nse2_sim_state: phase=restored')[0]
        if args.puk:
            verify_puk_recovery(reference,before,after,pixels('nse2_puk_probe_1.png'),pixels('nse2_sim_state_reference.png'))
        else:
            verify_pin_boot(reference,after,pixels('nse2_pin_1.png'),pixels('nse2_sim_state_reference.png'),
                            pixels('nse2_pin_3.png'),action='wrong',initial_card=before)
        state=target/'sta/nse2r4v/nse2_sim_pending.sta'
        if not state.is_file() or not state.stat().st_size:
            raise ValueError('PIN checkpoint artifact absent')
        result['state_sha256']=hashlib.sha256(state.read_bytes()).hexdigest()
        with (run/'cold.log').open('w') as stream:
            cold_options=['--pin-action','reauth'] if args.puk else ['--pin-action','blocked'] if args.last_retry else []
            run_stage(common+[str(run/'cold')]+options+['--pin-card',str(card)]+cold_options,root,stream)
        if (run/'cold/nvram/nse2r4v/sim_card').read_bytes()!=after:
            raise ValueError('cold PIN wait changed restored card')
        result['cold_retry_persistence']=True
        if args.puk:
            result['cold_replacement_pin_authenticated']=True
        (run/'acceptance.json').write_text(json.dumps(result,indent=2)+'\n')
        print('NSE-2 pending PUK replay and cold replacement-PIN authentication PASS' if args.puk else
              'NSE-2 pending PIN replay and cold retry persistence PASS')
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        parser.exit(1,f'NSE-2 PIN state FAIL: {error}\n')


if __name__=='__main__':
    main()

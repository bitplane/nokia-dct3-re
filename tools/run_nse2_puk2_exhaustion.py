"""Exhaust PUK2 through linked physical-input processes, then cold-test refusal."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nse2_sim_check import verify_pin2_input
from tools.sim_security_trace_check import ATTEMPTS_OFFSET,AOC_NVRAM_SIZE
from tools.process_runner import run_stage


def check_transition(before,after,remaining):
    if len(before)!=AOC_NVRAM_SIZE or len(after)!=AOC_NVRAM_SIZE:
        raise ValueError('PUK2 exhaustion requires complete predecessor cards')
    if before[ATTEMPTS_OFFSET+1]!=0 or before[ATTEMPTS_OFFSET+3]!=remaining+1:
        raise ValueError('PUK2 exhaustion skipped its predecessor retry')
    expected=bytearray(before)
    expected[ATTEMPTS_OFFSET+3]=remaining
    if after!=expected:
        raise ValueError('PUK2 exhaustion modified unrelated storage')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory',type=Path)
    parser.add_argument('--blocked-card',required=True,type=Path)
    parser.add_argument('--mame',type=Path)
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]
    run=args.run_directory.resolve()
    card=args.blocked_card.resolve()
    try:
        initial=card.read_bytes()
        verify_pin2_input(initial,blocked=True)
        if len(initial)!=AOC_NVRAM_SIZE or initial[ATTEMPTS_OFFSET+3]!=10:
            raise ValueError('requires a complete PIN2-blocked card with ten PUK2 attempts')
        run.mkdir(parents=True,exist_ok=False)
        stages=[]
        for index in range(1,12):
            permanent=index==11
            target=run/('cold' if permanent else f'wrong{index:02}')
            before=card.read_bytes()
            command=[sys.executable,str(root/'tools/run_nse2_native.py'),str(target),
                     '--mame',str((args.mame or root/'mame/mame').resolve()),
                     '--record-checksum-only','--nominal-vbat','--insert-sim','--security-code',
                     '--phone-retry-zero','--pin2-action','permanent' if permanent else 'puk2-wrong',
                     '--pin2-card',str(card),'--seconds','30']
            with (run/f'{target.name}.log').open('w') as stream:
                run_stage(command,root,stream)
            metadata=json.loads((target/'inputs.json').read_text())['pin2_card']
            if metadata['sha256']!=hashlib.sha256(before).hexdigest() or metadata['source']!=str(card):
                raise ValueError('cold predecessor source/hash differs')
            next_card=target/'nvram/nse2r4v/sim_card'
            after=next_card.read_bytes()
            result=json.loads((target/'acceptance.json').read_text())['pin2_change']
            if permanent:
                if after!=before or not result.get('permanent_pin2_loss') or not result.get('physical_menu_continuation'):
                    raise ValueError('cold permanent refusal/continuation differs')
            else:
                check_transition(before,after,10-index)
                if result.get('puk2_attempts_remaining')!=10-index or not result.get('wrong_puk2_rejected'):
                    raise ValueError('physical PUK2 rejection result differs')
            stages.append(dict(stage=target.name,source=str(card),source_sha256=metadata['sha256'],
                               puk2_attempts=after[ATTEMPTS_OFFSET+3]))
            (run/'stages.json').write_text(json.dumps(stages,indent=2)+'\n')
            print(f'NSE-2 {target.name} PASS; PUK2 attempts={after[ATTEMPTS_OFFSET+3]}',flush=True)
            card=next_card
        (run/'acceptance.json').write_text(json.dumps(dict(physical_wrong_puk2_attempts=10,
            cold_permanent_pin2_loss=True,physical_menu_continuation=True,stable_long_boot=False),indent=2)+'\n')
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        parser.exit(1,f'NSE-2 PUK2 exhaustion FAIL: {error}\n')


if __name__=='__main__':
    main()

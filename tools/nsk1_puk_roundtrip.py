"""Physical NSK-1 PIN/PUK retry, recovery and permanent-block cold lifecycles."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.make_sim_card_profile import make_profile
from tools.nsk1_rom4_compare import prepare
from tools.nsk1_security_compare import run_machine,check_phase
from tools.nsk1_phonebook_roundtrip import copy_persistent,frame_check
from tools.nsk1_pin_roundtrip import check_card,check_contacts,PIN_PROMPT,PHONE_PROMPT,ACCEPTED,CODE_ERROR
from tools.sim_security_trace_check import statuses,CHV_OFFSET,ATTEMPTS_OFFSET,AOC_NVRAM_SIZE


PUK_PROMPT = '2944a1b1d362846853f92be495ece23c7986528e9fb519fcae638a8f1a2023b8'
NEW_PIN_PROMPT = '836967ace63acc78a441c69f89272159ec24faa2c4dec6983d436edb9f223bc7'
NEW_PIN = b'4321\xff\xff\xff\xff'
WRONG_PUK = '642bb9aa764fc34ad31c1eb32611724b5c3902d56c127cfe5e0a447e37d58f7c'
PERMANENT_BLOCK = '19aea3a524806cfa5a9a69c445b4760be356da73c71a637b2b377db2dcfa1bde'


def exhaustion_profile():
    # Current-format lab defaults: PLMN lists, SMSS, ACMmax and PUCT. Avoid
    # treating legacy-format expansion as an unrelated credential write.
    card = make_profile(True)
    suffix = bytes.fromhex('00f110') + b'\xff'*43
    if len(card)+len(suffix) != AOC_NVRAM_SIZE:
        raise ValueError('laboratory card storage layout changed')
    return card+suffix


def check_run(run, phase, previous=None, exhaust_puk=False):
    if phase not in ('block','wrong','unblock','cold','permanent'):
        raise ValueError('unknown physical PUK phase')
    summary,trace = check_phase(run,(run/'initial_eeprom.bin').read_bytes(),through=40,
                                minimum_records=2 if phase == 'block' else 1)
    card = (run/'nvram/nsk1r4t/sim_card').read_bytes()
    if statuses(trace,0x24) or statuses(trace,0x26) or statuses(trace,0x28):
        raise ValueError('unexpected credential change/enable/disable command')
    if phase == 'block':
        for attempt in (1,2,3):
            check_contacts(trace,f'block{attempt}',5)
        if statuses(trace,0x20) != [(0x9804,2,10),(0x9804,1,10),(0x9840,0,10)] or statuses(trace,0x2c):
            raise ValueError('physical PIN exhaustion sequence differs')
        check_card(card,0)
        frame_check(run/'snap/nsk1_pin_block_1.png',PIN_PROMPT)
    elif phase == 'permanent':
        if previous is None or len(previous) != AOC_NVRAM_SIZE:
            raise ValueError('permanent-block phase requires qualified exhausted storage')
        normalized = bytearray(previous)
        if normalized[ATTEMPTS_OFFSET+2] != 0:
            raise ValueError('predecessor PUK counter is not exhausted')
        normalized[ATTEMPTS_OFFSET+2] = 10
        check_card(normalized,0)
        if card != previous or statuses(trace,0x20) or statuses(trace,0x2c):
            raise ValueError('permanently blocked cold card changed or retried credentials')
        for time in (10,20,30,40):
            frame_check(run/f'snap/nsk1_native_{time}s.png',PERMANENT_BLOCK)
    else:
        if previous is None or len(previous) != AOC_NVRAM_SIZE:
            raise ValueError('cold PUK phase requires qualified predecessor storage')
        expected = bytearray(previous)
        if phase in ('wrong','unblock'):
            normalized = bytearray(previous)
            allowed = range(1,11) if phase == 'wrong' and exhaust_puk else ((10,) if phase == 'wrong' else (9,10))
            if normalized[ATTEMPTS_OFFSET+2] not in allowed:
                raise ValueError('unexpected predecessor PUK retry state')
            normalized[ATTEMPTS_OFFSET+2] = 10
            check_card(normalized,0)
            for name,count in (('puk',9),('new',5),('confirm',5)):
                check_contacts(trace,name,count)
            remaining = previous[ATTEMPTS_OFFSET+2]-1
            result = ((0x9840 if remaining == 0 else 0x9804),0,remaining) if phase == 'wrong' else (0x9000,3,10)
            if statuses(trace,0x2c) != [result] or statuses(trace,0x20):
                raise ValueError('physical PUK recovery sequence differs')
            if phase == 'wrong':
                expected[ATTEMPTS_OFFSET+2] = remaining
            else:
                expected[CHV_OFFSET:CHV_OFFSET+8] = NEW_PIN
                expected[ATTEMPTS_OFFSET] = 3
                expected[ATTEMPTS_OFFSET+2] = 10
            terminal = CODE_ERROR if phase == 'wrong' and remaining == 0 else WRONG_PUK
            frames = ((1,PUK_PROMPT),(2,NEW_PIN_PROMPT),(4,terminal)) if phase == 'wrong' else ((1,PUK_PROMPT),(2,NEW_PIN_PROMPT),(4,PHONE_PROMPT))
            for index,digest in frames:
                frame_check(run/f'snap/nsk1_puk_unblock_{index}.png',digest)
            if phase == 'wrong' and remaining == 0:
                for index in (5,6):
                    frame_check(run/f'snap/nsk1_puk_unblock_{index}.png',PERMANENT_BLOCK)
        else:
            original = bytearray(previous)
            if original[CHV_OFFSET:CHV_OFFSET+8] != NEW_PIN:
                raise ValueError('predecessor lacks the recovered replacement PIN')
            # Reuse the shared original-credential checker for all other fields.
            from tools.make_sim_card_profile import CHV1
            original[CHV_OFFSET:CHV_OFFSET+8] = CHV1
            check_card(original,3)
            check_contacts(trace,'pin',5)
            check_contacts(trace,'phone',6)
            if statuses(trace,0x20) != [(0x9000,3,10)] or statuses(trace,0x2c):
                raise ValueError('cold replacement PIN authentication differs')
            for index,digest in ((1,PIN_PROMPT),(2,ACCEPTED),(3,PHONE_PROMPT)):
                frame_check(run/f'snap/nsk1_pin_reload_{index}.png',digest)
        if card != expected:
            raise ValueError('PUK lifecycle changed unrelated persisted card bytes')
    summary['physical_sim_security_phase'] = phase
    return summary,card


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('binary','roms','capture','run'):
        parser.add_argument(name,type=Path)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--wrong-puk-first',action='store_true',help='verify failed-PUK persistence before cold recovery')
    mode.add_argument('--exhaust-puk',action='store_true',help='physically consume all ten PUK retries, then verify cold permanent blocking')
    args = parser.parse_args()
    root = args.run.resolve()
    try:
        if root.exists():
            raise ValueError('PUK gate requires a fresh directory')
        block = root/'block'
        initial = prepare(args.roms.resolve(),block,args.capture.resolve(),True,True,True,True,True)
        (block/'initial_eeprom.bin').write_bytes(initial)
        card = exhaustion_profile() if args.exhaust_puk else make_profile(True)
        (block/'nvram/nsk1r4t/sim_card').write_bytes(card)
        path = block/'input_manifest.json'
        manifest = json.loads(path.read_text())
        manifest['synthetic_pin_profile_sha256'] = hashlib.sha256(card).hexdigest()
        manifest['synthetic_pin_profile'] = 'shared laboratory PIN-enabled card; retries initially 3, not pre-blocked'
        path.write_text(json.dumps(manifest,indent=2)+'\n')
        previous_run,previous_card = None,None
        phases = [('block','block','nsk1_pin_block.lua')]
        if args.exhaust_puk:
            phases.extend((f'wrong-{index:02d}','wrong','nsk1_puk_wrong.lua') for index in range(1,11))
            phases.append(('permanent','permanent','nsk1_boot_observe.lua'))
        else:
            if args.wrong_puk_first:
                phases.append(('wrong','wrong','nsk1_puk_wrong.lua'))
            phases.extend((('unblock','unblock','nsk1_puk_input.lua'),('cold','cold','nsk1_puk_reload.lua')))
        for name,phase,script in phases:
            run = root/name
            if previous_run is not None:
                copy_persistent(run,previous_run)
            command = run_machine(args.binary,run,Path(__file__).with_name(script),41,True)
            summary,card = check_run(run,phase,previous_card,args.exhaust_puk)
            if args.exhaust_puk and phase == 'block':
                expected = bytearray(exhaustion_profile())
                expected[ATTEMPTS_OFFSET] = 0
                if card != expected:
                    raise ValueError('initial PIN blocking changed unrelated laboratory card bytes')
            summary['command'] = command
            (run/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
            previous_run,previous_card = run,card
            print(f'NSK-1 PUK phase {name}: PASS retries={card[ATTEMPTS_OFFSET+2]}',flush=True)
        if args.exhaust_puk:
            (root/'result.json').write_text(json.dumps({'physical_pin_exhaustion':True,
                'physical_wrong_puk_attempts':10,'cold_permanent_block':True,
                'seeded_blocked_state':False,'scope':'native compatibility; no fitted-mask or network proof'},indent=2)+'\n')
            print('NSK-1 physical PUK exhaustion / cold permanent block PASS')
            return
        (root/'result.json').write_text(json.dumps({'physical_pin_exhaustion':True,'cold_puk_recovery':True,
            'cold_replacement_pin':True,'seeded_blocked_state':False,
            'physical_wrong_puk_cold_retry':args.wrong_puk_first,
            'scope':'native compatibility; no permanent-block, fitted-mask or network proof'},indent=2)+'\n')
        print('NSK-1 physical PIN blocking / '+('wrong PUK / ' if args.wrong_puk_first else '')+'cold PUK recovery / cold replacement PIN PASS')
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        parser.exit(1,f'NSK-1 PUK round trip failed: {error}; inspect {root}\n')


if __name__ == '__main__':
    main()

"""Physical NSK-1 PIN blocking, cold PUK recovery and cold replacement PIN."""
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
from tools.nsk1_pin_roundtrip import check_card,check_contacts,PIN_PROMPT,PHONE_PROMPT,ACCEPTED
from tools.sim_security_trace_check import statuses,CHV_OFFSET,ATTEMPTS_OFFSET


PUK_PROMPT = '2944a1b1d362846853f92be495ece23c7986528e9fb519fcae638a8f1a2023b8'
NEW_PIN_PROMPT = '836967ace63acc78a441c69f89272159ec24faa2c4dec6983d436edb9f223bc7'
NEW_PIN = b'4321\xff\xff\xff\xff'


def check_run(run, phase, previous=None):
    if phase not in ('block','unblock','cold'):
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
    else:
        if previous is None:
            raise ValueError('cold PUK phase requires qualified predecessor storage')
        expected = bytearray(previous)
        if phase == 'unblock':
            check_card(previous,0)
            for name,count in (('puk',9),('new',5),('confirm',5)):
                check_contacts(trace,name,count)
            if statuses(trace,0x2c) != [(0x9000,3,10)] or statuses(trace,0x20):
                raise ValueError('physical PUK recovery sequence differs')
            expected[CHV_OFFSET:CHV_OFFSET+8] = NEW_PIN
            expected[ATTEMPTS_OFFSET] = 3
            for index,digest in ((1,PUK_PROMPT),(2,NEW_PIN_PROMPT),(4,PHONE_PROMPT)):
                frame_check(run/f'snap/nsk1_puk_unblock_{index}.png',digest)
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
    args = parser.parse_args()
    root = args.run.resolve()
    try:
        if root.exists():
            raise ValueError('PUK gate requires a fresh directory')
        block = root/'block'
        initial = prepare(args.roms.resolve(),block,args.capture.resolve(),True,True,True,True,True)
        (block/'initial_eeprom.bin').write_bytes(initial)
        card = make_profile(True)
        (block/'nvram/nsk1r4t/sim_card').write_bytes(card)
        path = block/'input_manifest.json'
        manifest = json.loads(path.read_text())
        manifest['synthetic_pin_profile_sha256'] = hashlib.sha256(card).hexdigest()
        manifest['synthetic_pin_profile'] = 'shared laboratory PIN-enabled card; retries initially 3, not pre-blocked'
        path.write_text(json.dumps(manifest,indent=2)+'\n')
        previous_run,previous_card = None,None
        for phase,script in (('block','nsk1_pin_block.lua'),('unblock','nsk1_puk_input.lua'),('cold','nsk1_puk_reload.lua')):
            run = root/phase
            if previous_run is not None:
                copy_persistent(run,previous_run)
            command = run_machine(args.binary,run,Path(__file__).with_name(script),41,True)
            summary,card = check_run(run,phase,previous_card)
            summary['command'] = command
            (run/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
            previous_run,previous_card = run,card
        (root/'result.json').write_text(json.dumps({'physical_pin_exhaustion':True,'cold_puk_recovery':True,
            'cold_replacement_pin':True,'seeded_blocked_state':False,
            'scope':'native compatibility; no wrong-PUK/permanent-block, fitted-mask or network proof'},indent=2)+'\n')
        print('NSK-1 physical PIN blocking / cold PUK recovery / cold replacement PIN PASS')
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        parser.exit(1,f'NSK-1 PUK round trip failed: {error}; inspect {root}\n')


if __name__ == '__main__':
    main()

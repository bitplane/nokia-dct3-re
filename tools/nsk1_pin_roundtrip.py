"""Physical NSK-1 wrong-PIN persistence and correct cold verification."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.make_sim_card_profile import make_profile,CHV1,CHV2,PUK1,PUK2
from tools.nsk1_rom4_compare import prepare
from tools.nsk1_security_compare import run_machine,check_phase
from tools.nsk1_phonebook_roundtrip import copy_persistent,frame_check
from tools.sim_security_trace_check import statuses,CHV_OFFSET,ATTEMPTS_OFFSET,ENABLED_OFFSET,AOC_NVRAM_SIZE


PIN_PROMPT = '558546ebf965584341b2aba31df75892eeee3ea2acaace70a8bb20bc3c6eb8ab'
CODE_ERROR = 'fd5b8fb1ab0c3397bf958d567a46aee84a72f75123802a00d5bc802a1814543e'
ACCEPTED = 'dd6eba3e2fe5e7bdfbc0fc187d2dc63475c1227911a0deeaa4be70546075a0c4'
PHONE_PROMPT = 'f82d19b12314200132f0d19ab17a2d86b202d65ba7bc817d1a5c6cbb90a59dfc'


def check_contacts(trace, phase, count):
    marker = f'nsk1_pin_contact: phase={phase} '
    rows = re.findall(re.escape(marker)+r'index=(\d+) down=([01]) t=[0-9.]+ physical_only=1',trace)
    if rows != [(str(index),down) for index in range(1,count+1) for down in ('1','0')] or len(rows) != trace.count(marker):
        raise ValueError('physical SIM security contacts differ')


def check_card(card, retries):
    if (len(card) != AOC_NVRAM_SIZE or card[ENABLED_OFFSET] != 1
            or card[CHV_OFFSET:CHV_OFFSET+32] != CHV1+CHV2+PUK1+PUK2
            or card[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET+4] != bytes((retries,3,10,10))):
        raise ValueError('persistent SIM credential/retry contract differs')


def check_run(run, correct=False, previous=None):
    summary,trace = check_phase(run,(run/'initial_eeprom.bin').read_bytes(),
                                through=40 if correct else 20,minimum_records=1 if correct else 2)
    check_contacts(trace,'pin' if correct else 'wrong',5)
    if correct:
        check_contacts(trace,'phone',6)
    retries = 3 if correct else 2
    if statuses(trace,0x20) != [(0x9000 if correct else 0x9804,retries,10)]:
        raise ValueError('SIM VERIFY result/retry transition differs')
    if statuses(trace,0x2c) or statuses(trace,0x24) or statuses(trace,0x26):
        raise ValueError('unexpected SIM credential mutation')
    card = (run/'nvram/nsk1r4t/sim_card').read_bytes()
    check_card(card,retries)
    if correct:
        if previous is None:
            raise ValueError('cold retry proof requires the qualified wrong-PIN card')
        check_card(previous,2)
        expected = bytearray(previous)
        expected[ATTEMPTS_OFFSET] = 3
        if card != expected:
            raise ValueError('cold VERIFY changed unrelated card storage')
        for index,digest in ((1,PIN_PROMPT),(2,ACCEPTED),(3,PHONE_PROMPT)):
            frame_check(run/f'snap/nsk1_pin_reload_{index}.png',digest)
    else:
        frame_check(run/'snap/nsk1_pin_wrong_1.png',PIN_PROMPT)
        frame_check(run/'snap/nsk1_pin_wrong_2.png',CODE_ERROR)
    summary['physical_sim_pin_correct' if correct else 'physical_sim_pin_wrong'] = True
    return summary,card


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('binary','roms','capture','run'):
        parser.add_argument(name,type=Path)
    args = parser.parse_args()
    root = args.run.resolve()
    try:
        if root.exists():
            raise ValueError('SIM PIN gate requires a fresh directory')
        wrong,correct = root/'wrong',root/'correct'
        initial = prepare(args.roms.resolve(),wrong,args.capture.resolve(),True,True,True,True,True)
        (wrong/'initial_eeprom.bin').write_bytes(initial)
        card = make_profile(True)
        (wrong/'nvram/nsk1r4t/sim_card').write_bytes(card)
        path = wrong/'input_manifest.json'
        manifest = json.loads(path.read_text())
        manifest['synthetic_pin_profile_sha256'] = hashlib.sha256(card).hexdigest()
        manifest['synthetic_pin_profile'] = 'shared laboratory PIN-enabled card; no donor storage'
        path.write_text(json.dumps(manifest,indent=2)+'\n')
        command = run_machine(args.binary,wrong,Path(__file__).with_name('nsk1_pin_fixture.lua'),24,True)
        summary,card = check_run(wrong)
        summary['command'] = command
        (wrong/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
        copy_persistent(correct,wrong)
        command = run_machine(args.binary,correct,Path(__file__).with_name('nsk1_pin_reload.lua'),41,True)
        summary,_ = check_run(correct,True,card)
        summary['command'] = command
        (correct/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
        (root/'result.json').write_text(json.dumps({'physical_wrong_pin':True,
            'cold_retry_persistence':True,'physical_correct_pin':True,'phone_code_distinct':True,
            'scope':'native compatibility SIM PIN; no PUK, fitted mask or network proof'},indent=2)+'\n')
        print('NSK-1 physical wrong PIN / cold correct PIN PASS; compatibility limits retained')
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        parser.exit(1,f'NSK-1 PIN round trip failed: {error}; inspect {root}\n')


if __name__ == '__main__':
    main()

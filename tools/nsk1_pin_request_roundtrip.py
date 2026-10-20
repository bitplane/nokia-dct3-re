"""Physical NSK-1 PIN request disable/enable with independent cold boots."""
import argparse
import json
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsk1_pin_roundtrip import check_run as check_pin,check_contacts,PIN_PROMPT,PHONE_PROMPT,check_card
from tools.nsk1_phonebook_roundtrip import copy_persistent,frame_check
from tools.nsk1_security_compare import run_machine,check_phase
from tools.sim_security_trace_check import statuses,ENABLED_OFFSET

DONE = '8a8c6c68ea5d450f6e4a7fc928ef838f2389c43acb6649491edb4bf0e1d7ad2f'
SEQUENCE = 'menu menu down down down menu down down menu menu one two three four menu down menu'.split()


def check_transition(previous,card,enabled):
    if len(previous) <= ENABLED_OFFSET:
        raise ValueError('truncated PIN request source card')
    baseline=bytearray(previous)
    baseline[ENABLED_OFFSET]=1
    check_card(baseline,3)
    if previous[ENABLED_OFFSET] != int(not enabled):
        raise ValueError('PIN request source flag differs')
    expected=bytearray(previous)
    expected[ENABLED_OFFSET]=int(enabled)
    if card != expected:
        raise ValueError('PIN request changed unrelated card storage')


def check_menu_contacts(trace,enabled):
    sequence=SEQUENCE[1:] if enabled else SEQUENCE
    marker='nsk1_pin_request_contact: '
    rows=re.findall(re.escape(marker)+r'index=(\d+) key=(\w+) down=([01]) t=[0-9.]+ physical_only=1',trace)
    expected=[(str(i),name,down) for i,name in enumerate(sequence,1) for down in ('1','0')]
    if rows != expected or len(rows) != trace.count(marker):
        raise ValueError('physical PIN-request menu contacts differ')


def check_run(run,previous,enabled,cold=False):
    summary,trace=check_phase(run,(run/'initial_eeprom.bin').read_bytes(),
                              through=40 if cold else 59,minimum_records=1)
    card=(run/'nvram/nsk1r4t/sim_card').read_bytes()
    if cold:
        if card != previous or card[ENABLED_OFFSET] != int(enabled):
            raise ValueError('cold PIN-request card changed')
        if any(statuses(trace,ins) for ins in (0x20,0x24,0x26,0x28,0x2c)):
            raise ValueError('no-input cold boot authenticated or mutated credentials')
        for index in range(1,5):
            frame_check(run/f'snap/nsk1_pin_request_cold_{index}.png',PIN_PROMPT if enabled else PHONE_PROMPT)
    else:
        check_transition(previous,card,enabled)
        check_menu_contacts(trace,enabled)
        if not enabled:
            check_contacts(trace,'pin',5)
            check_contacts(trace,'phone',6)
        expected_verify=[] if enabled else [(0x9000,3,10)]
        if statuses(trace,0x20) != expected_verify:
            raise ValueError('startup PIN verification differs')
        for ins in (0x24,0x26,0x28,0x2c):
            expected=[(0x9000,3,10)] if ins == (0x28 if enabled else 0x26) else []
            if statuses(trace,ins) != expected:
                raise ValueError('PIN-request command/result differs')
        frame_check(run/f'snap/nsk1_pin_request_{16 if enabled else 17}.png',DONE)
    summary['physical_pin_request']={'enabled':enabled,'cold_no_input':cold}
    return summary,card


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('binary','pin_capture','run'):
        parser.add_argument(name,type=Path)
    args=parser.parse_args()
    try:
        root=args.run.resolve()
        if root.exists():
            raise ValueError('PIN request gate requires a fresh directory')
        source=args.pin_capture.resolve()
        _,wrong=check_pin(source/'wrong')
        _,card=check_pin(source/'correct',True,wrong)
        source=source/'correct'
        results={}
        for name,enabled,cold,script in (
                ('disable',False,False,'nsk1_pin_request.lua'),
                ('disabled_cold',False,True,'nsk1_pin_request_cold.lua'),
                ('enable',True,False,'nsk1_pin_request_enable.lua'),
                ('enabled_cold',True,True,'nsk1_pin_request_cold.lua')):
            run=root/name
            copy_persistent(run,source)
            run_machine(args.binary.resolve(),run,Path(__file__).with_name(script),41 if cold else 61,True)
            results[name],card=check_run(run,card,enabled,cold)
            source=run
        (root/'summary.json').write_text(json.dumps(results,indent=2)+'\n')
    except (ValueError,OSError) as error:
        print(f'NSK-1 PIN request gate failed: {error}',file=sys.stderr)
        return 1
    print('NSK-1 physical PIN request disable/cold/enable/cold PASS')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

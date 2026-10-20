"""Physical NSK-1 ordinary CHANGE CHV and cold replacement-PIN verification."""
import argparse
import json
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsk1_pin_roundtrip import check_run as check_pin,check_card,check_contacts,PIN_PROMPT,ACCEPTED,PHONE_PROMPT
from tools.nsk1_phonebook_roundtrip import copy_persistent,frame_check
from tools.nsk1_security_compare import run_machine,check_phase
from tools.sim_security_trace_check import statuses,CHV_OFFSET

MENU='menu menu down down down menu down down menu down down down down menu down menu'.split()
CHANGED='e3d554448ecdac351002f8a3f94582448fc1f03cc5a9b3071e9b0e60272f018a'


def check_change(previous,card):
    check_card(previous,3)
    expected=bytearray(previous)
    expected[CHV_OFFSET:CHV_OFFSET+4]=b'4321'
    if card != expected:
        raise ValueError('ordinary PIN change modified unrelated card bytes')


def check_change_contacts(trace, pin2=False, restore=False, wrong=False, puk2=False, wrong_puk2=False, entry_only=False, navigate=False):
    menu = MENU[:]
    if pin2:
        menu.insert(-1, 'down')
    old = ['five','six','seven','eight','menu'] if pin2 else ['one','two','three','four','menu']
    replacement = ['eight','seven','six','five','menu'] if pin2 else ['four','three','two','one','menu']
    if restore:
        old, replacement = replacement, old
    if wrong:
        old = ['five','six','seven','five','menu']
    if puk2:
        old = ['eight','seven','six','five','four','three','two','one','menu']
    if wrong_puk2:
        old = ['eight','seven','six','five','four','three','two','two','menu']
    sequences={'menu':menu,'old':old,'new':replacement,'confirm':replacement}
    if entry_only:
        sequences={'menu':menu}
        if navigate:
            sequences['navigate']=['down']
    marker='nsk1_pin_change_contact: '
    rows=re.findall(re.escape(marker)+r'phase=(\w+) index=(\d+) key=(\w+) down=([01]) t=[0-9.]+ physical_only=1',trace)
    expected=[(phase,str(i),name,down) for phase,names in sequences.items()
              for i,name in enumerate(names,1) for down in ('1','0')]
    if rows != expected or len(rows) != trace.count(marker):
        raise ValueError('physical PIN-change menu/credential contacts differ')


def check_run(run,previous,cold=False):
    summary,trace=check_phase(run,(run/'initial_eeprom.bin').read_bytes(),through=40 if cold else 59,minimum_records=1)
    check_contacts(trace,'pin',5)
    check_contacts(trace,'phone',6)
    if statuses(trace,0x20) != [(0x9000,3,10)]:
        raise ValueError('startup PIN verification failed')
    for ins in (0x24,0x26,0x28,0x2c):
        if statuses(trace,ins) != ([(0x9000,3,10)] if ins==0x24 and not cold else []):
            raise ValueError('ordinary PIN change command/status differs')
    card=(run/'nvram/nsk1r4t/sim_card').read_bytes()
    if cold:
        if card != previous or card[CHV_OFFSET:CHV_OFFSET+8] != b'4321\xff\xff\xff\xff':
            raise ValueError('cold replacement PIN/storage differs')
        for index,digest in ((1,PIN_PROMPT),(2,ACCEPTED),(3,PHONE_PROMPT)):
            frame_check(run/f'snap/nsk1_pin_reload_{index}.png',digest)
    else:
        check_change(previous,card)
        check_change_contacts(trace)
        frame_check(run/'snap/nsk1_pin_change_19.png',CHANGED)
    summary['ordinary_pin_change_cold' if cold else 'ordinary_pin_change']=True
    return summary,card


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('binary','pin_capture','run'):
        parser.add_argument(name,type=Path)
    args=parser.parse_args()
    try:
        root=args.run.resolve()
        if root.exists():
            raise ValueError('PIN change gate requires a fresh directory')
        source=args.pin_capture.resolve()
        _,wrong=check_pin(source/'wrong')
        _,card=check_pin(source/'correct',True,wrong)
        source=source/'correct'
        results={}
        for name,cold,script,seconds in (('change',False,'nsk1_pin_change.lua',75),
                                       ('cold',True,'nsk1_pin_changed_reload.lua',41)):
            run=root/name
            copy_persistent(run,source)
            run_machine(args.binary.resolve(),run,Path(__file__).with_name(script),seconds,True)
            results[name],card=check_run(run,card,cold)
            source=run
        (root/'summary.json').write_text(json.dumps(results,indent=2)+'\n')
    except (ValueError,OSError) as error:
        print(f'NSK-1 ordinary PIN change failed: {error}',file=sys.stderr)
        return 1
    print('NSK-1 physical ordinary PIN change and cold verification PASS')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

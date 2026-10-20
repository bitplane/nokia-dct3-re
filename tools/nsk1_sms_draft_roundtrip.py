"""Physical NSK-1 offline SMS draft save and independently qualified cold read."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsk1_rom4_compare import prepare
from tools.nsk1_security_compare import run_machine,check_phase
from tools.nsk1_phonebook_roundtrip import copy_persistent,check_contacts,frame_check
from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH,ADN_LENGTH


SMS_OFFSET = 1636
RECORD_SIZE = 176
DRAFT = bytes.fromhex('070281f001ff00810000024110').ljust(RECORD_SIZE,b'\xff')
SAVED_HASH = 'd4562fa5c9fbc8bb0180f9361ffebbecf39ad0bcda7708ab15e45b196b4144eb'
READ_HASH = 'f2320fef70b570e75b15c83bcefdcbb8beaf7c80c29b184ca7ddac24e9b9de57'
SAVE_KEYS = ('menu','two','three','two','zero','menu')+('down',)*6+('menu',)
SAVE_CODES = (0x19,2,3,2,0x0a,0x19)+(0x18,)*6+(0x19,)
READ_KEYS = ('menu','menu','two','two','menu','menu')
READ_CODES = (0x19,0x19,2,2,0x19,0x19)
ERASE_CONFIRM = '06710f9190043e99bf225a5143e57f6ac8f622d79d89faf95750f59854349c7d'
ERASED = 'a60c4b9f39973847843cb184ff7ea1e5b2ffc5913d09e825df22c7cabaaf4173'
EMPTY = 'cdff157a2a10f5c1528e9085b1981401a9714ae37cded35e3eddd43fd8a83fb0'


def check_phone_code(trace, following):
    contacts = re.findall(r'nsk1_code_contact: index=(\d+) column=(\d+) mask=([0-9a-f]{2}) down=([01]) t=[0-9.]+ physical_only=1',trace)
    expected = [(str(index),str(column),f'{mask:02x}',down)
                for index,(column,mask) in enumerate(((2,2),(3,2),(4,2),(2,4),(3,4),(1,4)),1)
                for down in ('1','0')]
    keys = re.findall(r'nsk1_code_key: value=([0-9a-f]{2}) t=[0-9.]+ observer_writes=0',trace)
    gates = re.findall(r'nsk1_code_gate: match=([0-9a-f]{8}) retry=([0-9a-f]{2}) cycles=[0-9a-f]{16} observer_writes=0',trace)
    if (contacts != expected or len(contacts) != trace.count('nsk1_code_contact:')
            or [int(key,16) for key in keys] != [1,2,3,4,5,0x19]+list(following)
            or len(keys) != trace.count('nsk1_code_key:')
            or gates != [('00000001','00')] or len(gates) != trace.count('nsk1_code_gate:')):
        raise ValueError('own physical phone-code/editor input differs')


def check_card(card):
    if (len(card) != CURRENT_NVRAM_LENGTH or card[:ADN_LENGTH] != b'\xff'*ADN_LENGTH
            or card[SMS_OFFSET:SMS_OFFSET+RECORD_SIZE] != DRAFT
            or card[SMS_OFFSET+RECORD_SIZE:SMS_OFFSET+10*RECORD_SIZE] != (b'\0'+b'\xff'*175)*9):
        raise ValueError('expected exact unsent A-space draft and otherwise empty SMS/ADN storage')


def check_run(run, cold=False, previous=None):
    summary,trace = check_phase(run,(run/'initial_eeprom.bin').read_bytes(),
                                through=40 if cold else 59,minimum_records=1 if cold else 2)
    check_phone_code(trace,READ_CODES if cold else SAVE_CODES)
    check_contacts(trace,'nsk1_sms_reload_contact' if cold else 'nsk1_sms_draft_contact',READ_KEYS if cold else SAVE_KEYS)
    card = (run/'nvram/nsk1r4t/sim_card').read_bytes()
    check_card(card)
    if cold:
        if (previous is None or card != previous or re.search(r'update fid=6f3c|ins=dc .*selected=6f3c',trace)
                or 'ins=b2 p1=01 p2=04 p3=b0 selected=6f3c' not in trace):
            raise ValueError('cold Outbox read omitted the SIM read or changed card storage')
        frame_check(run/'snap/nsk1_sms_reload_3.png',READ_HASH)
    else:
        if (re.findall(r'update fid=6f3c record=(\d+) length=(\d+)',trace) != [('1','176')]
                or 'ins=dc p1=01 p2=04 p3=b0 selected=6f3c' not in trace
                or not re.search(r'SIM status ins=dc sw=9000\b',trace)):
            raise ValueError('physical draft did not perform exactly one successful SIM record update')
        frame_check(run/'snap/nsk1_sms_draft_9.png',SAVED_HASH)
    summary['physical_cold_sms_read' if cold else 'physical_sms_draft_save'] = True
    return summary,card


def check_deleted(run, previous, cold=False):
    summary,trace = check_phase(run,(run/'initial_eeprom.bin').read_bytes(),
                                through=40 if cold else 50,minimum_records=1)
    check_phone_code(trace,READ_CODES+(() if cold else (0x19,)*3))
    check_contacts(trace,'nsk1_sms_reload_contact',READ_KEYS)
    if not cold:
        check_contacts(trace,'nsk1_sms_erase_contact',('menu',)*3)
        check_card(previous)
    card = (run/'nvram/nsk1r4t/sim_card').read_bytes()
    normalized = bytearray(card)
    if len(normalized) != CURRENT_NVRAM_LENGTH or normalized[SMS_OFFSET] != 0:
        raise ValueError('SMS record is not persistently free')
    normalized[SMS_OFFSET] = 7
    check_card(normalized)
    expected = bytearray(previous)
    if not cold:
        expected[SMS_OFFSET] = 0
    if card != expected:
        raise ValueError('SMS deletion/cold search changed bytes outside the permitted status')
    if cold:
        if ('ins=b2 p1=01 p2=04 p3=b0 selected=6f3c' not in trace
                or re.search(r'update fid=6f3c|ins=dc .*selected=6f3c',trace)):
            raise ValueError('cold empty Outbox omitted read or rewrote SMS')
        frame_check(run/'snap/nsk1_sms_empty_1.png',EMPTY)
    else:
        if (re.findall(r'update fid=6f3c record=(\d+) length=(\d+)',trace) != [('1','176')]
                or 'ins=dc p1=01 p2=04 p3=b0 selected=6f3c' not in trace
                or not re.search(r'SIM status ins=dc sw=9000\b',trace)):
            raise ValueError('SMS deletion did not commit one successful record-one update')
        frame_check(run/'snap/nsk1_sms_reload_3.png',READ_HASH)
        frame_check(run/'snap/nsk1_sms_erase_2.png',ERASE_CONFIRM)
        frame_check(run/'snap/nsk1_sms_erase_3.png',ERASED)
    summary['physical_cold_empty_outbox' if cold else 'physical_sms_deletion'] = True
    return summary,card


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('binary','roms','capture','run'):
        parser.add_argument(name,type=Path)
    parser.add_argument('--erase',action='store_true',help='extend through physical SMS deletion and cold empty Outbox')
    args = parser.parse_args()
    root = args.run.resolve()
    try:
        if root.exists():
            raise ValueError('offline SMS gate requires a fresh directory')
        save,cold = root/'save',root/'cold'
        initial = prepare(args.roms.resolve(),save,args.capture.resolve(),True,True,True,True,True)
        (save/'initial_eeprom.bin').write_bytes(initial)
        command = run_machine(args.binary,save,Path(__file__).with_name('nsk1_sms_draft_input.lua'),65,True)
        summary,card = check_run(save)
        summary['command'] = command
        (save/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
        check_run(save)
        copy_persistent(cold,save)
        command = run_machine(args.binary,cold,Path(__file__).with_name('nsk1_sms_draft_reload.lua'),41,True)
        summary,_ = check_run(cold,True,card)
        summary['command'] = command
        (cold/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
        if args.erase:
            erased,empty = root/'erase',root/'empty'
            check_run(save)
            copy_persistent(erased,save)
            command = run_machine(args.binary,erased,Path(__file__).with_name('nsk1_sms_draft_erase.lua'),51,True)
            summary,deleted = check_deleted(erased,card)
            summary['command'] = command
            (erased/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
            copy_persistent(empty,erased)
            command = run_machine(args.binary,empty,Path(__file__).with_name('nsk1_sms_draft_empty.lua'),41,True)
            summary,_ = check_deleted(empty,deleted,cold=True)
            summary['command'] = command
            (empty/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
        (root/'result.json').write_text(json.dumps({'physical_sms_draft_save':True,'physical_cold_outbox_read':True,
            'physical_delete_cold_empty':args.erase,
            'whole_card_preserved_on_read':True,'ram_or_save_state_copied':False,
            'scope':'offline unsent draft on native compatibility composition; no network SMS or fitted-mask proof'},indent=2)+'\n')
        print('NSK-1 physical offline SMS draft save / cold Outbox read'+(' / delete / cold empty' if args.erase else '')+' PASS')
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        parser.exit(1,f'NSK-1 SMS draft round trip failed: {error}; inspect {root}\n')


if __name__ == '__main__':
    main()

"""Own NSB-1 PPM B offline draft save/cold Outbox read; no radio SMS claim."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb1_rom4_compare import (check_card_run, check_frame, check_receive_activity,
                              check_security_gate, inputs_for_ppm, prepare,
                              prepare_cold_run, run_mame)

SMS_OFFSET = 1636
RECORD_SIZE = 176
DRAFT = bytes.fromhex('070281f001ff00810000024110').ljust(RECORD_SIZE, b'\xff')
SAVED_HASH = 'd4562fa5c9fbc8bb0180f9361ffebbecf39ad0bcda7708ab15e45b196b4144eb'
READ_HASH = 'f2320fef70b570e75b15c83bcefdcbb8beaf7c80c29b184ca7ddac24e9b9de57'
ERASE_CONFIRM_HASH = '06710f9190043e99bf225a5143e57f6ac8f622d79d89faf95750f59854349c7d'
ERASED_HASH = 'a60c4b9f39973847843cb184ff7ea1e5b2ffc5913d09e825df22c7cabaaf4173'
EMPTY_HASH = 'cdff157a2a10f5c1528e9085b1981401a9714ae37cded35e3eddd43fd8a83fb0'


def check_card(data):
    if len(data) != 3570:
        raise ValueError('draft requires complete current SIM storage')
    if data[SMS_OFFSET:SMS_OFFSET+RECORD_SIZE] != DRAFT:
        raise ValueError('missing exact unsent SMS-SUBMIT A-space record')
    if data[SMS_OFFSET+RECORD_SIZE:SMS_OFFSET+2*RECORD_SIZE] != b'\x00' + b'\xff' * (RECORD_SIZE-1):
        raise ValueError('draft changed the other SMS record')


def check_contacts(trace, reload=False):
    names = (['menu','two','one','two','menu'] if reload else
             ['menu','two','one','three','two','zero','menu'] + ['down']*6 + ['menu'])
    phase = 'reload' if reload else 'draft'
    observed = re.findall(r'nsb1_sms_' + phase + r'_contact: key=(\w+) down=([01]) t=[0-9.]+ physical_only=1', trace)
    if observed != [(name, down) for name in names for down in ('1', '0')]:
        raise ValueError('missing physical SMS draft contacts')


def check_deleted_card(data):
    # Firmware frees the record by status only; retained TPDU bytes are expected.
    expected = b'\x00'+DRAFT[1:]
    if len(data) != 3570 or data[SMS_OFFSET:SMS_OFFSET+RECORD_SIZE] != expected:
        raise ValueError('deleted SMS status/preserved payload differs')


def check_erased(trace,card,snapshots):
    check_contacts(trace,reload=True)
    contacts = re.findall(r'nsb1_sms_erase_contact: index=(\d+) down=([01]) t=[0-9.]+ physical_only=1',trace)
    if contacts != [(str(index),down) for index in range(1,4) for down in ('1','0')]:
        raise ValueError('missing physical SMS erase confirmation')
    if (re.findall(r'update fid=6f3c record=(\d+) length=(\d+)',trace) != [('1','176')] or
            'ins=dc p1=01 p2=04 p3=b0 selected=6f3c' not in trace or
            'SIM status ins=dc sw=9000' not in trace):
        raise ValueError('SMS erase UPDATE RECORD lifecycle differs')
    check_deleted_card(card)
    check_frame(snapshots/'sms_reload_2.png',READ_HASH)
    check_frame(snapshots/'sms_erase_1.png',ERASE_CONFIRM_HASH)
    check_frame(snapshots/'sms_erase_2.png',ERASED_HASH)


def check_empty(trace,card,snapshots):
    check_contacts(trace,reload=True)
    check_deleted_card(card)
    if 'ins=dc' in trace or 'ins=b2 p1=01 p2=04 p3=b0 selected=6f3c' not in trace:
        raise ValueError('cold empty Outbox did not read without rewriting')
    check_frame(snapshots/'sms_empty_1.png',EMPTY_HASH)


def check_editor_state(trace):
    saved = re.findall(r'nsb1_sms_state: event=saved t=([0-9.]+) pc=([0-9a-f]{8}) ram=([0-9a-f]{8}) dsp=([^\r\n]+)', trace)
    restored = re.findall(r'nsb1_sms_state: event=restored result=pass t=([0-9.]+) pc=([0-9a-f]{8}) ram=([0-9a-f]{8}) dsp=([^\r\n]+)', trace)
    if len(saved) != 1 or saved != restored or 'nsb1_sms_state: FAIL' in trace:
        raise ValueError('native SMS editor save-state mismatch or incomplete roundtrip')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('rom_root', type=Path)
    parser.add_argument('run_dir', type=Path)
    parser.add_argument('--checksum-control-from', type=Path, required=True,
                        help='qualified own PPM B erased-storage run')
    parser.add_argument('--restore-editor', action='store_true',
                        help='save/load the populated editor before physical Save continuation')
    parser.add_argument('--erase',action='store_true',help='extend through physical deletion and cold empty Outbox')
    args = parser.parse_args()
    try:
        root, output = args.rom_root.resolve(), args.run_dir.resolve()
        if output.exists():
            raise ValueError('use a fresh draft roundtrip directory')
        save, cold = output/'save', output/'cold'
        rompath = prepare(root, save, args.checksum_control_from.resolve(), True, True,
                          ppm='B', english_dictionary=True)
        code = run_mame(args.binary.resolve(), rompath, save,
                        Path(__file__).with_name('nsb1_sms_draft_state.lua' if args.restore_editor
                                                 else 'nsb1_sms_draft_input.lua').resolve(), 61, bios='671b')
        trace, card = check_card_run(save, code, 61, ppm='B')
        check_receive_activity(trace, 60)
        check_security_gate(trace, True)
        check_contacts(trace)
        if args.restore_editor:
            check_editor_state(trace)
        check_card(card)
        if 'SIM status ins=dc sw=9000' not in trace:
            raise ValueError('missing successful card update')
        check_frame(save/'snap/sms_draft_5.png', SAVED_HASH)
        rompath = prepare_cold_run(cold, save, check_card, inputs_for_ppm('B'), ppm='B')
        code = run_mame(args.binary.resolve(), rompath, cold,
                        Path(__file__).with_name('nsb1_sms_draft_reload.lua').resolve(), 41, bios='671b')
        trace, reloaded = check_card_run(cold, code, 41, ppm='B')
        check_receive_activity(trace, 40)
        check_security_gate(trace, True)
        check_contacts(trace, reload=True)
        check_card(reloaded)
        if reloaded[SMS_OFFSET:SMS_OFFSET+2*RECORD_SIZE] != card[SMS_OFFSET:SMS_OFFSET+2*RECORD_SIZE]:
            raise ValueError('cold Outbox read changed SMS storage')
        if 'ins=dc' in trace:
            raise ValueError('cold Outbox read rewrote a card record')
        check_frame(cold/'snap/sms_reload_2.png', READ_HASH)
        if args.erase:
            erased,empty = output/'erase',output/'empty'
            rompath = prepare_cold_run(erased,cold,check_card,inputs_for_ppm('B'),ppm='B')
            code = run_mame(args.binary.resolve(),rompath,erased,
                            Path(__file__).with_name('nsb1_sms_draft_erase.lua').resolve(),49,True,bios='671b')
            trace,deleted = check_card_run(erased,code,49,ppm='B')
            check_receive_activity(trace,48)
            check_security_gate(trace,True)
            check_erased(trace,deleted,erased/'snap')
            expected = bytearray(reloaded)
            expected[SMS_OFFSET] = 0
            if deleted != bytes(expected):
                raise ValueError('SMS deletion changed bytes beyond its status')
            rompath = prepare_cold_run(empty,erased,check_deleted_card,inputs_for_ppm('B'),ppm='B')
            code = run_mame(args.binary.resolve(),rompath,empty,
                            Path(__file__).with_name('nsb1_sms_draft_empty.lua').resolve(),40,True,bios='671b')
            trace,final = check_card_run(empty,code,40,ppm='B')
            check_receive_activity(trace,39)
            check_security_gate(trace,True)
            check_empty(trace,final,empty/'snap')
            if final != deleted:
                raise ValueError('cold empty Outbox changed persisted card')
        (output/'result.json').write_text(json.dumps({
            'offline_sim_draft_roundtrip': True, 'ppm': 'B',
            'synthetic_dictionary_language': 1, 'network_sms_accepted': False,
            'native_editor_state_restored': args.restore_editor,
            'physical_erase_cold_empty':args.erase,
            'fitted_dsp_mask_proved': False}, indent=2)+'\n')
    except (ValueError, OSError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'NSB-1 draft roundtrip failed: {error}\n')
    scope = 'save/read/erase/cold empty Outbox' if args.erase else 'save/cold Outbox read'
    print(f'NSB-1 physical offline SIM draft {scope} PASS; no network SMS claim')


if __name__ == '__main__':
    main()

"""Own NSB-1 PPM B offline draft save/cold Outbox read; no radio SMS claim."""
import argparse
import json
from pathlib import Path
import re
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
        (output/'result.json').write_text(json.dumps({
            'offline_sim_draft_roundtrip': True, 'ppm': 'B',
            'synthetic_dictionary_language': 1, 'network_sms_accepted': False,
            'native_editor_state_restored': args.restore_editor,
            'fitted_dsp_mask_proved': False}, indent=2)+'\n')
    except (ValueError, OSError) as error:
        parser.exit(1, f'NSB-1 draft roundtrip failed: {error}\n')
    print('NSB-1 physical offline SIM draft save/cold Outbox read PASS; no network SMS claim')


if __name__ == '__main__':
    main()

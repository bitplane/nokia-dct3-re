"""Accept own NSB-1 physical SIM contact save and cold-process readback."""

import argparse
import json
from pathlib import Path
import re
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb1_rom4_compare import (
    INPUTS, check_card_run, check_frame, check_menu, check_receive_activity, check_security_gate,
    prepare, prepare_cold_run, run_mame,
)
from tools.sim_phonebook_check import (
    ADN_LENGTH, CURRENT_NVRAM_LENGTH, validate_phonebook, validate_phonebook_storage,
)


SAVED_RGB_SHA256 = '8c01f87b60e87cb07ab22ffe2418b7a4510554681ede41c423e85abecaca5231'
RELOADED_RGB_SHA256 = '2b55f3e9f92b0d1b7b3c06e51f5d1d080ce6e896cb0042f1ed5b98d5ee764aca'
SAVE_KEYS = ('down', 'menu', 'two', 'three', 'two', 'menu', 'one', 'two', 'three', 'menu')
ERASE_KEYS = ('down','down','down','menu','menu','menu','menu')
ERASE_CONFIRM = '1a6949a2e5808dda650df2687b8a4134bf573b0f27d7f284a4cf76eb5ccd443b'
EMPTY_BOOK = 'ad7a1d617d2b9646c17bdcddec9e3a313dc2acc9932ac6e24c455beb33a8267d'


def check_empty_storage(storage):
    if len(storage) != CURRENT_NVRAM_LENGTH or storage[:ADN_LENGTH] != b'\xff'*ADN_LENGTH:
        raise ValueError('phonebook is not durably erased')


def check_erased(trace,storage,snapshots,cold=False):
    phase,keys = ('cold',('menu','menu')) if cold else ('erase',ERASE_KEYS)
    contacts = re.findall(rf'nsb1_phonebook_erase: phase={phase} index=(\d+) key=([a-z]+) down=([01]) t=[0-9.]+ physical_only=1',trace)
    expected = [(str(index),key,down) for index,key in enumerate(keys,1) for down in ('1','0')]
    if contacts != expected:
        raise ValueError('physical erase/search contacts differ')
    check_empty_storage(storage)
    update = re.search(r'update fid=6f3a|ins=dc .*selected=6f3a',trace)
    if cold:
        if update or 'ins=b2 p1=01 p2=04 p3=20 selected=6f3a' not in trace:
            raise ValueError('cold empty search did not independently read without rewriting')
        check_frame(snapshots/'nsb1_phonebook_empty_1.png',EMPTY_BOOK)
    else:
        if ('ins=dc p1=01 p2=04 p3=20 selected=6f3a' not in trace or
                re.findall(r'update fid=6f3a record=(\d+) length=(\d+)',trace) != [('1','32')] or
                not re.search(r'SIM status ins=dc sw=9000\b',trace)):
            raise ValueError('erase did not commit exactly one record-one update')
        check_frame(snapshots/'nsb1_phonebook_erase_1.png',ERASE_CONFIRM)
        check_frame(snapshots/'nsb1_phonebook_erase_2.png',EMPTY_BOOK)


def check_saved(trace, storage, snapshots):
    contacts = re.findall(r'nsb1_phonebook_contact: index=(\d+) key=([a-z]+) down=([01]) t=[0-9.]+ physical_only=1', trace)
    expected = [(str(index), key, down) for index, key in enumerate(SAVE_KEYS, 1) for down in ('1', '0')]
    if contacts != expected:
        raise ValueError('physical contact-save sequence missing or changed')
    validate_phonebook(trace, storage)
    check_frame(snapshots / 'nsb1_phonebook_5.png', SAVED_RGB_SHA256)


def check_reloaded(trace, storage, snapshots):
    contacts = re.findall(r'nsb1_phonebook_search: index=(\d+) down=([01]) t=[0-9.]+ physical_only=1', trace)
    if contacts != [('1', '1'), ('1', '0'), ('2', '1'), ('2', '0')]:
        raise ValueError('physical cold-search sequence missing or changed')
    if 'ins=b2 p1=01 p2=04 p3=20 selected=6f3a' not in trace:
        raise ValueError('cold firmware did not read EF_ADN record one')
    if re.search(r'update fid=6f3a|ins=dc .*selected=6f3a', trace):
        raise ValueError('cold-search phase rewrote the contact')
    validate_phonebook_storage(storage)
    check_frame(snapshots / 'nsb1_phonebook_reloaded.png', RELOADED_RGB_SHA256)


def prepare_reload(run, source):
    return prepare_cold_run(run, source, validate_phonebook_storage, INPUTS)


def check_phase(run, returncode, seconds):
    trace, storage = check_card_run(run, returncode, seconds)
    check_receive_activity(trace, seconds - 1)
    check_security_gate(trace, True)
    # Subsequent physical actions change the selection before parent frame 3.
    check_menu(trace, run / 'snap', frame_indices=(2,))
    return trace, storage


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('rom_root', type=Path)
    parser.add_argument('run_dir', type=Path)
    parser.add_argument('--checksum-control-from', required=True, type=Path)
    parser.add_argument('--erase',action='store_true',help='extend through physical deletion and cold empty-book search')
    args = parser.parse_args()
    try:
        binary, root, run = args.binary.resolve(), args.rom_root.resolve(), args.run_dir.resolve()
        if run.exists():
            raise ValueError('round trip requires a fresh directory')
        save, reload = run / 'save', run / 'reload'
        rompath = prepare(root, save, args.checksum_control_from.resolve(), True, True)
        code = run_mame(binary, rompath, save, Path(__file__).with_name('nsb1_phonebook_fixture.lua').resolve(), 50, True)
        trace, written = check_phase(save, code, 50)
        check_saved(trace, written, save / 'snap')
        rompath = prepare_reload(reload, save)
        code = run_mame(binary, rompath, reload, Path(__file__).with_name('nsb1_phonebook_reload.lua').resolve(), 40, True)
        trace, recovered = check_phase(reload, code, 40)
        check_reloaded(trace, recovered, reload / 'snap')
        if written[:1600] != recovered[:1600]:
            raise ValueError('cold search changed EF_ADN contents')
        if args.erase:
            erased,empty = run/'erase',run/'empty'
            rompath = prepare_reload(erased,reload)
            code = run_mame(binary,rompath,erased,Path(__file__).with_name('nsb1_phonebook_erase.lua').resolve(),43,True)
            trace,deleted = check_phase(erased,code,43)
            check_erased(trace,deleted,erased/'snap')
            if recovered[ADN_LENGTH:] != deleted[ADN_LENGTH:]:
                raise ValueError('erase changed unrelated persisted SIM files')
            rompath = prepare_cold_run(empty,erased,check_empty_storage,INPUTS)
            code = run_mame(binary,rompath,empty,Path(__file__).with_name('nsb1_phonebook_empty.lua').resolve(),40,True)
            trace,final = check_phase(empty,code,40)
            check_erased(trace,final,empty/'snap',cold=True)
            if deleted != final:
                raise ValueError('cold empty search changed persisted SIM contents')
        (run / 'result.json').write_text(json.dumps({'physical_save': True, 'cold_search': True,
            'physical_erase_cold_empty':args.erase,
            'contact': 'ADA/123', 'donor_storage': False, 'fitted_mask_proved': False}, indent=2) + '\n')
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'NSB-1 phonebook round trip failed: {error}\n')
    scope = 'save/search/erase/cold empty search' if args.erase else 'save and cold search'
    print(f'NSB-1 physical ADA/123 {scope} PASS; compatibility/nominal-storage limits retained; evidence={run}')


if __name__ == '__main__':
    main()

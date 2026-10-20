"""Accept own NSB-1 physical SIM contact save and cold-process readback."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb1_rom4_compare import (
    INPUTS, check_menu, check_native_execution, check_security_gate, check_trace,
    prepare, run_mame,
)
from tools.sim_phonebook_check import validate_phonebook, validate_phonebook_storage


SAVED_RGB_SHA256 = '8c01f87b60e87cb07ab22ffe2418b7a4510554681ede41c423e85abecaca5231'
RELOADED_RGB_SHA256 = '2b55f3e9f92b0d1b7b3c06e51f5d1d080ce6e896cb0042f1ed5b98d5ee764aca'
SAVE_KEYS = ('down', 'menu', 'two', 'three', 'two', 'menu', 'one', 'two', 'three', 'menu')


def check_frame(path, expected):
    from PIL import Image
    with Image.open(path) as source:
        frame = source.convert('RGB')
        if frame.size != (84, 48) or hashlib.sha256(frame.tobytes()).hexdigest() != expected:
            raise ValueError(f'phonebook frame mismatch: {path.name}')


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
    if run.exists():
        raise ValueError('cold reload requires a fresh directory')
    manifest = json.loads((source / 'input_manifest.json').read_text())
    if (manifest.get('inputs') != [{'source': name, 'sha1': digest} for name, digest in INPUTS]
            or not manifest.get('synthetic_sim_inserted') or not manifest.get('synthetic_retry_initialized')
            or manifest.get('external_eeprom') != 'diagnostic checksum/retry control; calibration/identity not validated'):
        raise ValueError('cold reload requires qualified own synthetic save provenance')
    for name, digest in INPUTS:
        if hashlib.sha1((source / 'inputs/nsb1r4t' / Path(name).name).read_bytes()).hexdigest() != digest:
            raise ValueError('cold reload executable/resident inputs changed')
    storage = source / 'nvram/nsb1r4t'
    files = {name: (storage / name).read_bytes() for name in ('flash', 'eeprom', 'sim_card')}
    if hashlib.sha1(files['flash']).hexdigest() != INPUTS[0][1] or len(files['eeprom']) != 8192:
        raise ValueError('cold reload executable or EEPROM geometry changed')
    validate_phonebook_storage(files['sim_card'])
    target = run / 'nvram/nsb1r4t'
    target.mkdir(parents=True)
    shutil.copytree(source / 'inputs', run / 'inputs')
    shutil.copytree(source / 'cfg', run / 'cfg')
    for name, data in files.items():
        (target / name).write_bytes(data)
    manifest['cold_source_run'] = str(source.resolve())
    manifest['cold_storage_sha256'] = {name: hashlib.sha256(data).hexdigest() for name, data in files.items()}
    (run / 'input_manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return run / 'inputs'


def check_phase(run, returncode, seconds):
    image = (run / 'inputs/nsb1r4t/5190f671a.fls').read_bytes()
    trace = (run / 'error.log').read_text()
    check_trace(trace, image[0xb9350:0xb9350 + 372], image[0xbe570:0xbe570 + 372], returncode)
    check_native_execution(trace, seconds - 1)
    check_security_gate(trace, True)
    # Subsequent physical actions change the selection before parent frame 3.
    check_menu(trace, run / 'snap', frame_indices=(2,))
    if (run / 'nvram/nsb1r4t/flash').read_bytes() != image:
        raise ValueError('phonebook phase changed executable flash')
    if len((run / 'nvram/nsb1r4t/eeprom').read_bytes()) != 8192:
        raise ValueError('phonebook phase changed EEPROM geometry')
    return trace, (run / 'nvram/nsb1r4t/sim_card').read_bytes()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('rom_root', type=Path)
    parser.add_argument('run_dir', type=Path)
    parser.add_argument('--checksum-control-from', required=True, type=Path)
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
        (run / 'result.json').write_text(json.dumps({'physical_save': True, 'cold_search': True,
            'contact': 'ADA/123', 'donor_storage': False, 'fitted_mask_proved': False}, indent=2) + '\n')
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'NSB-1 phonebook round trip failed: {error}\n')
    print(f'NSB-1 physical ADA/123 save and cold search PASS; compatibility/nominal-storage limits retained; evidence={run}')


if __name__ == '__main__':
    main()

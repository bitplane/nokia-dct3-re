"""Physical NSK-1 SIM contact save and independently qualified cold search."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsk1_rom4_compare import prepare,INPUTS
from tools.nsk1_security_compare import check as check_security,check_phase,run_machine
from tools.sim_phonebook_check import validate_phonebook,validate_phonebook_storage,ADN_LENGTH,CURRENT_NVRAM_LENGTH


SAVED_RGB_SHA256 = '8c01f87b60e87cb07ab22ffe2418b7a4510554681ede41c423e85abecaca5231'
RECALLED_RGB_SHA256 = '22e30aaccff2be9f4f9628a8033d6e6c15b6615f2082af9de5a04ff1ba952a63'
SAVE_KEYS = ('down','menu','two','three','two','menu','one','two','three','menu')
SAVE_CODES = (0x18,0x19,2,3,2,0x19,1,2,3,0x19)
SEARCH_KEYS = ('menu','two','menu')
SEARCH_CODES = (0x19,2,0x19)
ERASE_KEYS = ('down','down','menu','menu','menu','menu')
ERASE_CODES = (0x18,0x18,0x19,0x19,0x19,0x19)
ERASE_CONFIRM = '1a6949a2e5808dda650df2687b8a4134bf573b0f27d7f284a4cf76eb5ccd443b'
EMPTY_BOOK = 'ad7a1d617d2b9646c17bdcddec9e3a313dc2acc9932ac6e24c455beb33a8267d'
EMPTY_SEARCH = '3600e3d1cab564b185d03403552ea401e1e76edd07b110184716700828a5df2a'


def frame_check(path, expected):
    from PIL import Image
    with Image.open(path) as source:
        frame = source.convert('RGB')
        if frame.size != (84,48) or hashlib.sha256(frame.tobytes()).hexdigest() != expected:
            raise ValueError(f'own phonebook frame differs: {path.name}')


def check_contacts(trace, marker, keys):
    rows = re.findall(rf'{marker}: index=(\d+) key=([a-z]+) down=([01]) t=[0-9.]+ physical_only=1',trace)
    expected = [(str(index),key,down) for index,key in enumerate(keys,1) for down in ('1','0')]
    if rows != expected or len(rows) != trace.count(marker+':'):
        raise ValueError('physical phonebook workflow contacts differ')


def check_saved(run):
    initial = (run/'initial_eeprom.bin').read_bytes()
    summary,trace = check_phase(run,initial,through=50)
    summary['physical_security'] = check_security(trace,run/'snap',True,SAVE_CODES)
    check_contacts(trace,'nsk1_phonebook_contact',SAVE_KEYS)
    card = (run/'nvram/nsk1r4t/sim_card').read_bytes()
    validate_phonebook(trace,card)
    if (re.findall(r'update fid=6f3a record=(\d+) length=(\d+)',trace) != [('1','32')]
            or not re.search(r'SIM status ins=dc sw=9000\b',trace)):
        raise ValueError('expected exactly one successful physical ADN update')
    frame_check(run/'snap/nsk1_phonebook_5.png',SAVED_RGB_SHA256)
    summary['physical_phonebook_save'] = True
    summary['final_card_sha256'] = hashlib.sha256(card).hexdigest()
    return summary,card


def prepare_reload(run, source):
    if run.exists():
        raise ValueError('cold contact read requires a fresh directory')
    # Revalidate raw source evidence instead of trusting a saved PASS field.
    check_saved(source)
    manifest = json.loads((source/'input_manifest.json').read_text())
    expected = [{'source':name,'sha1':digest} for name,digest in INPUTS]
    if (manifest.get('inputs') != expected or manifest.get('adc2_source') != 554
            or manifest.get('inserted_synthetic_sim') is not True
            or manifest.get('peer_completions') is not False
            or not isinstance(manifest.get('phone_retry'),dict)
            or manifest['phone_retry'].get('after') != 0):
        raise ValueError('cold read source is not the qualified own input composition')
    return copy_persistent(run,source)


def copy_persistent(run, source):
    if run.exists():
        raise ValueError('cold continuation requires a fresh directory')
    for name in ('inputs','nvram','cfg'):
        shutil.copytree(source/name,run/name)
    initial = (run/'nvram/nsk1r4t/eeprom').read_bytes()
    (run/'initial_eeprom.bin').write_bytes(initial)
    provenance = {'classification':'cold continuation of qualified own persistent-input workflow',
                  'source_manifest_sha256':hashlib.sha256((source/'input_manifest.json').read_bytes()).hexdigest(),
                  'source_card_sha256':hashlib.sha256((source/'nvram/nsk1r4t/sim_card').read_bytes()).hexdigest(),
                  'copied_directories':['inputs','nvram','cfg'],'ram_or_save_state_copied':False}
    (run/'input_manifest.json').write_text(json.dumps(provenance,indent=2)+'\n')
    return initial


def check_reloaded(run, initial, saved_card):
    summary,trace = check_phase(run,initial,through=40,minimum_records=1)
    summary['physical_security'] = check_security(trace,run/'snap',True,SEARCH_CODES)
    check_contacts(trace,'nsk1_phonebook_search',SEARCH_KEYS)
    card = (run/'nvram/nsk1r4t/sim_card').read_bytes()
    validate_phonebook_storage(card)
    if (card != saved_card or 'ins=b2 p1=01 p2=04 p3=20 selected=6f3a' not in trace
            or re.search(r'update fid=6f3a|ins=dc .*selected=6f3a',trace)):
        raise ValueError('cold lookup did not preserve/read the complete card without contact rewrite')
    for index in (1,2,3):
        frame_check(run/f'snap/nsk1_phonebook_reloaded_{index}.png',RECALLED_RGB_SHA256)
    summary['physical_cold_contact_search'] = True
    return summary


def check_empty(run, initial, previous, cold=False):
    summary,trace = check_phase(run,initial,through=40 if cold else 50,minimum_records=1)
    summary['physical_security'] = check_security(trace,run/'snap',True,SEARCH_CODES if cold else ERASE_CODES)
    check_contacts(trace,'nsk1_phonebook_empty' if cold else 'nsk1_phonebook_erase',SEARCH_KEYS if cold else ERASE_KEYS)
    card = (run/'nvram/nsk1r4t/sim_card').read_bytes()
    if (len(card) != CURRENT_NVRAM_LENGTH or card[:ADN_LENGTH] != b'\xff'*ADN_LENGTH
            or len(previous) != len(card) or card[ADN_LENGTH:] != previous[ADN_LENGTH:]):
        raise ValueError('empty ADN or unrelated SIM storage differs')
    if cold:
        if (card != previous or 'ins=b2 p1=01 p2=04 p3=20 selected=6f3a' not in trace
                or re.search(r'update fid=6f3a|ins=dc .*selected=6f3a',trace)):
            raise ValueError('cold empty search rewrote storage or omitted the card read')
        frame_check(run/'snap/nsk1_phonebook_empty_1.png',EMPTY_BOOK)
        for index in (2,3):
            frame_check(run/f'snap/nsk1_phonebook_empty_{index}.png',EMPTY_SEARCH)
    else:
        validate_phonebook_storage(previous)
        if ('ins=dc p1=01 p2=04 p3=20 selected=6f3a' not in trace
                or re.findall(r'update fid=6f3a record=(\d+) length=(\d+)',trace) != [('1','32')]
                or not re.search(r'SIM status ins=dc sw=9000\b',trace)):
            raise ValueError('physical erase did not commit exactly one record-one update')
        frame_check(run/'snap/nsk1_phonebook_erase_4.png',ERASE_CONFIRM)
        frame_check(run/'snap/nsk1_phonebook_erase_5.png',EMPTY_BOOK)
    summary['physical_cold_empty_search' if cold else 'physical_phonebook_erase'] = True
    return summary,card


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary',type=Path)
    parser.add_argument('roms',type=Path)
    parser.add_argument('capture',type=Path)
    parser.add_argument('run',type=Path)
    parser.add_argument('--erase',action='store_true',help='extend through physical deletion and cold empty search')
    args = parser.parse_args()
    run = args.run.resolve()
    try:
        if run.exists():
            raise ValueError('contact round trip requires a fresh directory')
        save,cold = run/'save',run/'cold'
        initial = prepare(args.roms.resolve(),save,args.capture.resolve(),True,True,True,True,True)
        (save/'initial_eeprom.bin').write_bytes(initial)
        command = run_machine(args.binary,save,Path(__file__).with_name('nsk1_phonebook_save.lua'),51,True)
        summary,card = check_saved(save)
        summary['command'] = command
        (save/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
        seed = prepare_reload(cold,save)
        command = run_machine(args.binary,cold,Path(__file__).with_name('nsk1_phonebook_reload.lua'),41,True)
        summary = check_reloaded(cold,seed,card)
        summary['command'] = command
        (cold/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
        if args.erase:
            erased,empty = run/'erase',run/'empty'
            seed = prepare_reload(erased,save)
            command = run_machine(args.binary,erased,Path(__file__).with_name('nsk1_phonebook_erase.lua'),51,True)
            summary,deleted = check_empty(erased,seed,card)
            summary['command'] = command
            (erased/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
            # Copy only after the preceding raw trace/storage/frame checks pass.
            seed = copy_persistent(empty,erased)
            command = run_machine(args.binary,empty,Path(__file__).with_name('nsk1_phonebook_empty.lua'),41,True)
            summary,_ = check_empty(empty,seed,deleted,cold=True)
            summary['command'] = command
            (empty/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
        (run/'result.json').write_text(json.dumps({'physical_save':True,'physical_cold_search':True,
            'physical_erase_cold_empty':args.erase,
            'contact':'ADA/123','whole_card_preserved_on_read':True,'ram_or_save_state_copied':False,
            'scope':'native compatibility SIM contact persistence; no fitted-mask or network acceptance'},indent=2)+'\n')
        print('NSK-1 physical ADA/123 save and cold search'+(' / erase and cold empty search' if args.erase else '')+' PASS; native compatibility limits retained')
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        parser.exit(1,f'NSK-1 contact round trip failed: {error}; inspect {run}\n')


if __name__ == '__main__':
    main()

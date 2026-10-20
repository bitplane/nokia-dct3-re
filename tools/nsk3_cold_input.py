"""Copy only NSK-3 persistent inputs, recording their pre-launch bytes/hashes."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsk3_static_check import inspect


def prepare(source,seed,output):
    if output.exists():raise ValueError('cold continuation requires a fresh directory')
    _,image=inspect(source,'boot4')
    storage=seed/'nvram/nsk3r4t'
    if (storage/'flash').read_bytes()!=image or len((storage/'eeprom').read_bytes())!=8192:
        raise ValueError('requires own NSK-3 flash and 8 KiB persistent EEPROM')
    hashes={name:hashlib.sha256((storage/name).read_bytes()).hexdigest()
            for name in ('flash','eeprom','sim_card')}
    shutil.copytree(seed/'nvram',output/'nvram')
    shutil.copytree(seed/'cfg',output/'cfg')
    for name in ('eeprom','sim_card'):
        shutil.copyfile(storage/name,output/f'initial_{name}.bin')
    (output/'cold_input.json').write_text(json.dumps({
        'source':str(seed.resolve()),'source_sha256':hashes,
        'copied_directories':['nvram','cfg'],'ram_or_save_state_copied':False},indent=2)+'\n')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','seed','output'):parser.add_argument(name,type=Path)
    args=parser.parse_args()
    try:prepare(args.source,args.seed,args.output)
    except (ValueError,OSError) as error:
        print(f'NSK-3 cold preparation failed: {error}',file=sys.stderr)
        raise SystemExit(1)

"""Qualify observed EEPROM-backed name-buffer replacement, not a cache repair."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsm1_v523_static_check import decode_group7,SHA1


def check(trace,template,image):
    if hashlib.sha1(image).hexdigest()!=SHA1:raise ValueError('own executable differs')
    if hashlib.sha1(template).hexdigest()!='07335e492f72ba9da9c3436890c9eb4af67fd9a9':
        raise ValueError('own repair-template control differs')
    record=next(r for r in decode_group7(image) if r['id']==0x728)
    if record['offset']!=0x19d4 or record['size']!=37:raise ValueError('own record descriptor differs')
    return check_capture(trace,template[0x19d4:0x19d4+32])


def check_capture(trace,record):
    requests=re.findall(r'nsm1_name_record_request: pointer=([0-9a-f]+) record=([0-9a-f]+) selector=([0-9a-f]+) offset=([0-9a-f]+)',trace)
    if len(requests)!=trace.count('nsm1_name_record_request: ') or requests!=[('001137a0','00000728','00007ff0',f'{i:08x}') for i in (0,1)]:
        raise ValueError('name record requests differ')
    results=re.findall(r'nsm1_name_record_result: result=([0-9a-f]+) data=([0-9a-f]+)',trace)
    raw=record.hex()
    if len(results)!=trace.count('nsm1_name_record_result: ') or results!=[('00000001',raw)]*2:raise ValueError('name bytes differ from original template')
    if len(record)!=32 or record[16:20]!=bytes.fromhex('2a079700'):
        raise ValueError('observed name prefix differs')
    if not re.search(r'nsm1_name_write: .*value=41000000 .*pc=002042a2 ',trace):
        raise ValueError('initial A string not observed')
    if len(re.findall(r'nsm1_name_write: .*value=2a079700 mask=ffffffff pc=002c3a0e .*caller=00259eed ',trace))!=2:
        raise ValueError('EEPROM-backed buffer replacement differs')
    return {'record':'0x0728','offset':'0x19d4','name_prefix':'2a079700',
            'template_source_proved':True,'cache_repair_proved':False}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('trace','template','image'):parser.add_argument(name,type=Path)
    args=parser.parse_args()
    try:print(check(args.trace.read_text(),args.template.read_bytes(),args.image.read_bytes()))
    except (ValueError,OSError) as error:
        print(f'NSM-1 name-source qualification failed: {error}',file=sys.stderr);return 1
    return 0


if __name__=='__main__':raise SystemExit(main())

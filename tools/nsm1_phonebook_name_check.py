"""Qualify observed EEPROM-backed name-buffer replacement, not a cache repair."""
import argparse
import hashlib
from pathlib import Path
import re
import sys
from PIL import Image

SELECTED='8b953af1aa7385609df59fee4b01998ead9cc31ac1a044fac208b01d0a7aac7f'
CLEAN='695c5291d789ebaf9c11583885e0183373313ee4e3fdd5ee16c9aee36d957c8b'

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsm1_v523_static_check import decode_group7,SHA1
from tools.nsk1_phonebook_roundtrip import frame_check


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
            'template_source_proved':True,'sim_name_corruption_proved':False}


def check_namespace(trace):
    rows=re.findall(r'nsm1_name_format_entry: mode=([0-9a-f]+) index=([0-9a-f]+) data=([0-9a-f]+) output=([0-9a-f]+) caller=([0-9a-f]+)',trace)
    if rows!=[(f'{mode:08x}',f'{index:08x}','00111ce8','00111cb8','0025b4c7') for mode,index in ((2,1),(1,1),(1,2))]:
        raise ValueError('SIM/EEPROM formatter mode sequence differs')
    return {'selected_mode':2,'extra_eeprom_rows':2,'selected_name_missing':False}


def check_list(before,after):
    bands=[]
    for path in (before,after):
        with Image.open(path) as source:
            im=source.convert('RGB')
            if im.size!=(84,48):raise ValueError('list geometry differs')
            # The A is white-on-black in this selected row, not a missing label.
            band=bytes(value for y in range(7,17) for x in range(84) for value in im.getpixel((x,y)))
        if hashlib.sha256(band).hexdigest()!=SELECTED:raise ValueError('selected A row differs')
        bands.append(band)
    if bands[0]!=bands[1]:raise ValueError('Phone erasure changed selected SIM name')
    frame_check(after,CLEAN)
    return {'selected_sim_name':'A','selected_row_unchanged':True,'clean_post_erase_list':True}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('trace','template','image'):parser.add_argument(name,type=Path)
    parser.add_argument('--namespace',action='store_true')
    parser.add_argument('--before-list',type=Path)
    parser.add_argument('--after-list',type=Path)
    args=parser.parse_args()
    try:
        trace=args.trace.read_text()
        print(check(trace,args.template.read_bytes(),args.image.read_bytes()))
        if args.namespace:print(check_namespace(trace))
        if bool(args.before_list)!=bool(args.after_list):raise ValueError('before/after list images must be paired')
        if args.before_list:print(check_list(args.before_list,args.after_list))
    except (ValueError,OSError) as error:
        print(f'NSM-1 name-source qualification failed: {error}',file=sys.stderr);return 1
    return 0


if __name__=='__main__':raise SystemExit(main())

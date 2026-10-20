"""Inspect own NSK-1 startup/catalogue structure; no runtime or mask claim."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.dct3_eeprom_descriptor import eeprom_geometry


PROFILES = {
    'standard': ('a2a99ec6c52104eebcbae281cf8e845bdc8ea1d9',0x296d10),
    '2915': ('64068df6b7950b47f34ac141d0ef9c51dc666cca',0x296d00),
}


def read_catalogue(image,table):
    offset = table-0x200000
    if offset < 8 or offset+112 > len(image):
        raise ValueError('catalogue table outside image')
    size,destination = struct.unpack_from('>II',image,offset-8)
    if size != 112 or not 0x100000 <= destination <= 0x110000-size:
        raise ValueError('catalogue initializer bounds differ')
    pointers = struct.unpack_from('>28I',image,offset)
    if [i for i,pointer in enumerate(pointers) if not pointer] != [11,26,27]:
        raise ValueError('catalogue null-slot layout differs')
    entries = []
    for index,pointer in enumerate(pointers):
        if not pointer:
            continue
        start = pointer-0x200000
        if start < 0 or start+12 > len(image) or pointer & 1:
            raise ValueError('descriptor pointer outside image or unaligned')
        header = struct.unpack_from('>6H',image,start)
        end = start+12+header[2]*2
        if end > len(image) or header[0]+header[2] > 0x10000:
            raise ValueError('descriptor payload or declared destination outside bounds')
        entries.append({'slot':index,'descriptor':hex(pointer),'header':list(header),
                        'payload_sha1':hashlib.sha1(image[start+12:end]).hexdigest()})
    return {'initializer_destination':hex(destination),'slots':28,'nonzero':len(entries),
            'entries':entries,'execution_or_mask_identity_proved':False}


def inspect(image,variant):
    if variant not in PROFILES:
        raise ValueError('unknown NSK-1 variant')
    digest,table = PROFILES[variant]
    if len(image) != 0x100000 or hashlib.sha1(image).hexdigest() != digest:
        raise ValueError('requires own pinned NSK-1 normalized variant')
    catalogue = read_catalogue(image,table)
    if catalogue['entries'][-1]['header'] != [0x0f00,0,186,0x0700,180,0]:
        raise ValueError('bootstrap descriptor differs')
    # Own ARM ADD/BX pair enters Thumb at 2000ec, not at flash reset entry.
    if image[0xe4:0xec] != bytes.fromhex('e28f0001e12fff10'):
        raise ValueError('startup interworking boundary differs')
    return {'variant':variant,'sha1':digest,'catalogue':catalogue,
            'arm_entry':'0x200040','thumb_entry':'0x2000ec',
            'firmware_eeprom_geometry':eeprom_geometry(image[5]),
            'stack_literal':hex(int.from_bytes(image[0x16c:0x170],'big')),
            'boot_or_fitted_hardware_proved':False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image',type=Path)
    parser.add_argument('--variant',choices=PROFILES,default='standard')
    args = parser.parse_args()
    try:
        result = inspect(args.image.read_bytes(),args.variant)
    except (OSError,ValueError,struct.error) as error:
        parser.exit(1,f'NSK-1 static check failed: {error}\n')
    print(json.dumps(result,indent=2))


if __name__ == '__main__':
    main()

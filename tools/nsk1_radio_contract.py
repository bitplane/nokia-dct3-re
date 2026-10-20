"""Own NSK-1 standard radio receive/activation contracts and bounded references."""
import argparse
import hashlib
import json
from pathlib import Path

from tools.nsk1_static_check import PROFILES
from tools.nse5_roller_contract import thumb_bl_candidates


BASE = 0x200000


def literal_references(image, targets):
    pools = {BASE+offset:int.from_bytes(image[offset:offset+4],'big')
             for offset in range(0,len(image)-3,4)
             if int.from_bytes(image[offset:offset+4],'big') in targets}
    loads = []
    for offset in range(0,len(image)-3,2):
        word = int.from_bytes(image[offset:offset+2],'big')
        if word & 0xf800 != 0x4800:
            continue
        address = BASE+offset
        pool = ((address+4)&~3)+(word & 255)*4
        if pool in pools:
            loads.append({'address':hex(address),'pool':hex(pool),'value':hex(pools[pool])})
    return {'aligned_pools':[{'address':hex(address),'value':hex(value)} for address,value in pools.items()],
            'thumb16_literal_load_candidates':loads,
            'aligned_halfwords_scanned':len(range(0,len(image)-3,2)),
            'scope':'aligned pools and Thumb-16 load encodings; not reachable-code or computed-alias closure'}


def analyze(image):
    if hashlib.sha1(image).hexdigest() != PROFILES['standard'][0]:
        raise ValueError('radio contract requires own standard NSK-1 image')
    def word(address):
        return int.from_bytes(image[address-BASE:address-BASE+4],'big')
    checks = {0x2941e4:0x294210,0x26a534:0x13b8,0x218c58:0x13a3,
              0x218950:0x218996,0x218c1c:0x043f,0x24d7dc:0x0834,
              0x26e234:0x03e9,0x26e230:0x10be77}
    for address,value in checks.items():
        if word(address) != value:
            raise ValueError(f'own dispatch/control literal differs at {address:08x}')
    fragments = {0x26a440:'b5101c04483b80201c2021a8',
                 0x2177e2:'486b27017507',0x21884c:'4edb2003',
                 0x26df9a:'4825794170a1790070e0'}
    for address,encoded in fragments.items():
        expected = bytes.fromhex(encoded)
        if image[address-BASE:address-BASE+len(expected)] != expected:
            raise ValueError(f'own radio instruction fragment differs at {address:08x}')
    stores = [hex(BASE+offset) for offset in range(0x13000,0x18e00,2)
              if ((encoded:=int.from_bytes(image[offset:offset+2],'big')) & 0xf800 == 0x7000
                  and (encoded >> 6) & 31 == 20)]
    calls = thumb_bl_candidates(image,(0x2177a0,))[0x2177a0]
    return {'image_sha1':hashlib.sha1(image).hexdigest(),
            'rx_route':{'type':'0x8b','table':'0x2941c4','index':8,'handler':'0x26a440',
                        'status':'0x13b8','task':11,'consumer':'0x218996'},
            'activation':{'input':'0x043f','handler':'0x218d20','initializer':'0x2177a0',
                          'producer_input':'0x0834','producer_handler':'0x24df64',
                          'constructor':'0x26def0','control_bytes':['0x10be7c','0x10be7b']},
            'owner_references':literal_references(image,{0x107158,0x10716c}),
            'activation_input_references':literal_references(image,{0x043f,0x0834}),
            'member_store_candidates':{'range':['0x213000','0x218e00'],'offset':20,'addresses':stores,
                                       'scope':'Thumb-16 immediate STRB encoding candidates; includes other owners'},
            'initializer_direct_bl_candidates':[hex(address) for address in calls],
            'limitations':['computed aliases and generic structure copies unclosed','indirect/ARM callers unclosed',
                           'control-byte provisioning unproved','no RF payload semantics or fitted mask proof']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image',type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(analyze(args.image.read_bytes()),indent=2))
    except (OSError,ValueError) as error:
        parser.exit(1,f'NSK-1 radio contract failed: {error}\n')


if __name__ == '__main__':
    main()

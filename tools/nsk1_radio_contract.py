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
              0x26e234:0x03e9,0x26e230:0x10be77,0x2086c4:0x10a444,
              0x20c914:0x0834,0x20c970:0x20a590,0x221374:0x09ec,
              0x26dec4:0x07d9,0x24e074:0x24dd0a,0x24df58:0x07e5,
              0x24d6e0:0x07d6,0x26df38:0x26e20c,
              0x2941e0:0x294218,0x26a500:0x1390,0x26a504:0x108a84,
              0x2128d8:0x1389,0x2125c8:0x212b28}
    for address,value in checks.items():
        if word(address) != value:
            raise ValueError(f'own dispatch/control literal differs at {address:08x}')
    fragments = {0x26a440:'b5101c04483b80201c2021a8',
                 0x2177e2:'486b27017507',0x21884c:'4edb2003',
                 0x26df9a:'4825794170a1790070e0',
                 0x20cb58:'20a3010146484288d100e53d',
                 0x208588:'484e7fc170a17f0070e0',
                 0x2091b2:'78a070682801',
                 0x20bd02:'203f014146484288',
                 0x2212ce:'200fe7f9'}
    for address,encoded in fragments.items():
        expected = bytes.fromhex(encoded)
        if image[address-BASE:address-BASE+len(expected)] != expected:
            raise ValueError(f'own radio instruction fragment differs at {address:08x}')
    stores = [hex(BASE+offset) for offset in range(0x13000,0x18e00,2)
              if ((encoded:=int.from_bytes(image[offset:offset+2],'big')) & 0xf800 == 0x7000
                  and (encoded >> 6) & 31 == 20)]
    calls = thumb_bl_candidates(image,(0x2177a0,))[0x2177a0]
    # MOVS r0,#a3; LSLS r0,r0,#4 followed by a direct BL is a bounded
    # producer pattern, not closure of data-driven or indirect input creation.
    pattern_sites = [BASE+offset for offset in range(0,len(image)-7,2)
                     if image[offset:offset+4] == bytes.fromhex('20a30100')]
    upstream_calls = thumb_bl_candidates(image,(0x221236,))[0x221236]
    producers = [site+4 for site in pattern_sites if site+4 in upstream_calls]
    completion_calls = thumb_bl_candidates(image,(0x26dc28,))[0x26dc28]
    completion_producers = [BASE+offset+10 for offset in range(0,len(image)-13,2)
                            if image[offset:offset+4] == bytes.fromhex('203f0140')
                            and BASE+offset+10 in completion_calls]
    return {'image_sha1':hashlib.sha1(image).hexdigest(),
            'rx_route':{'type':'0x8b','table':'0x2941c4','index':8,'handler':'0x26a440',
                        'status':'0x13b8','task':11,'consumer':'0x218996'},
            'activation':{'input':'0x043f','handler':'0x218d20','initializer':'0x2177a0',
                          'producer_input':'0x0834','producer_handler':'0x24df64',
                          'constructor':'0x26def0','control_bytes':['0x10be7c','0x10be7b']},
            'upstream_activation':{'selector':'0x20cb58','input':'0x0a30',
                                   'status_constructor':'0x2081d8','control_bytes':['0x10a463','0x10a460'],
                                   'a30_constructor':'0x221236','target_task':15,
                                   'literal_shift_direct_bl_producers':[hex(address) for address in producers],
                                   'scope':'MOVS/LSLS/direct-BL pattern only; not all argument or descriptor sources'},
            'mode_request':{'input':'0x09ec','post_task':15,'receive':'0x2092f6',
                            'mode_handler':'0x2091b2','owner':'0x10a444',
                            'main_dispatch':'0x20c950','state6_handler':'0x20a590',
                            'scope':'static mode-storage/state-6 selection contract; runtime state requires its own trace'},
            'continuation_wait':{'entry':'0x20bbf4','receive_call':'0x20bcfc',
                                 'comparison':'0x20bd02','success_input':'0x07e0',
                                 'completion_constructor':'0x26dc28','destination_task':15,
                                 'literal_shift_direct_bl_producers':[hex(address) for address in completion_producers],
                                 'scope':'one explicit success comparison; other lifecycle exits not closed'},
            'nested_request':{'input':'0x07d6','task':16,'dispatcher':'0x24dff8',
                              'state29_handler':'0x24dd0a','expected_input':'0x07e5',
                              'request_constructor':'0x26def0','request':'0x03e9','destination_task':10,
                              'scope':'static nested route; runtime values require observation'},
            'task10_scan':{'request_handler':'0x2132e8','pending_owner':'0x108a84',
                           'pending_flag_bit':25,'immediate_processor':'0x212588',
                           'mode1_entry':'0x2127f2','builder':'0x2112b0','builder_mode':2,
                           'post':'0x2114f4','transport_task':3,'packet_type':'0x1a',
                           'scope':'static path; no registration or completion claim'},
            'candidate_completion':{'rx_type':'0x8a','route':'0x26a2ec','status':'0x1390',
                                    'task':10,'processor_case':'0x212b28',
                                    'scope':'static counter-processing route; relation to required completion unproved'},
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

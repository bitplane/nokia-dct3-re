"""Own NSK-1 standard radio receive/activation contracts and bounded references."""
import argparse
import hashlib
import json
from pathlib import Path

from tools.nsk1_static_check import PROFILES
from tools.nse5_roller_contract import thumb_bl_candidates
from tools.c54x_rom4_port_census import direct_callers


BASE = 0x200000


def native_completion_contract(image, data=None):
    if hashlib.sha1(image).hexdigest() != 'a05a1e96a8c36ec5a47e1ea059d15afa54ca5739':
        raise ValueError('native completion contract requires acquired NSE-1 ROM4 program')
    expected = bytes.fromhex('61f8 1949 0080 f830 3153 7713 1200 10f8 1867 8083 7715 0002 7713 1200 f274 37ce 7714 008a')
    if image[0x313f*2:0x313f*2+len(expected)] != expected:
        raise ValueError('native type-8a publisher sequence differs')
    selector = bytes.fromhex('e80f 08f8 0012 880e fe00 34f8 06d1')
    if image[0x31ae*2:0x31ae*2+len(selector)] != selector:
        raise ValueError('native control selector sequence differs')
    setter = bytes.fromhex('7712 06d0 3292 e801 f482 1a82 8082')
    if image[0x31e7*2:0x31e7*2+len(setter)] != setter:
        raise ValueError('native control completion setter differs')
    request4 = [hex(address) for address, delayed in direct_callers(image, 0x318e)
                if delayed and image[(address+2)*2:(address+4)*2] == bytes.fromhex('7712 0004')]
    if data is not None:
        if hashlib.sha1(data).hexdigest() != '024c7f970f4ef754d3e90471de48a167515f930d':
            raise ValueError('native completion contract requires acquired NSE-1 ROM4 data')
        if int.from_bytes(data[0xb0b7*2:0xb0b7*2+2], 'big') != 0x4a3c:
            raise ValueError('native index-four command table differs')
    return {'publisher_call':'0x314d','publisher':'0x37ce','packet_type':'0x8a',
            'delay_slot':'0x314f sets AR4 to 0x008a',
            'payload':'two bytes at data 0x1200, copied from data 0x1867',
            'gate':'0x313f tests data 0x1949 bit 0x0080; set branches to 0x3153 instead',
            'preceding_control':'0x3078 calls 0x31ae with AR2=4; BITT uses T=15-AR2 against data 0x06d1, selecting physical bit 4',
            'completion_setter':'0x31e7..0x31ed ORs 1 shifted by pending index data 0x06d0 into data 0x06d1',
            'request_helper':'0x318e',
            'index4_delayed_call_candidates':request4,
            'index4_request_selection':{'host_type':'0x70','type_handler':'0x4951',
                                        'subcommand':'0x11','table_base':'0xb0a6',
                                        'table_entry':'0xb0b7','routine':'0x4a3c',
                                        'scope':'data-ROM table entry must be checked against acquired companion data image'},
            'companion_data_checked':data is not None,
            'rssi_publisher':{'entry':'0x47f6','type_load':'0x47ff','packet_type':'0x8b',
                              'publisher_call':'0x4810','publisher':'0x37ce','payload_bytes':162,
                              'scope':'resident publisher; uploaded caller addresses require live program evidence'},
            'scope':'resident ROM4 static path; neither execution nor fitted DSP mask is established by this contract'}


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
              0x2128d8:0x1389,0x2125c8:0x212b28,
              0x26e898:0x0447,0x26e880:0x07e5,0x21147c:0x108a00,
              0x23a8f0:0x05eb,0x23a8f4:0x05e6,0x23ebbc:0x0813,
              0x24e3b8:0x109f00,0x24e094:0x03e9,
              0x20bfb0:0x09ef,0x20bd7c:0x09ec,0x20bd80:0x0a02,
              0x20bd84:0x0a03,0x20bd8c:0x07d5}
    checks.update({0x23af00:0x10858c,0x23befc:0x05e6,0x23bf00:0x05ed,
                   0x23c394:0x05e6,0x23c398:0x05ed,0x23c7bc:0x05e6,0x23c7c0:0x05ed,
                   0x23aef0:0x040b,0x26a038:0x040b,0x26a4f0:0x040b,
                   0x2ac8e0:0x040c0000,0x2ac2f0:0x040d0000,0x2ac2f4:0x040d0300,
                   0x26e410:0x05dd,0x24dac4:0x07ea,0x24dd48:0x05dd,
                   0x26e8a8:0x07ea,0x26e79c:0x109f24})
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
                 0x2212ce:'200fe7f9',0x23a736:'782870a08026',
                 0x23bb6e:'21ff313b5c082800d10148e0',
                 0x23bffe:'21ff313b5c082800d10148e2',
                 0x23c59c:'20ff303b46595c402800d1014884',
                 0x26a15e:'200d4641f7ebf881'}
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
    dynamic_request_calls = thumb_bl_candidates(image, (0x26e25c,))[0x26e25c]
    completion_producers = [BASE+offset+10 for offset in range(0,len(image)-13,2)
                            if image[offset:offset+4] == bytes.fromhex('203f0140')
                            and BASE+offset+10 in completion_calls]
    task13_posts = thumb_bl_candidates(image, (0x255268,))[0x255268]
    task13_immediate = [site for site in task13_posts
                        if image[site-BASE-2:site-BASE] == bytes.fromhex('200d')
                        or image[site-BASE-4:site-BASE-2] == bytes.fromhex('200d')]
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
            'completion_recode':{'message_input':'0x05e6','helper_case':'0x26e56a',
                                  'condition':'message byte +2 is nonzero','returned_status':'0x07e5',
                                  'scope':'internal helper recode, not a raw DSP packet or a closed producer census'},
            'completion_producer_selectors':{'constructor':'0x23a728',
                                             'calls':['0x23bb80','0x23c010','0x23c5b0'],
                                             'selection':'owner byte +0x13a zero selects 0x05e6; nonzero selects 0x05ed',
                                             'message_byte2':'constructor copies owner byte +0',
                                             'receive_call':'0x23abca','receive_owner':'0x10858c',
                                             'scope':'three checked direct selector sites, not all indirect or computed producers'},
            'completion_server_ingress':{'rx_type':'0x80','dispatch':'0x294250',
                                         'decoder':'0x269f28','input':'0x040b',
                                         'delivery':'0x26a132','post':'0x26a162','destination_task':13,
                                         'server_dispatch':'0x23abdc',
                                         'scope':'conditional decoder delivery; RF payload semantics and all alternate sources unclosed'},
            'task13_direct_post_inventory':{'primitive':'0x255268','direct_call_candidates':len(task13_posts),
                                            'immediate_task13_candidates':[hex(site) for site in task13_immediate],
                                            'reset_template':'0x2ac8e0:040c0000',
                                            'other_templates':['0x2ac2f0:040d0000','0x2ac2f4:040d0300'],
                                            'dynamic_constructor':'0x26e25c',
                                            'dynamic_inputs':['0x05dd..0x05e3','0x060a','0x060b'],
                                            'dynamic_post':'0x26e3ee',
                                            'scope':'MOVS r0,#13 within final two halfwords only; excludes shared tails, register/data-driven and indirect callers'},
            'dynamic_request_predecessor':{'constructor':'0x26e25c',
                                           'direct_call_candidates':[hex(site) for site in dynamic_request_calls],
                                           'explicit_05dd_call':'0x24d9a8','branch_input':'0x07ea',
                                           'branch_selector':'0x24d70e','body':'0x24d99c',
                                           'body_gate':'owner byte +0x0a differs from 4',
                                           'virtual_input':'helper 0x26e424 mode 2 with queued object at 0x109f28 and no prior result returns 0x07ea',
                                           'scope':'bounded explicit selector and helper route; queued-work producer and alternate call arguments unclosed'},
            'nested_completion_decoder':{'inputs':['0x05ea','0x05eb'],
                                          'helper_case':'0x26e4e4','buffer_pointer_offset':8,
                                          'selector':'buffer byte +4 low nibble equals 6',
                                          'parser':'0x23daac','command24_case':'0x23e784',
                                          'command24_success_status':'0x0813',
                                          'constructor':'0x23a728','destination_task':16,
                                          'scope':'command 0x24 is not a demonstrated source of awaited 0x07e5; other parser commands and upstream buffer producers remain open'},
            'scan_finalizer_delivery':{'input':'0x042f','destination_task':15,
                                       'receiver_dispatch':'0x20931c',
                                       'selector_arithmetic':'0x0410 + 0x1f',
                                       'handler':'0x209480','effect':'copy message byte +2 to r7 +0x18',
                                       'return':'0x20948a returns unchanged input in r6',
                                       'mode1_continuation':['0x20bd02','0x20bc44','0x20bcac','0x20bcc8','0x20bb5a','0x20bb8e'],
                                       'mode1_effect':'0x20bb96 selects lifecycle state 0x19; 0x20bb9e sets owner +0x15 to 1',
                                       'scope':'static current-continuation handling, not observed finalizer delivery or closure of other lifecycle states'},
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
    parser.add_argument('--dsp-program',type=Path)
    parser.add_argument('--dsp-data',type=Path)
    args = parser.parse_args()
    try:
        result = analyze(args.image.read_bytes())
        if args.dsp_program:
            result['native_completion'] = native_completion_contract(
                args.dsp_program.read_bytes(), args.dsp_data.read_bytes() if args.dsp_data else None)
        elif args.dsp_data:
            raise ValueError('--dsp-data requires --dsp-program')
        print(json.dumps(result,indent=2))
    except (OSError,ValueError) as error:
        parser.exit(1,f'NSK-1 radio contract failed: {error}\n')


if __name__ == '__main__':
    main()

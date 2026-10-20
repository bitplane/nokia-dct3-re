"""Own NSK-3 state1 candidate; not an exhaustive producer/writer census."""
from tools.nsk3_static_check import inspect


def decode(image):
    fragments={0x2536bc:'48b54285d101f000fed8',0x2544bc:'48eae683',
               0x219136:'486b2701750746417541',0x21a674:'1c282104f063fcfd',
               0x21a6a4:'78a8f7fefd25',0x20af86:'20a3010198004288d101f003fd20',
               0x20e9da:'48d1e00a',0x208df8:'484e7fc170a17f0070e0e052',
               0x295c22:'207446692202f010fae5',0x21ac36:'203f802820ff30778420',
               0x21ac9a:'210102494288db0849e04288da05',
               0x21d670:'8c2120ff30774281da4c',
               0x2881e0:'48821a082800d100e77038ff38a02800d100e760',
               0x2880b6:'78a02800d100e1024ec5e100'}
    for address,encoded in fragments.items():
        expected=bytes.fromhex(encoded)
        if image[address-0x200000:address-0x200000+len(expected)]!=expected:
            raise ValueError('own state-transition instructions differ')
    def word(address):
        return int.from_bytes(image[address-0x200000:address-0x200000+4],'big')
    selector=word(((0x2536bc+4)&~3)+0x2d4)
    header=word(((0x2544bc+4)&~3)+0x3a8)
    owner=word(((0x219136+4)&~3)+0x1ac)
    if word(0x20ed20)!=0x834 or word(0x208f34)!=0x10a7d8:
        raise ValueError('upstream constructor header/context differs')
    if (selector,header,owner)!=(0x834,0x43f,0x106ce8):
        raise ValueError('own transition selector/header/owner differs')
    completion_input=word(0x2883ec)+0xff+0xa0
    if completion_input!=0x5e6 or word(0x2883d4)!=0x7e5:
        raise ValueError('helper completion namespace differs')
    return {'upstream_selector':0xa30,'upstream_context':0x10a7d8,
            'completion_input':completion_input,'nonzero_completion_result':0x7e5,
            'selector':selector,'header':header,'owner':owner,'new_state':1,
            'exhaustive_writer_census':False}


def inspect_source(source):
    _,image=inspect(source,'boot4')
    return decode(image)

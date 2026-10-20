"""Own NSK-3 state1 candidate; not an exhaustive producer/writer census."""
from tools.nsk3_static_check import inspect


def decode(image):
    fragments={0x2536bc:'48b54285d101f000fed8',0x2544bc:'48eae683',
               0x219136:'486b2701750746417541',0x21a674:'1c282104f063fcfd',
               0x21a6a4:'78a8f7fefd25'}
    for address,encoded in fragments.items():
        expected=bytes.fromhex(encoded)
        if image[address-0x200000:address-0x200000+len(expected)]!=expected:
            raise ValueError('own state-transition instructions differ')
    def word(address):
        return int.from_bytes(image[address-0x200000:address-0x200000+4],'big')
    selector=word(((0x2536bc+4)&~3)+0x2d4)
    header=word(((0x2544bc+4)&~3)+0x3a8)
    owner=word(((0x219136+4)&~3)+0x1ac)
    if (selector,header,owner)!=(0x834,0x43f,0x106ce8):
        raise ValueError('own transition selector/header/owner differs')
    return {'selector':selector,'header':header,'owner':owner,'new_state':1,
            'exhaustive_writer_census':False}


def inspect_source(source):
    _,image=inspect(source,'boot4')
    return decode(image)

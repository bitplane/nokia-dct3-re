"""Own NSK-3 retry-field contract, not donor credentials or factory settings."""
import struct


def inspect(image):
    def word(address):
        return int.from_bytes(image[address-0x200000:address-0x200000+4],'big')
    if image[0xa64bc:0xa64c8]!=bytes.fromhex('b510493d78092905db05202b'):
        raise ValueError('own security retry guard differs')
    if (word(0x2a65ac),word(0x2a65b4),word(0x2a65c0))!=(0x10c51c,0x10c520,0x70a):
        raise ValueError('own security context/retry ID differs')
    directory=word(0x2a39d8)
    if directory!=0x2c1aa0:raise ValueError('own record directory differs')
    pointer,limit,_=struct.unpack_from('>IHH',image,directory-0x200000+7*8)
    if pointer!=0x2c1798 or limit!=97:raise ValueError('own security group differs')
    for index,expected in ((1,(0x701,0x3b0,8)),(9,(0x709,0x3b0,4)),(10,(0x70a,0x3b4,1))):
        if struct.unpack_from('>IHH',image,pointer-0x200000+index*8)!=expected:
            raise ValueError('own security record extent differs')
    # Own reset helper clears context+4, then writes record70a through2a394e.
    if image[0xa63c4:0xa63d8]!=bytes.fromhex('b50048792100710121041808497b2200f7fdfabb'):
        raise ValueError('own retry initialization routine differs')
    return {'retry_eeprom':0x3b4,'retry_ram':0x10c520,'retry_record':0x70a,
            'code_eeprom':0x3b0,'rejected_count_at_least':5,
            'initialization':'own helper2a63c4 writes zero; not a factory-default claim'}

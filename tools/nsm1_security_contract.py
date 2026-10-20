"""Own NSM-1 phone retry-field source and initializer, not lock-policy guesses."""
from tools.nsm1_v523_static_check import literal,decode_group7


def inspect(image):
    spans={0x2bcd96:'493d78092905db05',0x2bcdd0:'2900d00720007120',
           0x29d6d4:'4ddc1c2849d22208',0x2dde40:'d13338d400000000'}
    for address,raw in spans.items():
        expected=bytes.fromhex(raw)
        if image[address-0x200000:address-0x200000+len(expected)]!=expected:
            raise ValueError(f'own security contract differs at {address:08x}')
    records={r['id']:r for r in decode_group7(image)}
    if (literal(image,0x2bcd96)!=0x112824 or literal(image,0x2bcdaa)!=0x112820
            or literal(image,0x2bcdc6)!=0x70a or literal(image,0x29d6d8)!=0x2dde40
            or records[0x70a]!={'id':0x70a,'offset':0x3d0,'size':1}):
        raise ValueError('retry source/default record differs')
    return {'eeprom_offset':0x3d0,'initial_value':0,'reject_at':5,
            'classification':'own-initializer external retry-state fixture'}

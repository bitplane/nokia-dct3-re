"""Decode the firmware's EEPROM descriptor, not a fitted-part identification."""


def eeprom_geometry(descriptor):
    if not isinstance(descriptor,int) or not 0 <= descriptor <= 0xff:
        raise ValueError('EEPROM descriptor must be one byte')
    page_shift = ((descriptor >> 3) & 7)-1
    if page_shift < 0:
        raise ValueError('EEPROM descriptor has an invalid page shift')
    return {'descriptor':hex(descriptor),'address_bytes':2 if descriptor & 0x40 else 1,
            'page_bytes':1 << page_shift,'capacity_bytes':1 << ((descriptor & 7)+9),
            'fitted_part_proved':False}

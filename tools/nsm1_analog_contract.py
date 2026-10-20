"""Own NSM-1 analog validation and shared report-tail entries, not physical units."""
FRAGMENTS={
    0x223786:'e140',0x2239c2:'e026',
    0x28a7b0:'b5701c0c1c052007f035fb8580284aab',
    0x28a7d8:'48a5434821e8f038fb7f',
    0x28a85e:'2000882922e100d24291db0b4ae64291dc08882122e100d24291db034ae24291dc002001',
    0x28a884:'b500b081466846691c89f7ffff8f2800',
    0x2e0b3c:'00040506070301020201',
}


def inspect(image):
    for address,raw in FRAGMENTS.items():
        expected=bytes.fromhex(raw)
        if image[address-0x200000:address-0x200000+len(expected)]!=expected:
            raise ValueError(f'own NSM-1 analog instructions differ at {address:08x}')
    return {'source7_channel':2,'scale':[1500,232],'inclusive_limits':[1800,5500],
            'counter':'0x11089a','physical_units_proved':False}

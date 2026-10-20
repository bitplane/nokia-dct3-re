"""Own boot4 analog startup validator; no electrical fixture selection."""
from pathlib import Path
import argparse
import json

FRAGMENTS={
    0x27846e:'4898434821e8f034fc3e04080c008028',
    0x2784f4:'2000882922e100d24291db0b4a754291dc08882122e100d24291db034a714291dc002001',
    0x27851a:'b500b081466846691c89f7ffff8f2800d00f498d7a482800d0011e407248',
    0x2786cc:'0010fe00000005dc',
    0x2786d8:'0000157c',
    0x2a9944:'002c7a54',
    0x2c7a54:'00040506070301020201',
}


def inspect(image):
    for address,encoded in FRAGMENTS.items():
        expected=bytes.fromhex(encoded)
        if image[address-0x200000:address-0x200000+len(expected)]!=expected:
            raise ValueError(f'own boot4 analog fragment differs at {address:08x}')
    return {'source7_channel':2,'source8':'cached sample with conditional differential correction',
            'scale_numerator':1500,'scale_denominator':232,
            'gain_address':'0x10fe34','offset_address':'0x10fe38',
            'signed_inclusive_limits':[1800,5500],
            'accepted_samples_before_report':10,'countdown_address':'0x10a689',
            'saturated_identity_conversion':1023*1500//232,
            'physical_units_proved':False,'nominal_input_selected':False}


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image',type=Path)
    args=parser.parse_args()
    print(json.dumps(inspect(args.image.read_bytes()),indent=2))

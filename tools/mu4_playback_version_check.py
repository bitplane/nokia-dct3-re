"""Compare only the pinned R060/R061 final-input policy, not whole playback."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.mu4_clock_contract import SOURCE_SHA256
from tools.mu4_context_contract import playback_final_input_words
from tools.noki5510_a00_inventory import uploaded_word_image

R061_SHA256='fad9a4d8c340c91fc06355ba18b42cdff9f2682ed48abb88903344d281b89d99'
TARGET=(0x4a11,0xeeff,0x8811,0x1005,0x1104,0x7681,9,0x76e1,2,2,0x81e1,3,
        0x70e1,9,0x3750,0x80e1,10,0x80f8,0x3751,0x76e1,1,0,0x4811,
        0xf982,0x8b0a,0xee01,0x8a11,0xf4e4)


def compare_images(old,new):
    policy=playback_final_input_words()
    if len(policy)!=235:
        raise ValueError('reviewed policy extent changed')
    for address,word in policy.items():
        expected=0x9019 if address==0x2958b else word
        if old.get(address)!=word or new.get(address)!=expected:
            raise ValueError(f'final-input policy differs at {address:06x}')
    if old.get(0x2958a)!=0xf983 or new.get(0x2958a)!=0xf983:
        raise ValueError('cleanup call does not target page 3')
    for image,base in ((old,0x39005),(new,0x39019)):
        if any(image.get(base+offset)!=word for offset,word in enumerate(TARGET)):
            raise ValueError('relocated cleanup target body differs')
    differences=sum(old.get(address)!=new.get(address) for address in old.keys()|new.keys())
    return dict(policy_words=235,identical_policy_words=234,
                relocated_call={'site':'02958a','R060':'039005','R061':'039019',
                                'identical_target_words':len(TARGET)},
                mapped_words={'R060':len(old),'R061':len(new)},
                raw_destination_differences=differences,
                scope='Pinned final-input policy and one relocated target only; not whole-image equivalence, '
                      'native R061 execution, intended sample extent or correct EOF/audio completion')


def compare_sources(old,new):
    if hashlib.sha256(old).hexdigest()!=SOURCE_SHA256 or hashlib.sha256(new).hexdigest()!=R061_SHA256:
        raise ValueError('expected original R060 and R061 sources')
    return compare_images(uploaded_word_image(bytes.fromhex(old.decode('ascii')),'aa99'),
                          uploaded_word_image(bytes.fromhex(new.decode('ascii')),'aa99'))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('r060',type=Path)
    parser.add_argument('r061',type=Path)
    args=parser.parse_args()
    try:
        result=compare_sources(args.r060.read_bytes(),args.r061.read_bytes())
    except (OSError,ValueError) as error:
        parser.exit(1,f'MU4 playback version policy: FAIL: {error}\n')
    print(json.dumps(result,indent=2))


if __name__=='__main__':
    main()

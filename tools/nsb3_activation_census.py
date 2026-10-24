"""Quantified direct-Thumb activation callers in the own NSB-3 MCU extent."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.message_census import decode_image, extract_calls
from tools.nsb3_nominal_input import IMAGE_SHA256
from tools.nse5_roller_contract import thumb_bl_candidates


def check_counts(text):
    rows = re.findall(r'nsb3_radio_activation_counts: controller=(\d+) initializer=(\d+) '
                      r'input13a3=(\d+) construct13a3=(\d+) observer_writes=0', text)
    if rows != [('0', '0', '0', '0')] or 'nsb3_radio_activation: site=' in text:
        raise ValueError('qualified cold-run activation absence differs')


def check_upstream(text):
    if 'nsb3_radio_upstream: header=03e90100 caller=00214931 count=1 observer_writes=0' not in text:
        raise ValueError('own task11 request was not consumed')
    if 'nsb3_radio_service_start: mode=00000002 caller=00213f41 count=2 observer_writes=0' not in text:
        raise ValueError('own asynchronous service start differs')
    posts = re.findall(r'nsb3_radio_service_post: object=([0-9a-f]{8}) header=0002441a '
                       r'body=00419800 caller=002128a1 count=3 observer_writes=0', text)
    if len(posts) != 1 or not 0x100000 <= int(posts[0], 16) < 0x110000:
        raise ValueError('own type1a service publication missing')
    if 'nsb3_radio_upstream_count: count=3 cap=64 observer_writes=0' not in text:
        raise ValueError('upstream observation missing or capture changed')


def callers(image, target):
    # The shared census consumes swap16 input; the NSB-3 collection is CPU-order.
    swapped = bytearray(image)
    swapped[::2], swapped[1::2] = image[1::2], image[::2]
    profile = {'apis': [{'address': hex(target), 'name': 'activation',
                        'kind': 'callback', 'arguments': {'input': 'r0'}}]}
    candidates = thumb_bl_candidates(image, (target,))[target]
    decoded = extract_calls(profile, decode_image(swapped, 0x200000), swapped, 0x200000)
    sites = {row['callsite'] for row in decoded}
    return {'direct_bl_candidates': len(candidates), 'decoded_calls': len(decoded),
            'arguments_recovered': sum(row['arguments']['input'] is not None for row in decoded),
            'undecoded_candidates': [hex(site) for site in candidates if site not in sites],
            'calls': [{'site': hex(row['callsite']), 'input': row['arguments']['input']} for row in decoded]}


def inspect(image):
    if hashlib.sha256(image).hexdigest() != IMAGE_SHA256:
        raise ValueError('requires own qualified NSB-3 image')
    mcu = image[:0xd6d00]
    constructor = callers(mcu, 0x2937c4)
    initializer = callers(mcu, 0x218bb0)
    publisher = callers(mcu, 0x211174)
    if (constructor['direct_bl_candidates'], constructor['decoded_calls'],
            constructor['arguments_recovered'], constructor['undecoded_candidates']) != (38, 38, 38, []):
        raise ValueError('controller constructor coverage differs')
    if any(row['input'] == 0x43f for row in constructor['calls']):
        raise ValueError('043f caller changes the qualified direct-call boundary')
    if [row['site'] for row in initializer['calls']] != ['0x218d50', '0x21a13e']:
        raise ValueError('own state-one initializer callers differ')
    return {'mcu_bytes_scanned': len(mcu), 'constructor': constructor,
            'state_one_initializer': initializer, 'status13a3_publisher': publisher,
            'scope': 'direct aligned Thumb BL plus local constant recovery; no indirect, tail-branch, data-driven or ARM caller closure'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('--run', type=Path, help='also check own native/result-cache controls and bounded activation counts')
    args = parser.parse_args()
    try:
        result = inspect(args.image.read_bytes())
        if args.run:
            from tools.nsb3_radio_check import check, check_consumer
            text = (args.run / 'error.log').read_text()
            check(text)
            check_consumer(text, (args.run / 'nsb3_radio_cached_vector.bin').read_bytes())
            check_counts(text)
            check_upstream(text)
            result['state_one_or_13a3_entrances_observed'] = False
            result['own_task11_request_to_type1a_observed'] = True
    except (OSError, ValueError) as exc:
        parser.exit(1, f'NSB-3 activation census: FAIL: {exc}\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()

"""Quantified direct-Thumb activation callers in the own NSB-3 MCU extent."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
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


def completion_routes(image):
    """Pin own selectors without assigning sibling protocol semantics."""
    for address, encoded in (
        (0x211ab8, '48481a082800d02e38022800d02338022800d01d'),
        (0x2112e0, '48d67800281fd11f4668f064fc3b2800d11af7ffff3f'),
        (0x213a1e, '464078801e402800d100e0c738012800d06738012800d100e08d38012800d100e0c5'),
        (0x213bf4, '465046495c402806d0012807d101f7fdfab7'),
    ):
        expected = bytes.fromhex(encoded)
        if image[address - 0x200000:address - 0x200000 + len(expected)] != expected:
            raise ValueError('own completion selector instructions differ')
    literals = {address: struct.unpack_from('>I', image, address - 0x200000)[0]
                for address in (0x211bdc, 0x21163c, 0x213ca4)}
    if literals != {0x211bdc: 0x3ed, 0x21163c: 0x108764, 0x213ca4: 0x3e9}:
        raise ValueError('own completion selector literals differ')
    return {'input03f1': {'dispatcher': '0x211ab0', 'handler': '0x2112dc',
                         'required_byte': '0x108764', 'required_value': 0x1f,
                         'additional_zero_predicate': '0x275b64'},
            'input03e9': {'dispatcher': '0x213950',
                         'selector3_publisher_call': '0x213b64',
                         'selector4_publisher_call': '0x213c02',
                         'selector4_prior_states': [6, 7]},
            'scope': 'own selector paths to13a3; neither input is proved to be a native RF completion'}


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
            'completion_routes': completion_routes(image),
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

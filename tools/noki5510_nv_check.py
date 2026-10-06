"""Assess recovered NPM-5 logical NV checks without provisioning records."""
import argparse
import json
from pathlib import Path
import struct


def assess(cache):
    if len(cache) != 0x3800:
        raise ValueError('expected complete 0x3800-byte logical NV cache')
    security_sum = sum(cache[:0x11c]) & 0xffff
    security_stored = struct.unpack_from('>I', cache, 0x11c)[0]
    config_sum = (sum(cache[0x120:0x256]) - sum(cache[0x154:0x156])) & 0xffff
    config_stored = struct.unpack_from('>H', cache, 0x256)[0]
    config_guard = struct.unpack_from('>H', cache, 0x170)[0]
    return {'security': {'sum': security_sum, 'stored': security_stored,
                         'valid': security_sum == security_stored},
            'configuration': {'sum': config_sum, 'stored': config_stored,
                              'guard': config_guard,
                              'valid': config_sum == config_stored and bool(config_sum | config_guard)},
            'scope': 'decoded cache validation, not physical storage or identity provenance'}


def verify_erased_frontier(text, cache):
    report = assess(cache)
    if cache[:0x258] != b'\xff' * 0x258:
        raise ValueError('critical NV regions are not erased')
    if report['security']['valid'] or report['configuration']['valid']:
        raise ValueError('not the documented erased-NV negative control')
    for event in ('pc=0024c7dc address=0013fbf2 data=00000012',
                  'pc=0024c858 address=0013fbec data=0000000c',
                  '5510_selftest_return: command=0d flags=88 records=ffff00ff/00ffff00/00ffff00/0cff0000/00001200'):
        if event not in text:
            raise ValueError('missing own NV-failure consumer: ' + event)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('cache', type=Path)
    parser.add_argument('--erased-frontier-log', type=Path)
    args = parser.parse_args()
    try:
        cache = args.cache.read_bytes()
        report = (verify_erased_frontier(args.erased_frontier_log.read_text(), cache)
                  if args.erased_frontier_log else assess(cache))
    except (OSError, ValueError) as error:
        parser.exit(1, f'5510 NV audit FAIL: {error}\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()

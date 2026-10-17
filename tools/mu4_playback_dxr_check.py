"""Bind actual channel-3 source reads to owned DXR writes, not transmission."""
import argparse
from array import array
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

if __package__:
    from .mu4_playback_capture_check import verify as verify_capture
else:
    from mu4_playback_capture_check import verify as verify_capture

FILE = 'mu4_playback_dxr.writes'
ROW = re.compile(
    r'mu4_playback_dxr_capture: records=(\d+) bytes=(\d+) fnv1a=([0-9a-f]{8}) '
    r'encoding=le64ns_u32ordinal_u16word_u8owner_u8reserved file=mu4_playback_dxr.writes '
    r'save_restore=unsupported observer_writes=0 audio_acceptance=0$')


def parse_row(text):
    lines = [line for line in text.splitlines() if 'mu4_playback_dxr_capture:' in line]
    if len(lines) != 1 or not (match := ROW.search(lines[0])):
        raise ValueError('missing, malformed or duplicate DXR ownership capture')
    return {'records':int(match[1]), 'bytes':int(match[2]), 'hash':match[3]}


def check_writes(row, data, source):
    if not source or not row['records'] or len(data) != row['bytes'] or len(data) != 16*row['records']:
        raise ValueError('DXR capture/source extent mismatch')
    fingerprint = 2166136261
    for byte in data:
        fingerprint = ((fingerprint ^ byte)*16777619) & 0xffffffff
    if f'{fingerprint:08x}' != row['hash']:
        raise ValueError('DXR capture hash mismatch')
    previous = None
    owners = Counter()
    consumed = 0
    for when, ordinal, word, owner, reserved in struct.iter_unpack('<QIHBB', data):
        if reserved or owner > 6 or (previous is not None and when < previous):
            raise ValueError('invalid DXR owner, reserved byte or timestamp order')
        owners['no_active_dma' if not owner else f'dma_channel_{owner-1}'] += 1
        if owner == 4:
            if ordinal != consumed+1 or consumed >= len(source):
                raise ValueError('channel-3 DXR write does not identify the next actual source read')
            if word != source[consumed]:
                raise ValueError('owned DXR write differs from its actual DMA source word')
            consumed += 1
        elif ordinal:
            raise ValueError('non-channel-3 DXR write carries a channel-3 source ordinal')
        previous = when
    if consumed != len(source):
        raise ValueError('actual channel-3 source reads lack owned DXR writes')
    return {'records':row['records'], 'sha256':hashlib.sha256(data).hexdigest(),
        'owner_counts':dict(owners), 'channel_3_source_reads_bound':consumed,
        'dma_to_dxr_validated':True, 'dxr_to_tx_validated':False,
        'native_audio_validated':False,
        'scope':'Actual DMA bus ownership and ordered source/write values only; DXR acceptance, overwrite, reset and transmission remain separate'}


def verify(directory):
    text = (directory/'error.log').read_text()
    capture = verify_capture(text, directory, require_framing=True, require_tx_times=True)
    source = array('H')
    source.frombytes((directory/'mu4_playback_dma.le16').read_bytes())
    if sys.byteorder != 'little':
        source.byteswap()
    result = check_writes(parse_row(text), (directory/FILE).read_bytes(), source)
    result['source_sha256'] = capture['streams']['dma_source']['sha256']
    result['serial_callbacks'] = capture['serial_callbacks']
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    try:
        result = verify(args.directory)
    except (OSError, ValueError) as error:
        print(f'MU4 DXR ownership: {error}', file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

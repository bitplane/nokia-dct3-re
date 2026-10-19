"""Append a declared existing ADTS packet; never classify the prefix stop as EOF."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

if __package__:
    from .mu4_recorder_media_inventory import adts_header
else:
    from mu4_recorder_media_inventory import adts_header


def append_lookahead(encoded):
    packets, cursor = [], 0
    while cursor < len(encoded):
        header = adts_header(encoded,cursor)
        if not header or not header['frame_within_file']:
            raise ValueError('requires a complete ADTS packet chain')
        length = header['frame_bytes']
        if length <= 7:
            raise ValueError('empty ADTS packet')
        packets.append((cursor,length))
        cursor += length
    if len(packets) < 2:
        raise ValueError('requires at least two packets')
    offset,length = packets[-2]
    if length < 16:
        raise ValueError('selected lookahead packet is shorter than the recovered reserve')
    result = encoded+encoded[offset:offset+length]
    return result,dict(source_bytes=len(encoded),source_sha256=hashlib.sha256(encoded).hexdigest(),
                       original_packets=len(packets),appended_packet=len(packets)-1,
                       appended_source_offset=offset,appended_bytes=length,
                       output_sha256=hashlib.sha256(result).hexdigest(),
                       scope='Original packet prefix plus explicit existing-packet lookahead; not original file EOF or handset playback')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path)
    parser.add_argument('output',type=Path)
    args = parser.parse_args()
    try:
        payload,report = append_lookahead(args.source.read_bytes())
        with args.output.open('xb') as output:
            output.write(payload)
    except (OSError,ValueError) as error:
        print(f'AAC lookahead fixture: {error}',file=sys.stderr)
        return 1
    print(json.dumps(report,indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

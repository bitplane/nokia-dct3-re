"""Check one native AAC ABI capture; waveform errors remain diagnostics."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import sys

if __package__:
    from .mu4_context_contract import contract, reference_pcm_conversion
else:
    from mu4_context_contract import contract, reference_pcm_conversion


def rows(text, marker):
    result = []
    for line in text.splitlines():
        if marker+':' not in line:
            continue
        fields = {}
        for token in line.split(marker+':',1)[1].split():
            key, separator, value = token.partition('=')
            if not separator or key in fields:
                raise ValueError(f'malformed {marker}')
            fields[key] = value
        result.append(fields)
    return result


def check_capture(text, raw, converted, reference):
    setup = rows(text, 'mu4_aac_abi_setup')
    headers = rows(text, 'mu4_aac_abi_header')
    sources = rows(text, 'mu4_aac_abi_sources')
    results = rows(text, 'mu4_aac_abi_result')
    captures = rows(text, 'mu4_aac_abi_capture')
    if any(len(group) != 1 for group in (setup, headers, sources, results, captures)):
        raise ValueError('requires one setup, source, result and completed capture')
    if setup[0] != dict(original_upload_words='49843',original_initializers='228',input_bytes='2048',
                        constructor='023d27',process='023d5d',isolated_abi='1',handset_boot='0',conformance='0'):
        raise ValueError('ABI setup scope changed')
    if headers[0] != dict(ordinal='1',sync='0fff',id='0001',layer='0000',protection='0001',
                          profile='0001',frequency='0004',channels='0002',frame_length='0134',
                          blocks='0001',identity='Execute_entry',observer_writes='0'):
        raise ValueError('generated MPEG-2 first-header identity changed')
    markers = ['mu4_aac_abi_'+name+':' for name in ('setup','header','sources','result','capture')]
    if [text.index(marker) for marker in markers] != sorted(text.index(marker) for marker in markers):
        raise ValueError('capture event order changed')
    if sources[0] != dict(left='f000',right='e800',sp='8fe8',result='0000',identity='Execute_entry',observer_writes='0'):
        raise ValueError('unverified decoder-return source binding')
    if results[0] != dict(returned='1',result='0000',base='162e',index='009b',remaining='0010',
                          illegal='0',pc='001018',isolated_abi='1',conformance='0'):
        raise ValueError('failed, incomplete or changed first-frame result')
    if captures[0] != dict(frames='1024',channels='2',raw_bytes='8192',converted_bytes='4096',
                           source='decoder_return_pointers',memory='post_wrapper_return',conformance='0'):
        raise ValueError('capture scope changed')
    if len(raw) != 8192 or len(converted) != 4096 or len(reference) < 4096 or len(reference) % 4:
        raise ValueError('incomplete raw, converted or stereo reference interval')
    if 'Fatal error:' in text or 'unsupported instruction' in text:
        raise ValueError('faulted native observation')
    values = struct.unpack('<2048i', raw)
    actual = struct.unpack('<2048h', converted)
    wanted = struct.unpack('<2048h', reference[:4096])
    if any(reference_pcm_conversion(value & 0xffffffff)['word'] != (word & 0xffff)
           for value, word in zip(values, actual)):
        raise ValueError('post-return raw buffers disagree with native converted memory')
    channels = []
    for channel in range(2):
        errors = [actual[i]-wanted[i] for i in range(channel,2048,2)]
        channels.append(dict(channel=channel, mismatches=sum(error != 0 for error in errors),
                             maximum=max(map(abs,errors)),
                             rms=math.sqrt(math.fsum(error*error for error in errors)/1024)))
    return dict(frames=1024, channels=channels, conversion_words=2048,
                raw_sha256=hashlib.sha256(raw).hexdigest(),
                converted_sha256=hashlib.sha256(converted).hexdigest(),
                reference_prefix_sha256=hashlib.sha256(reference[:4096]).hexdigest(),
                scope='One post-wrapper-return memory capture and exact conversion; host s16 differences are diagnostics, not codec conformance or per-store/input-read ownership')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('--source-a00', required=True, type=Path)
    parser.add_argument('--reference-pcm', required=True, type=Path)
    args = parser.parse_args()
    try:
        contract(args.source_a00.read_bytes())
        result = check_capture((args.run/'error.log').read_text(),
                               (args.run/'mu4_aac_abi_raw.s32le').read_bytes(),
                               (args.run/'mu4_aac_abi_converted.s16le').read_bytes(),
                               args.reference_pcm.read_bytes())
    except (OSError, ValueError) as error:
        print(f'MU4 AAC ABI: {error}', file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

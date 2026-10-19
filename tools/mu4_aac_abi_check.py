"""Check bounded native AAC ABI captures; waveform errors remain diagnostics."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import sys

if __package__:
    from .mu4_aac_lookahead_fixture import append_lookahead
    from .mu4_context_contract import contract, reference_pcm_conversion
    from .mu4_recorder_media_inventory import adts_header
else:
    from mu4_aac_lookahead_fixture import append_lookahead
    from mu4_context_contract import contract, reference_pcm_conversion
    from mu4_recorder_media_inventory import adts_header


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


def check_input(text, encoded, session=False):
    reads, ends = rows(text,'mu4_aac_abi_input_read'), rows(text,'mu4_aac_abi_input_end')
    if not 0 < len(reads) <= 4096 or len(ends) != 1 or len(encoded) < 2048:
        raise ValueError('missing, capped or incomplete input observation')
    header = adts_header(encoded,0)
    if not header or header['frame_bytes'] != 308 or not header['frame_within_file']:
        raise ValueError('input does not contain the declared first ADTS frame')
    seen = set()
    try:
        for ordinal, read in enumerate(reads,1):
            index = int(read['index'])
            if (int(read['ordinal']) != ordinal or not 0 <= index < 1024
                    or int(read['address'],16) != 0x162e+index
                    or int(read['value'],16) != int.from_bytes(encoded[2*index:2*index+2],'big')
                    or read['mask'] != 'ffff' or read['observer_writes'] != '0'
                    or read['context_pc'] not in ('023d8b','025129')):
                raise ValueError('input read identity, byte value or context changed')
            seen.add(index)
        if ends[0] != dict(reads=str(len(reads)),unique_words=str(len(seen)),highest_index=str(max(seen)),
                           complete='1',observer_writes='0'):
            raise ValueError('input terminal does not account for the reads')
    except (KeyError,TypeError) as error:
        raise ValueError('incomplete input-read fields') from error
    if seen != set(range(max(seen)+1)) or not set(range(155)).issubset(seen):
        raise ValueError('input coverage omits consumed or retained reservoir bytes')
    boundary = text.rfind('mu4_aac_abi_sources:') if session else text.index('mu4_aac_abi_sources:')
    if text.rfind('mu4_aac_abi_input_read:') > boundary:
        raise ValueError('input observation extends past decoder return')
    return dict(reads=len(reads),unique_words=len(seen),consumed_frame_bytes=308,
                observed_prefix_bytes=2*len(seen),lookahead_bytes=2*len(seen)-308,
                consumed_sha256=hashlib.sha256(encoded[:308]).hexdigest(),
                scope='Actual input-window reads and contiguous consumed-prefix coverage; context PCs are not retained instruction ownership or an all-address exclusion proof')


def check_session(text, raw, converted, reference, encoded, *, refill=False, stream=False, eof_boundary=False, lookahead=False):
    """Original calls sharing a constructor/reservoir; optional public-input refill."""
    try:
        if lookahead and append_lookahead(encoded[:65339])[0] != encoded:
            raise ValueError('lookahead is not the declared original-packet suffix')
        result = _check_session(text,raw,converted,reference,encoded,refill=refill or stream or eof_boundary or lookahead,
                               calls=174 if lookahead else 172 if eof_boundary else 32 if stream else 8 if refill else 4,boundary=eof_boundary)
        if lookahead:
            result['status'] = 'original_174_packet_prefix_with_declared_lookahead_not_eof'
        return result
    except (KeyError,TypeError) as error:
        raise ValueError('incomplete session fields') from error


def _check_session(text, raw, converted, reference, encoded, *, refill, calls, boundary=False):
    returns = rows(text, 'mu4_aac_session_return')
    headers = rows(text, 'mu4_aac_abi_header')
    sources = rows(text, 'mu4_aac_abi_sources')
    captures = rows(text, 'mu4_aac_abi_capture')
    if any(len(group) != calls for group in (returns,headers,sources,captures)):
        raise ValueError('requires complete native calls and captures')
    setup = rows(text,'mu4_aac_abi_setup')
    if setup != [dict(original_upload_words='49843',original_initializers='228',input_bytes='2048',
                      output_base='7000',output_words='2048',constructor='023d27',process='023d5d',
                      isolated_abi='1',handset_boot='0',conformance='0')]:
        raise ValueError('session setup changed')
    refills = rows(text,'mu4_aac_session_refill')
    if len(refills) != ((calls-1+int(boundary))//4 if refill else 0):
        raise ValueError('unexpected or missing public-input refill')
    markers = ['mu4_aac_abi_'+name for name in ('setup','header','sources','capture','result','input_end')]
    markers += ['mu4_aac_session_return','mu4_aac_session_refill','mu4_aac_wrapper_early_return']
    observed = [marker for line in text.splitlines() for marker in markers if marker+':' in line]
    expected_order = ['mu4_aac_abi_setup']
    for call in range(1,calls+1):
        expected_order += ['mu4_aac_abi_header','mu4_aac_abi_sources','mu4_aac_abi_capture','mu4_aac_session_return']
        if refill and call % 4 == 0 and (call < calls or boundary):
            expected_order.append('mu4_aac_session_refill')
    expected_order += ['mu4_aac_wrapper_early_return'] if boundary else ['mu4_aac_abi_result','mu4_aac_abi_input_end']
    if observed != expected_order:
        raise ValueError('session event order changed')
    base, consumed, window_offset = 0x162e, 0, 0
    for call, (result, header, source) in enumerate(zip(returns, headers, sources), 1):
        packet = adts_header(encoded, consumed)
        if not packet or not packet['frame_within_file']:
            raise ValueError('unsupported session packet extent')
        length = packet['frame_bytes']
        remaining = int(result['remaining'],16)
        if remaining not in (0,8,16,24,32) or (consumed+length+remaining//8-window_offset) % 2:
            raise ValueError('invalid retained reservoir count')
        cursor = 0x162e + (consumed+length+remaining//8-window_offset)//2
        index = cursor-base
        expected = dict(call=str(call),descriptor=f'{base+index:04x}',base=f'{base:04x}',
                        index=f'{index:04x}',remaining=f'{remaining:04x}',result='0000',observer_writes='0')
        expected_header = dict(ordinal=str(call),sync='0fff',id='0001',layer='0000',protection='0001',
                               profile='0001',frequency='0004',channels='0002',frame_length=f'{length:04x}',
                               blocks='0001',identity='Execute_entry',observer_writes='0')
        if result != expected or header != expected_header or not 0x162e <= base <= cursor <= 0x1a2e:
            raise ValueError('native descriptor/reservoir continuity changed')
        if source != dict(left='f000',right='e800',sp='8fe8',result='0000',identity='Execute_entry',observer_writes='0'):
            raise ValueError('session output source changed')
        base += index
        consumed += length
        if captures[call-1] != dict(frames='1024',channels='2',raw_bytes='8192',converted_bytes='4096',
                                    source='decoder_return_pointers',memory='post_wrapper_return',conformance='0'):
            raise ValueError('session capture scope changed')
        if refill and call % 4 == 0 and (call < calls or boundary):
            window_offset += 2*(cursor-0x162e)
            window_bytes = min(2048,len(encoded)-window_offset) & ~1
            if refills[call//4-1] != dict(after_call=str(call),source_byte=str(window_offset),old_cursor=f'{cursor:04x}',
                                  cursor='162e',end=f'{0x162e+window_bytes//2:04x}',bytes=str(window_bytes),remaining=f'{remaining:04x}',
                                  reservoir_high=encoded[window_offset-4:window_offset-2].hex(),
                                  reservoir_low=encoded[window_offset-2:window_offset].hex(),fixture_input='1',decoder_state_writes='0'):
                raise ValueError('refill changed private decoder state or source offset')
            base = 0x162e
    terminal = rows(text, 'mu4_aac_abi_result')
    if boundary:
        reserve = rows(text,'mu4_aac_tail_reserve')
        expected_reserve = [dict(opcode='f6aa',candidate_index=index,reserve_limit='00dc',extent='00e4',
                                 remaining=remaining,identity='Execute_entry',observer_writes='0')
                            for index,remaining in [('0000','0008'),('00dd','0000')]]
        if reserve != expected_reserve:
            raise ValueError('missing eight-word tail-reserve boundary')
        if terminal or rows(text,'mu4_aac_input_exhausted') or rows(text,'mu4_aac_wrapper_early_return') != [dict(
                completed_calls=str(calls),headers=str(calls),sources=str(calls),result='ffff',cursor='162e',
                end=f'{0x162e+window_bytes//2:04x}',base='162e',index='0000',remaining='0000',
                input_source=str(window_offset),window_bytes=str(window_bytes),illegal='0',observer_writes='0')]:
            raise ValueError('short-input boundary changed or read beyond source')
    elif terminal != [dict(returned='1',result='0000',base=result['base'],index=result['index'],
                         remaining=result['remaining'],illegal='0',pc='001023' if calls > 8 else '001076' if refill else '001042',
                         isolated_abi='1',conformance='0')]:
        raise ValueError('missing successful session terminal')
    fatal_messages = [line.split('Fatal error:',1)[1].strip() for line in text.splitlines() if 'Fatal error:' in line]
    if (any(not boundary or message != 'MU4 AAC wrapper returned without a new decoded block' for message in fatal_messages)
            or 'unsupported instruction' in text):
        raise ValueError('faulted session')
    words = calls*2048
    if len(raw) != words*4 or len(converted) != words*2 or len(reference) < words*2:
        raise ValueError('incomplete session PCM')
    values = struct.unpack(f'<{words}i', raw)
    actual = struct.unpack(f'<{words}h', converted)
    wanted = struct.unpack(f'<{words}h', reference[:words*2])
    if any(reference_pcm_conversion(x & 0xffffffff)['word'] != (y & 0xffff) for x,y in zip(values,actual)):
        raise ValueError('session conversion mismatch')
    if refill:
        check_refill_input(text,encoded,consumed,calls,boundary=boundary)
    else:
        check_input(text,encoded,session=True)
    return dict(calls=calls,frames=calls*1024,consumed_bytes=consumed,
                differing_samples=sum(x != y for x,y in zip(actual,wanted)),
                max_error=max(abs(x-y) for x,y in zip(actual,wanted)),conformance=False,
                status='native_short_input_boundary_not_eof_completion' if boundary else 'bounded_continuity')


def check_refill_input(text, encoded, consumed, calls=8, *, boundary=False):
    reads = rows(text,'mu4_aac_session_input_read')
    if not 0 < len(reads) <= max(4096,calls*1024) or rows(text,'mu4_aac_abi_input_read'):
        raise ValueError('missing, mixed or capped session input')
    source_words, indices = set(), set()
    window_offset, ordinal, window_bytes = 0, 0, 2048
    for line in text.splitlines():
        if 'mu4_aac_session_refill:' in line:
            record = rows(line,'mu4_aac_session_refill')[0]
            window_offset,window_bytes = int(record['source_byte']),int(record['bytes'])
        entries = rows(line,'mu4_aac_session_input_read')
        if not entries:
            continue
        read = entries[0]
        ordinal += 1
        index = int(read['index'])
        offset = window_offset+2*index
        if not 0 <= index < window_bytes//2 or offset+2 > len(encoded):
            raise ValueError('session input outside window/source')
        if read != dict(ordinal=str(ordinal),index=str(index),source_byte=str(offset),
                        address=f'{0x162e+index:04x}',value=f'{int.from_bytes(encoded[offset:offset+2],"big"):04x}',
                        mask='ffff',context_pc=read['context_pc'],observer_writes='0') or read['context_pc'] not in ('023d8b','025129'):
            raise ValueError('session input byte, offset or context changed')
        source_words.add(offset//2)
        indices.add(index)
    if source_words != set(range(max(source_words)+1)) or 2*len(source_words) < consumed:
        raise ValueError('session input omits consumed prefix')
    expected_end = [] if boundary else [dict(reads=str(len(reads)),unique_words=str(len(indices)),
                                           highest_index=str(max(indices)),complete='1',observer_writes='0')]
    if rows(text,'mu4_aac_abi_input_end') != expected_end:
        raise ValueError('session input summary changed')
    final_marker = 'mu4_aac_wrapper_early_return:' if boundary else 'mu4_aac_abi_sources:'
    if text.rfind('mu4_aac_session_input_read:') > text.rfind(final_marker):
        raise ValueError('session input after final decoder return')


def check_capture(text, raw, converted, reference):
    setup = rows(text, 'mu4_aac_abi_setup')
    headers = rows(text, 'mu4_aac_abi_header')
    sources = rows(text, 'mu4_aac_abi_sources')
    results = rows(text, 'mu4_aac_abi_result')
    captures = rows(text, 'mu4_aac_abi_capture')
    if any(len(group) != 1 for group in (setup, headers, sources, results, captures)):
        raise ValueError('requires one setup, source, result and completed capture')
    if setup[0] != dict(original_upload_words='49843',original_initializers='228',input_bytes='2048',
                        output_base='7000',output_words='2048',
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
    parser.add_argument('--input-aac', required=True, type=Path)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--four-calls', action='store_true')
    mode.add_argument('--refill', action='store_true', help='check eight calls with one public-input refill')
    mode.add_argument('--stream', action='store_true', help='check 32 calls with repeated public-input refills')
    mode.add_argument('--eof-boundary', action='store_true', help='check 172 successful calls followed by native short-input return, not full EOF success')
    mode.add_argument('--lookahead', action='store_true', help='check the original 174-packet prefix with a declared existing-packet suffix, not EOF')
    args = parser.parse_args()
    try:
        contract(args.source_a00.read_bytes())
        encoded = args.input_aac.read_bytes()
        capture_args = ((args.run/'error.log').read_text(),
                               (args.run/'mu4_aac_abi_raw.s32le').read_bytes(),
                               (args.run/'mu4_aac_abi_converted.s16le').read_bytes(),
                               args.reference_pcm.read_bytes())
        session = args.four_calls or args.refill or args.stream or args.eof_boundary or args.lookahead
        result = check_session(*capture_args,encoded,refill=args.refill,stream=args.stream,eof_boundary=args.eof_boundary,lookahead=args.lookahead) if session else check_capture(*capture_args)
        original = encoded[:65339] if args.lookahead else encoded
        if hashlib.sha256(original).hexdigest() != '6b55159252fa5bc00dadfc76bab243ee1d3993314d7c2c704ed98d73469d001c':
            raise ValueError('expected the independently generated MPEG-2 control')
        if not session:
            result['input_binding'] = check_input((args.run/'error.log').read_text(),encoded)
    except (OSError, ValueError) as error:
        print(f'MU4 AAC ABI: {error}', file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

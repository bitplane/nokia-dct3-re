"""Qualify original ISO AL09 native packet acceptance, not waveform conformance."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
import sys

from mu4_aac_abi_check import check_refill_input, rows
from mu4_context_contract import (SOURCE_SHA256, decode_mono_output, decode_pcm_conversion,
                                  reference_pcm_conversion, decode_decoder_input_reserve)
from mu4_recorder_media_inventory import adts_header
from noki5510_a00_inventory import uploaded_word_image


INPUT_SHA = '3117cfade03058ffceaf8953727bf84e03ba9c98652cb14bd7523bda4dcb72a6'
REFERENCE_SHA = 'a30567dea5f0869a8a1fa9ac04c3f97876d39671ae61acf11bf7ba41b69f048a'
STOP = 'MU4 AAC wrapper returned without a new decoded block'


def compare_host_pcm(raw, host, expected_host_samples):
    """Fixed zero-offset diagnostic; never fit timing/gain or award conformance."""
    if not raw or len(raw)%8 or len(host)!=expected_host_samples*4:
        raise ValueError('native or host PCM extent differs')
    native=struct.unpack(f'<{len(raw)//4}i',raw)[::2]
    values=struct.unpack(f'<{len(host)//4}f',host)
    if len(values)<len(native) or not all(math.isfinite(value) for value in values):
        raise ValueError('short or nonfinite host PCM')
    # Original argument-14 conversion maps raw/2^14 to s16 sample units.
    errors=[sample/16384-values[index]*32768 for index,sample in enumerate(native)]
    return dict(native_samples=len(native),host_samples=len(values),
                compared_samples=len(errors),uncompared_host_tail=len(values)-len(native),
                offset_samples=0,fitted_gain=False,fitted_offset=False,
                raw_to_s16_divisor=16384,
                rms_s16=math.sqrt(math.fsum(value*value for value in errors)/len(errors)),
                maximum_s16=max(map(abs,errors)),
                native_first_nonzero=next((i for i,value in enumerate(native) if value),None),
                host_first_nonzero=next((i for i,value in enumerate(values) if value),None),
                host_pcm_sha256=hashlib.sha256(host).hexdigest(),
                conformance=False,iso_reference_alignment=False,handset_playback=False)


def original_assets(encoded, reference, source):
    for data, digest in ((encoded, INPUT_SHA), (reference, REFERENCE_SHA),
                         (source, SOURCE_SHA256)):
        if hashlib.sha256(data).hexdigest() != digest:
            raise ValueError('requires unchanged original ISO inputs and R060 upload')
    words = uploaded_word_image(bytes.fromhex(source.decode('ascii')), 'aa99')
    decode_pcm_conversion(words)
    decode_decoder_input_reserve(words)
    return decode_mono_output(words)


def check_tail_reserve(text):
    """Pin the observed AL09 rejection, not a general EOF or padding policy."""
    expected = [dict(opcode='f6aa', candidate_index=candidate, reserve_limit='0040',
                     extent='0048', remaining=remaining, identity='Execute_entry',
                     observer_writes='0')
                for candidate, remaining in (('0000', '0008'), ('0048', '0000'))]
    if rows(text, 'mu4_aac_tail_reserve') != expected:
        raise ValueError('original AL09 tail reserve observations changed')
    markers = ('mu4_aac_session_return: call=342 ', 'mu4_aac_tail_reserve:',
               'mu4_aac_wrapper_early_return:')
    order = [marker for line in text.splitlines() for marker in markers if marker in line]
    if order != [markers[0], markers[1], markers[1], markers[2]]:
        raise ValueError('original AL09 tail reserve order changed')
    return dict(extent_words=72, reserve_words=8, rejected_candidate=72,
                reserve_limit=64, source_packet_bytes=147,
                complete_stream=False, eof_policy_established=False)


def check_stream(text, raw, converted, encoded, calls=342):
    setup = rows(text, 'mu4_aac_abi_setup')
    if setup != [dict(original_upload_words='49843', original_initializers='228',
                      input_bytes='2048', output_base='7000', output_words='2048',
                      constructor='023d27', process='023d5d', isolated_abi='1',
                      handset_boot='0', conformance='0')]:
        raise ValueError('original constructor/caller setup changed')
    headers = rows(text, 'mu4_aac_abi_header')
    sources = rows(text, 'mu4_aac_abi_sources')
    captures = rows(text, 'mu4_aac_abi_capture')
    returns = rows(text, 'mu4_aac_session_return')
    refills = rows(text, 'mu4_aac_session_refill')
    if any(len(group) != calls for group in (headers, sources, captures, returns)):
        raise ValueError('incomplete original-vector packet/capture sequence')
    markers = ('mu4_aac_abi_setup', 'mu4_aac_abi_header', 'mu4_aac_abi_sources',
               'mu4_aac_abi_capture', 'mu4_aac_session_return',
               'mu4_aac_session_refill', 'mu4_aac_wrapper_early_return')
    order = [marker for line in text.splitlines() for marker in markers if marker+':' in line]
    expected_order = [markers[0]]
    base, consumed, window, refill_index = 0x162e, 0, 0, 0
    for call, (header, source, capture, result) in enumerate(zip(headers, sources, captures, returns), 1):
        packet = adts_header(encoded, consumed)
        if (not packet or not packet['frame_within_file'] or
                (packet['mpeg_version'], packet['profile_bits'], packet['sampling_index'],
                 packet['channel_configuration'], packet['raw_data_blocks_minus_one']) != (2, 1, 4, 1, 0)):
            raise ValueError('requires complete MPEG-2 LC mono 44.1-kHz packets')
        consumed += packet['frame_bytes']
        remaining = int(result['remaining'], 16)
        if remaining not in (0, 8, 16, 24, 32) or (consumed + remaining//8 - window) % 2:
            raise ValueError('invalid byte-aligned native reservoir')
        cursor = 0x162e + (consumed + remaining//8 - window)//2
        if result != dict(call=str(call), descriptor=f'{cursor:04x}', base=f'{base:04x}',
                          index=f'{cursor-base:04x}', remaining=f'{remaining:04x}',
                          result='0000', observer_writes='0') or not base <= cursor <= 0x1a2e:
            raise ValueError('native input cursor/reservoir accounting changed')
        if header != dict(ordinal=str(call), sync='0fff', id='0001', layer='0000',
                          protection=f'{int(packet["header_bytes"] == 7):04x}', profile='0001',
                          frequency='0004', channels='0001', frame_length=f'{packet["frame_bytes"]:04x}',
                          blocks='0001', identity='Execute_entry', observer_writes='0'):
            raise ValueError('native header differs from original packet')
        if source != dict(left='e800', right='f000', sp='8fe8', result='0000',
                          identity='Execute_entry', observer_writes='0'):
            raise ValueError('mono return-pointer observation changed')
        if capture != dict(frames='1024', channels='2', raw_bytes='8192', converted_bytes='4096',
                           source='decoder_return_pointers', memory='post_wrapper_return', conformance='0'):
            raise ValueError('capture extent or ownership changed')
        expected_order += list(markers[1:5])
        base = cursor
        if call % 4 == 0:
            if refill_index >= len(refills):
                raise ValueError('missing caller-owned refill')
            window += 2*(cursor-0x162e)
            available = min(2048, len(encoded)-window) & ~1
            if refills[refill_index] != dict(after_call=str(call), source_byte=str(window),
                    old_cursor=f'{cursor:04x}', cursor='162e', end=f'{0x162e+available//2:04x}',
                    bytes=str(available), remaining=f'{remaining:04x}',
                    reservoir_high=encoded[window-4:window-2].hex(),
                    reservoir_low=encoded[window-2:window].hex(), fixture_input='1', decoder_state_writes='0'):
                raise ValueError('refill changed reservoir or original source position')
            refill_index += 1
            base = 0x162e
            expected_order.append(markers[5])
    expected_order.append(markers[6])
    if order != expected_order or refill_index != len(refills):
        raise ValueError('packet/refill/terminal order changed')
    available = min(2048, len(encoded)-window) & ~1
    if (rows(text, 'mu4_aac_wrapper_early_return') != [dict(completed_calls=str(calls),
            headers=str(calls), sources=str(calls), result='ffff', cursor=f'{cursor:04x}',
            end=f'{0x162e+available//2:04x}', base=f'{cursor:04x}', index='0000', remaining='0000',
            input_source=str(window), window_bytes=str(available), illegal='0', observer_writes='0')]
            or rows(text, 'mu4_aac_abi_result') or rows(text, 'mu4_aac_input_exhausted')
            or 'unsupported instruction' in text):
        raise ValueError('strict short-input boundary changed or faulted')
    check_refill_input(text, encoded, consumed, calls, boundary=True)
    if len(raw) != calls*8192 or len(converted) != calls*4096:
        raise ValueError('missing or truncated native output')
    raw_values = struct.unpack(f'<{calls*2048}i', raw)
    output = struct.unpack(f'<{calls*2048}h', converted)
    # The original mono branch aliases its second source after the observer.
    # Odd raw slots are consequently not an independently decoded channel.
    for sample in range(calls*1024):
        word = reference_pcm_conversion(raw_values[2*sample] & 0xffffffff)['word']
        if any((output[2*sample+channel] & 0xffff) != word for channel in range(2)):
            raise ValueError('native mono conversion/duplication differs from source contract')
    result = dict(calls=calls, consumed_bytes=consumed, mono_samples=calls*1024,
                input_reads=len(rows(text, 'mu4_aac_session_input_read')),
                conformance=False, crc_validation=False, waveform_alignment=False,
                complete_stream=False, handset_playback=False,
                scope='Original packet acceptance and mono conversion only; final packet remains undecoded')
    if calls == 342:
        result['tail_reserve'] = check_tail_reserve(text)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--source-a00', type=Path, required=True)
    parser.add_argument('--ffmpeg',help='optional independent zero-offset PCM diagnostic; not a conformance gate')
    args = parser.parse_args()
    try:
        encoded = args.input.read_bytes()
        original_assets(encoded, args.reference.read_bytes(), args.source_a00.read_bytes())
        console = (args.run/'console.log').read_text()
        if [line for line in console.splitlines() if 'Fatal error:' in line] != ['Fatal error: '+STOP]:
            raise ValueError('missing or unexpected intentional short-input termination')
        result = check_stream((args.run/'error.log').read_text(),
            (args.run/'mu4_aac_abi_raw.s32le').read_bytes(),
            (args.run/'mu4_aac_abi_converted.s16le').read_bytes(), encoded)
        if args.ffmpeg:
            command=[args.ffmpeg,'-v','error','-i',str(args.input),'-map','0:a:0',
                     '-c:a','pcm_f32le','-f','f32le','pipe:1']
            host=subprocess.run(command,check=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            version=subprocess.run([args.ffmpeg,'-version'],check=True,stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE).stdout.decode().splitlines()[0]
            result['host_diagnostic']=compare_host_pcm(
                (args.run/'mu4_aac_abi_raw.s32le').read_bytes(),host.stdout,343*1024)
            result['host_diagnostic'].update(command=command,version=version)
    except (OSError, ValueError, KeyError, TypeError,subprocess.SubprocessError) as error:
        parser.exit(1, f'ISO AL09 native probe: FAIL: {error}\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()

"""Bind a native decoder prefix to consumed reference bytes, not all-file audio."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys

if __package__:
    from .mu4_playback_frame_check import rows
    from .mu4_processing_check import match_packets
    from .mu4_recorder_media_inventory import adts_header
    from .mu4_decoder_session_check import verify as verify_session
else:
    from mu4_playback_frame_check import rows
    from mu4_processing_check import match_packets
    from mu4_recorder_media_inventory import adts_header
    from mu4_decoder_session_check import verify as verify_session


def check_packets(text, aac):
    scopes, ends = rows(text,'mu4_packet_read_scope'), rows(text,'mu4_packet_read_terminal')
    reads, calls = rows(text,'mu4_packet_read'), rows(text,'mu4_decoder_call')
    if len(scopes) != 1 or scopes[0] != dict(first='0.8',last='10.0',first_address='162e',
            last_address='1e2d',cap='131072',ownership='data_bus',pc='context_only',
            observer_writes='0',audio_acceptance='0') or len(ends) != 1:
        raise ValueError('missing or changed packet capture scope/terminal')
    try:
        end = ends[0]
        stop = float(end['t'])
        if (not 0 < len(reads) == int(end['reads']) < 131072 or end['closed'] != '1'
                or end['illegal'] != '0' or end['audio_acceptance'] != '0'
                or not math.isfinite(stop) or stop < 10):
            raise ValueError('incomplete, capped or faulted packet capture')
        if not calls or len(calls) % 2 or len(calls) >= 1024:
            raise ValueError('incomplete decoder call pairs')
        previous = 0.8
        for index, read in enumerate(reads,1):
            when = float(read['t'])
            if (int(read['ordinal']) != index or not previous <= when <= 10
                    or int(read['mask'],16) != 0xffff
                    or not 0x162e <= int(read['address'],16) <= 0x1e2d
                    or not 0 <= int(read['word'],16) <= 0xffff
                    or not 0x2510c <= int(read['context_pc'],16) <= 0x25143):
                raise ValueError('packet bus-read identity/domain/order changed')
            previous = when
        entries, returns, headers, offset, previous = [], [], [], 0, 0.8
        for index in range(len(calls)//2):
            entry, returned = calls[2*index:2*index+2]
            if (entry['phase'],returned['phase']) != ('entry','return') or any(
                    row['call'] != str(index+1) or row['identity'] != 'Execute_entry'
                    or row['context_only'] != '1' for row in (entry,returned)):
                raise ValueError('decoder call identity/order changed')
            first, last = float(entry['t']), float(returned['t'])
            if (not previous <= first < last <= 10 or
                    not 0x162e <= int(entry['base'],16) <= 0x1e2d or
                    not 0 <= int(returned['result'],16) < 1 << 40 or
                    int(returned['result'],16) & 0xffff):
                raise ValueError('uncovered input domain or failed/censored decoder call')
            previous = last
            header = adts_header(aac,offset)
            if not header or not header['frame_within_file']:
                raise ValueError('reference lacks the next complete ADTS-framed packet')
            offset += header['frame_bytes']
            headers.append(header)
            entries.append(entry)
            returns.append(returned)
        proofs = match_packets(entries,returns,reads,aac[:offset])
    except (KeyError,TypeError,OverflowError) as error:
        raise ValueError('incomplete packet capture fields') from error
    single_blocks = all(header['raw_data_blocks_minus_one'] == 0 for header in headers)
    tail, tail_headers = offset, 0
    while tail < len(aac):
        header = adts_header(aac,tail)
        if not header or not header['frame_within_file']:
            break
        tail += header['frame_bytes']
        tail_headers += 1
    assigned = sum(proof['actual_reads'] for proof in proofs)
    return dict(calls=len(proofs), consumed_prefix_bytes=offset,
                consumed_prefix_sha256=hashlib.sha256(aac[:offset]).hexdigest(),
                captured_reads=len(reads), assigned_reads=assigned,
                reads_outside_calls=len(reads)-assigned, packets=proofs,
                single_raw_block_packets=single_blocks,
                reference_bytes=len(aac), unconsumed_reference_bytes=len(aac)-offset,
                remaining_complete_adts_headers=tail_headers, remaining_unparsed_bytes=len(aac)-tail,
                fitted_alignment=False, native_audio_validated=False,
                scope='Consumed decoder prefix only; tail headers are framing, not payload validity; '
                      'no whole-file, retired-store, physical audio or speech acceptance')


def verify(directory, source, aac, pcm):
    session = verify_session(directory,source,pcm)
    result = check_packets((directory/'error.log').read_text(),aac)
    if (result['calls'] != session['calls'] or session['failed_calls'] or
            not result['single_raw_block_packets'] or session['converted_blocks'] != result['calls']):
        raise ValueError('packet prefix differs from the complete native session')
    result['session'] = {key: session[key] for key in ('calls','converted_blocks',
        'native_return_sha256','reference_blocks_not_compared','reference_comparison_diagnostic',
        'bounded_eight_call_arithmetic_validated')}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--source-a00',type=Path,required=True)
    parser.add_argument('--reference-aac',type=Path,required=True)
    parser.add_argument('--reference-pcm',type=Path,required=True)
    args = parser.parse_args()
    try:
        result = verify(args.directory,args.source_a00.read_bytes(),args.reference_aac.read_bytes(),args.reference_pcm.read_bytes())
    except (ValueError,OSError) as error:
        parser.exit(1,f'MU4 packet session: {error}\n')
    print(json.dumps(result,indent=2))


if __name__ == '__main__':
    main()

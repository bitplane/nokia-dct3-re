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
    from .mu4_decoder_session_check import check_session, FILE
    from .mu4_context_contract import contract, recording_session_observations
    from .mu4_coefficient_check import verify as verify_arithmetic
else:
    from mu4_playback_frame_check import rows
    from mu4_processing_check import match_packets
    from mu4_recorder_media_inventory import adts_header
    from mu4_decoder_session_check import verify as verify_session
    from mu4_decoder_session_check import check_session, FILE
    from mu4_context_contract import contract, recording_session_observations
    from mu4_coefficient_check import verify as verify_arithmetic


def check_packets(text, aac, *, window_last=10.0):
    if window_last not in (10.0, 12.0):
        raise ValueError('unsupported packet observation window')
    scopes, ends = rows(text,'mu4_packet_read_scope'), rows(text,'mu4_packet_read_terminal')
    reads, calls = rows(text,'mu4_packet_read'), rows(text,'mu4_decoder_call')
    if len(scopes) != 1 or scopes[0] != dict(first='0.8',last=f'{window_last:.1f}',first_address='162e',
            last_address='1e2d',cap='131072',ownership='data_bus',pc='context_only',
            observer_writes='0',audio_acceptance='0') or len(ends) != 1:
        raise ValueError('missing or changed packet capture scope/terminal')
    try:
        end = ends[0]
        stop = float(end['t'])
        if (not 0 < len(reads) == int(end['reads']) < 131072 or end['closed'] != '1'
                or end['illegal'] != '0' or end['audio_acceptance'] != '0'
                or not math.isfinite(stop) or stop < window_last):
            raise ValueError('incomplete, capped or faulted packet capture')
        if not calls or len(calls) % 2 or len(calls) >= 1024:
            raise ValueError('incomplete decoder call pairs')
        previous = 0.8
        for index, read in enumerate(reads,1):
            when = float(read['t'])
            if (int(read['ordinal']) != index or not previous <= when <= window_last
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
            if (not previous <= first < last <= window_last or
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


def extract_observed_packets(text, *, window_last=10.0):
    """Recover only covered bus/reservoir bytes; never synthesize decoder input."""
    calls, reads = rows(text, 'mu4_decoder_call'), rows(text, 'mu4_packet_read')
    observed, offset = {}, 0

    def insert(position, value):
        if position in observed and observed[position] != value:
            raise ValueError('conflicting observed packet bytes')
        observed[position] = value

    try:
        if not calls or len(calls) % 2:
            raise ValueError('incomplete decoder call pairs')
        for index in range(0, len(calls), 2):
            entry, returned = calls[index:index + 2]
            remaining, retained = int(entry['remaining'], 16), int(returned['remaining'], 16)
            if remaining not in (0, 8, 16, 24, 32) or retained not in (0, 8, 16, 24, 32):
                raise ValueError('packet boundary is not byte-aligned')
            start = int(entry['index'], 16) * 2 - remaining // 8
            length = int(returned['index'], 16) * 2 - retained // 8 - start
            if not 0 < length <= 8191:
                raise ValueError('invalid observed packet extent')
            prefix = int(entry['reservoir'], 16).to_bytes(4, 'big')[4 - remaining // 8:]
            for at, value in enumerate(prefix):
                insert(offset + at, value)
            for read in reads:
                if not float(entry['t']) <= float(read['t']) <= float(returned['t']):
                    continue
                relative = (int(read['address'], 16) - int(entry['base'], 16)) * 2 - start
                for at, value in enumerate(int(read['word'], 16).to_bytes(2, 'big'), relative):
                    if not 0 <= at < length + retained // 8:
                        raise ValueError('observed read outside packet/lookahead')
                    insert(offset + at, value)
            if any(position not in observed for position in range(offset, offset + length)):
                raise ValueError('missing observed packet bytes')
            offset += length
        data = bytes(observed[position] for position in range(offset))
        report = check_packets(text, data, window_last=window_last)
    except (KeyError, TypeError, OverflowError) as error:
        raise ValueError('incomplete observed packet fields') from error
    report['reference_independent'] = False
    report['scope'] = 'Observed bus/reservoir extraction for host diagnostics only; not emulation input or independent reference'
    return data, report


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


def verify_observed_prefix(directory, source, encoder_text, pcm, *, window_last=12.0):
    """Compose same-recording checks without inheriting the protected track's title."""
    contract(source)
    text = (directory/'error.log').read_text()
    data, packets = extract_observed_packets(text, window_last=window_last)
    encoder = recording_session_observations(encoder_text, data, complete_reference_prefix=True)
    session = check_session(text, (directory/FILE).read_bytes(), pcm)
    arithmetic = verify_arithmetic(directory, source)
    if (session['failed_calls'] or session['calls'] != packets['calls'] or
            session['converted_blocks'] != packets['calls'] or
            session['native_blocks_not_compared'] or session['reference_blocks_not_compared'] or
            not packets['single_raw_block_packets'] or not arithmetic['coefficient_publication_validated']):
        raise ValueError('observed packet, encoder, arithmetic and PCM prefix scopes disagree')
    return dict(calls=packets['calls'], consumed_prefix_bytes=len(data),
                consumed_prefix_sha256=packets['consumed_prefix_sha256'],
                encoder=encoder, session={key: value for key, value in session.items() if key != 'frame_metrics'},
                bounded_eight_call_arithmetic_validated=True,
                independent_encoded_reference=False, transport_gate_validated=False,
                native_audio_validated=False,
                scope='Same-recording consumed prefix and bounded arithmetic; host PCM diagnostic only; '
                      'not protected-title transport gate, whole-file completion, analog audio or speech')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--source-a00',type=Path)
    parser.add_argument('--reference-aac',type=Path)
    parser.add_argument('--reference-pcm',type=Path)
    parser.add_argument('--export-observed-aac',type=Path,
                        help='exclusive new host-diagnostic artifact; never emulation input')
    parser.add_argument('--window-last',type=float,choices=(10.0,12.0),default=10.0)
    parser.add_argument('--encoder-log',type=Path,
                        help='verify observed prefix against its own encoder and host PCM, not whole-file acceptance')
    args = parser.parse_args()
    try:
        if args.encoder_log:
            if not args.source_a00 or not args.reference_pcm or args.reference_aac or args.export_observed_aac:
                raise ValueError('encoder-prefix verification requires source and host PCM only')
            result = verify_observed_prefix(args.directory,args.source_a00.read_bytes(),
                args.encoder_log.read_text(),args.reference_pcm.read_bytes(),window_last=args.window_last)
        elif args.export_observed_aac:
            if any((args.source_a00,args.reference_aac,args.reference_pcm)):
                raise ValueError('observed extraction is not independent-reference verification')
            data, result = extract_observed_packets((args.directory/'error.log').read_text(),
                                                   window_last=args.window_last)
            with args.export_observed_aac.open('xb') as output:
                output.write(data)
        else:
            if not all((args.source_a00,args.reference_aac,args.reference_pcm)) or args.window_last != 10.0:
                raise ValueError('reference verification requires all three inputs and the original window')
            result = verify(args.directory,args.source_a00.read_bytes(),args.reference_aac.read_bytes(),args.reference_pcm.read_bytes())
    except (ValueError,OSError,KeyError) as error:
        parser.exit(1,f'MU4 packet session: {error}\n')
    print(json.dumps(result,indent=2))


if __name__ == '__main__':
    main()

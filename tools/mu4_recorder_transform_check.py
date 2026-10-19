"""Compare bounded native recorder snapshots/outputs with independent file readback."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

if __package__:
    from .mu4_context_contract import contract, reference_recording_windows, reference_transform_block
    from .mu4_recorder_media_inventory import adts_header
else:
    from mu4_context_contract import contract, reference_recording_windows, reference_transform_block
    from mu4_recorder_media_inventory import adts_header


def records(text, marker):
    result = []
    for line in text.splitlines():
        if marker + ':' not in line:
            continue
        fields = {}
        for token in line.split(marker + ':', 1)[1].split():
            key, separator, value = token.partition('=')
            if not separator or key in fields:
                raise ValueError(f'malformed {marker} record')
            fields[key] = value
        if fields.get('caller') not in ('03ee95', '03edae'):
            raise ValueError('unknown transform caller')
        if fields['caller'] == '03ee95':
            result.append(fields)
    return result


def number(row, key, *, base=16, bits=16):
    value = int(row[key], base)
    if not 0 <= value < 1 << bits:
        raise ValueError(f'{key} outside its word domain')
    return value


def word_list(value, count, *, bits=16):
    words = [int(word, 16) for word in value.split(',')]
    if len(words) != count or any(not 0 <= word < 1 << bits for word in words):
        raise ValueError('word-list shape or domain mismatch')
    return words


def verify(text, saved, samples=96):
    """Return a research report and captured plaintext, never an emulation input."""
    if not 1 <= samples <= 96:
        raise ValueError('expected one to 96 bounded samples')
    overlay = ('mu4_native_recorder_overlay: PASS marker=aa88 records=4622 words=73766 '
               'checked_ram_words=73762 excluded_register_words=4 before_execution=1')
    if text.count(overlay) != 1 or text.find('mu4_record_transform_input:') < text.index(overlay):
        raise ValueError('missing preceding verified AA88 upload')
    inputs = records(text, 'mu4_record_transform_input')
    outers = records(text, 'mu4_record_transform_outer')
    outputs = records(text, 'mu4_record_transform_output')
    if any(len(rows) != samples or [int(row['sample']) for row in rows] != list(range(1,samples+1))
           for rows in (inputs, outers, outputs)):
        raise ValueError('incomplete, duplicate or reordered transform samples')
    native, plain = bytearray(), bytearray()
    states, cursors = set(), []
    model_matches = source_matches = 0
    previous = 0.0
    for item, outer, output in zip(inputs, outers, outputs):
        when, ended = float(item['t']), float(output['t'])
        if not 0.8 <= when < ended <= 2 or when <= previous or float(outer['t']) != when:
            raise ValueError('transform sample timestamp scope/order mismatch')
        previous = ended
        if (item.get('ownership') != 'unvalidated' or output.get('ownership') != 'unvalidated' or
                outer.get('context_only') != '1' or output.get('audio_acceptance') != '0' or
                any(row.get('observer_writes') != '0' for row in (item, outer, output)) or
                item.get('state_base') != 'fc6c' or
                not 0x3f108 <= number(item, 'context_pc', bits=24) <= 0x3f10e):
            raise ValueError('transform evidence identity/scope mismatch')
        target = number(item, 'target')
        if not 0x80 <= target < 0xffff or number(output, 'target') != target:
            raise ValueError('transform output pointer mismatch')
        if number(outer, 'candidate_sp') != ((number(item, 'sp') + 10) & 0xffff):
            raise ValueError('outer frame does not match recovered prologue')
        args = word_list(outer['words_16_1b'], 6)
        cursor_pointer = number(outer, 'cursor_pointer')
        if (args != [0,32,cursor_pointer,0,28,4] or not 0x80 <= cursor_pointer < 0xffff or
                number(outer, 'position', bits=32) != 0 or not 0x80 <= number(outer,'source') <= 0xffe0):
            raise ValueError('sample outside admitted aligned 32-byte window domain')
        cursors.append(number(outer, 'cursor', bits=32))
        state = tuple(word_list(item['state'], 82))
        states.add(state)
        source = bytes(word_list(outer['source_words'], 32, bits=8))
        entry = number(item,'left'), number(item,'right')
        returned = number(output,'left'), number(output,'right')
        source_matches += source[:4] == b''.join(word.to_bytes(2,'little') for word in entry)
        model_matches += reference_transform_block(*entry, state, reverse=False) == returned
        plain.extend(source)
        native.extend(b''.join(word.to_bytes(2,'little') for word in returned) + source[4:])
    # Establish the origin from the entire first chunk, not an assumed seek.
    hits, offset = [], saved.find(native[:32])
    while offset >= 0:
        hits.append(offset)
        offset = saved.find(native[:32], offset+1)
    if len(hits) != 1:
        raise ValueError(f'first native chunk has {len(hits)} file origins, not a unique mapping')
    origin = hits[0]
    if origin + len(native) > len(saved):
        raise ValueError('matched file lacks complete observed prefix')
    mismatches = [index for index in range(samples)
                  if native[index*32:index*32+32] != saved[origin+index*32:origin+index*32+32]]
    cursor_contiguous = cursors == list(range(cursors[0],cursors[0]+32*samples,32)) and cursors[0] % 32 == 0
    report = {'samples': samples, 'model_matches': model_matches, 'source_prefix_matches': source_matches,
              'saved_chunk_matches': samples-len(mismatches), 'first_mismatched_samples': [i+1 for i in mismatches[:8]],
              'file_origin': origin, 'compared_bytes': len(native), 'state_variants': len(states),
              'cursor_first': cursors[0], 'cursor_last': cursors[-1], 'cursor_step_32': cursor_contiguous,
              'plaintext_sha256': hashlib.sha256(plain).hexdigest(),
              'saved_sha256': hashlib.sha256(saved).hexdigest(), 'native_audio_validated': False,
              'scope': '96-call-cap caller snapshots and actual output writes; bounded file correspondence, not retained operands, tail rules or codec/audio acceptance'}
    report['consistent'] = model_matches == source_matches == samples and not mismatches and cursor_contiguous
    return report, bytes(plain)


def project_complete_windows(text, saved, samples=96, packet_limit=None, *, comparison_prefix=None):
    """Host-only extension of one captured state; later native state is unknown."""
    proof, observed = verify(text,saved,samples)
    if not proof['consistent'] or proof['state_variants'] != 1:
        raise ValueError('host projection requires a consistent single-state native prefix')
    state = word_list(records(text,'mu4_record_transform_input')[0]['state'],82)
    origin = proof['file_origin']
    extent = (len(saved)-origin)//32*32
    projected = reference_recording_windows(saved[origin:origin+extent],state)
    if projected[:len(observed)] != observed:
        raise ValueError('host inverse projection disagrees with captured plaintext')
    if comparison_prefix is not None and (not comparison_prefix or
            len(comparison_prefix) > len(projected) or
            projected[:len(comparison_prefix)] != comparison_prefix):
        raise ValueError('supplied comparison prefix differs at fixed byte-zero origin')
    cursor, headers = 0, []
    stop = 'exact_projection_end'
    while cursor < len(projected):
        header = adts_header(projected,cursor)
        if header is None:
            stop = 'non_header'
            break
        if not header['frame_within_file']:
            stop = 'truncated_frame'
            break
        headers.append(header)
        cursor += header['frame_bytes']
    if not headers:
        raise ValueError('host projection contains no complete initial ADTS packet')
    fields = ('mpeg_version','profile_bits','sampling_index','channel_configuration','raw_data_blocks_minus_one')
    configurations = sorted({tuple(header[key] for key in fields) for header in headers})
    payload = projected[:cursor]
    if packet_limit is not None and (not isinstance(packet_limit,int) or not 1 <= packet_limit <= len(headers)):
        raise ValueError('projection packet limit exceeds the complete header chain')
    selected = len(headers) if packet_limit is None else packet_limit
    output = payload[:sum(header['frame_bytes'] for header in headers[:selected])]
    report = {'captured_prefix_bytes':len(observed), 'projected_window_bytes':extent,
        'complete_packets':len(headers), 'packet_bytes':cursor,
        'packet_sha256':hashlib.sha256(payload).hexdigest(),
        'output_packets':selected, 'output_bytes':len(output),
        'output_sha256':hashlib.sha256(output).hexdigest(),
        'unparsed_projected_bytes':extent-cursor, 'unmodeled_tail_bytes':len(saved)-origin-extent,
        'stop_reason':stop, 'configurations':[dict(zip(fields,values)) for values in configurations],
        'state_sha256':hashlib.sha256(b''.join(word.to_bytes(2,'big') for word in state)).hexdigest(),
        'native_beyond_prefix_validated':False, 'native_audio_validated':False,
        'scope':'Host-only constant-state projection over complete 32-byte windows; headers are not payload/CRC or later native-state validation'}
    if stop == 'truncated_frame':
        report['partial_next_frame_bytes'] = len(projected)-cursor
        report['next_frame_declared_bytes'] = header['frame_bytes']
    if comparison_prefix is not None:
        report['supplied_prefix_correspondence'] = dict(bytes=len(comparison_prefix),
            sha256=hashlib.sha256(comparison_prefix).hexdigest(), fitted_alignment=False,
            scope='Byte-zero equality only; supplied prefix provenance must be validated separately')
    return report, output


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('extracted_file', type=Path)
    parser.add_argument('--source-a00', type=Path, required=True)
    parser.add_argument('--samples', type=int, default=96)
    parser.add_argument('--output-projected-aac',type=Path,
                        help='create a new host-only complete-packet projection; never an emulation input')
    parser.add_argument('--project-packet-limit',type=int,help='explicit output prefix; report still inventories the complete header chain')
    parser.add_argument('--comparison-prefix',type=Path,
                        help='require byte-zero equality with separately supplied bytes; does not validate their provenance')
    args = parser.parse_args(argv)
    try:
        if (args.project_packet_limit is not None or args.comparison_prefix) and args.output_projected_aac is None:
            raise ValueError('projection options require an explicit output path')
        source = args.source_a00.read_bytes()
        contract(source)
        result, _ = verify(args.log.read_text(), args.extracted_file.read_bytes(), args.samples)
        result['source_a00_sha256'] = hashlib.sha256(source).hexdigest()
        if args.output_projected_aac is not None:
            inputs = [args.log,args.extracted_file,args.source_a00]
            if args.comparison_prefix:
                inputs.append(args.comparison_prefix)
            if args.output_projected_aac.resolve() in [path.resolve() for path in inputs]:
                raise ValueError('projection output must not replace input evidence')
            projection,payload = project_complete_windows(args.log.read_text(),args.extracted_file.read_bytes(),
                args.samples,args.project_packet_limit,
                comparison_prefix=args.comparison_prefix.read_bytes() if args.comparison_prefix else None)
            with args.output_projected_aac.open('xb') as output:
                output.write(payload)
            result['host_projection'] = projection
    except (OSError, KeyError, ValueError) as error:
        print(f'MU4 recording transform observation: {error}', file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if result['consistent'] else 2


if __name__ == '__main__':
    raise SystemExit(main())

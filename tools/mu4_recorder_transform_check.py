"""Compare bounded native recorder snapshots/outputs with independent file readback."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

if __package__:
    from .mu4_context_contract import contract, reference_transform_block
else:
    from mu4_context_contract import contract, reference_transform_block


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


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('extracted_file', type=Path)
    parser.add_argument('--source-a00', type=Path, required=True)
    parser.add_argument('--samples', type=int, default=96)
    args = parser.parse_args(argv)
    try:
        source = args.source_a00.read_bytes()
        contract(source)
        result, _ = verify(args.log.read_text(), args.extracted_file.read_bytes(), args.samples)
        result['source_a00_sha256'] = hashlib.sha256(source).hexdigest()
    except (OSError, KeyError, ValueError) as error:
        print(f'MU4 recording transform observation: {error}', file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if result['consistent'] else 2


if __name__ == '__main__':
    raise SystemExit(main())

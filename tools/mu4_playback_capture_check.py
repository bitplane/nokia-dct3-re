"""Validate observation-only MU4 word captures; no decoded-audio acceptance."""
import argparse
from array import array
import hashlib
import json
from pathlib import Path
import re
import sys

if __package__:
    from .noki5510_bootstrap_check import verify_playback_dma
else:
    from noki5510_bootstrap_check import verify_playback_dma


FILES = {'mcbsp_tx':'mu4_playback_tx.le16', 'codec_din':'mu4_playback_din.le16',
         'dma_source':'mu4_playback_dma.le16'}
ROW = re.compile(
    r'mu4_playback_word_capture: boundary=(mcbsp_tx|codec_din|dma_source) '
    r'words=(\d+) bytes=(\d+) hash=([0-9a-f]{8}) encoding=le16 file=([a-z0-9_.]+) '
    r'save_restore=unsupported observer_writes=0 audio_acceptance=0$')


def parse_rows(text):
    rows = {}
    for line in text.splitlines():
        if 'mu4_playback_word_capture:' not in line:
            continue
        found = ROW.search(line)
        if not found or found[1] in rows or FILES[found[1]] != found[5]:
            raise ValueError('malformed, duplicate or renamed playback capture')
        rows[found[1]] = {'words':int(found[2]), 'bytes':int(found[3]),
                         'hash':found[4], 'file':found[5]}
    if set(rows) != set(FILES):
        raise ValueError('missing playback capture boundary')
    return rows


def validate_stream(row, data):
    if len(data) % 2 or len(data) != row['bytes'] or len(data) != 2*row['words']:
        raise ValueError('playback capture word/byte count mismatch')
    words = array('H')
    words.frombytes(data)
    if sys.byteorder != 'little':
        words.byteswap()
    fingerprint = 2166136261
    for word in words:
        fingerprint = ((fingerprint ^ (word >> 8))*16777619) & 0xffffffff
        fingerprint = ((fingerprint ^ (word & 255))*16777619) & 0xffffffff
    if f'{fingerprint:08x}' != row['hash']:
        raise ValueError('playback capture disagrees with observed word hash')
    return words, {'words':len(words), 'nonzero':sum(word != 0 for word in words),
                   'sha256':hashlib.sha256(data).hexdigest(), 'observed_hash':row['hash']}


def compare_streams(source, sink, anchor=64):
    """Chronological first-nonzero alignment, never a best-fit waveform search."""
    if not isinstance(anchor,int) or anchor < 1:
        raise ValueError('capture alignment requires a positive anchor length')
    first = [next((i for i, word in enumerate(words) if word), None)
             for words in (source, sink)]
    result = {'source_words':len(source), 'sink_words':len(sink),
              'first_nonzero':first, 'anchor_words':anchor,
              'alignment_scope':'Initial chronological anchor only; reset/framing epochs are not aligned',
              'alignment_established':False, 'transport_pairing_validated':False}
    if None in first or min(len(source)-first[0],len(sink)-first[1]) < anchor:
        return result
    offset = first[1]-first[0]
    result['sink_offset_words'] = offset
    if source[first[0]:first[0]+anchor] != sink[first[1]:first[1]+anchor]:
        return result
    start_source, start_sink = max(0,-offset), max(0,offset)
    count = min(len(source)-start_source,len(sink)-start_sink)
    mismatches = 0
    examples = []
    for i in range(count):
        left, right = source[start_source+i], sink[start_sink+i]
        if left != right:
            mismatches += 1
            if len(examples) < 8:
                examples.append({'source_index':start_source+i,'sink_index':start_sink+i,
                                 'source_word':f'{left:04x}','sink_word':f'{right:04x}'})
    result.update(alignment_established=True, compared_words=count,
                  mismatched_words=mismatches, first_mismatches=examples,
                  exact_prefix_words=examples[0]['source_index']-start_source if examples else count,
                  source_prefix_words=start_source, sink_prefix_words=start_sink,
                  source_suffix_words=len(source)-start_source-count,
                  sink_suffix_words=len(sink)-start_sink-count,
                  source_extra_nonzero=sum(word != 0 for word in source[:start_source])+sum(word != 0 for word in source[start_source+count:]),
                  sink_extra_nonzero=sum(word != 0 for word in sink[:start_sink])+sum(word != 0 for word in sink[start_sink+count:]))
    return result


def verify(text, directory):
    observation = verify_playback_dma(text)
    rows = parse_rows(text)
    streams, reports = {}, {}
    for name, row in rows.items():
        streams[name], reports[name] = validate_stream(row,(directory/FILES[name]).read_bytes())
        prefix = {'mcbsp_tx':'tx', 'codec_din':'din', 'dma_source':'dma'}[name]
        count_key = 'dma_reads' if prefix == 'dma' else prefix+'_words'
        if reports[name]['words'] != observation[count_key] or reports[name]['nonzero'] != observation[prefix+'_nonzero']:
            raise ValueError('capture disagrees with terminal boundary counters')
        if prefix != 'dma' and row['hash'] != observation[prefix+'_hash']:
            raise ValueError('capture disagrees with terminal serial hash')
    return {'capture_integrity_validated':True,
            'streams':reports, 'tx_to_din':compare_streams(streams['mcbsp_tx'],streams['codec_din']),
            'dma_to_tx':compare_streams(streams['dma_source'],streams['mcbsp_tx']),
            'native_audio_validated':False,
            'scope':'Complete fresh-run observed words; no save/restore capture, waveform quality, analog audio or speech claim'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    args = parser.parse_args()
    try:
        report = verify((args.directory/'error.log').read_text(),args.directory)
    except (OSError,ValueError) as error:
        print(f'MU4 playback capture: {error}',file=sys.stderr)
        return 1
    print(json.dumps(report,indent=2,sort_keys=True))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

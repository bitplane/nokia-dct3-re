"""Validate observation-only MU4 word captures; no decoded-audio acceptance."""
import argparse
from array import array
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import re
import struct
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
SLOT_FILE = 'mu4_playback_din.slots'
SLOT_ROW = re.compile(
    r'mu4_playback_slot_capture: records=(\d+) bytes=(\d+) fnv1a=([0-9a-f]{8}) '
    r'encoding=le64ns_u8slot file=mu4_playback_din.slots '
    r'save_restore=unsupported observer_writes=0 audio_acceptance=0$')


def parse_slot_row(text, required=False):
    lines = [line for line in text.splitlines() if 'mu4_playback_slot_capture:' in line]
    if not lines and not required:
        return None
    if len(lines) != 1 or not (match := SLOT_ROW.search(lines[0])):
        raise ValueError('missing, malformed or duplicate codec slot capture')
    return {'records':int(match[1]),'bytes':int(match[2]),'hash':match[3]}


def validate_slots(row, data, word_count):
    if len(data) != row['bytes'] or len(data) != 9*row['records'] or row['records'] != word_count or not word_count:
        raise ValueError('codec slot/word count mismatch')
    fingerprint = 2166136261
    for byte in data:
        fingerprint = ((fingerprint ^ byte)*16777619) & 0xffffffff
    if f'{fingerprint:08x}' != row['hash']:
        raise ValueError('codec slot capture hash mismatch')
    previous_time = previous_slot = pending = last_frame = None
    slots, pair_intervals, frame_intervals = Counter(), Counter(), Counter()
    frames = orphans = discontinuities = 0
    examples = []
    first = []
    for index,(when,slot) in enumerate(struct.iter_unpack('<QB',data)):
        if slot not in (0,1) or (previous_time is not None and when <= previous_time):
            raise ValueError('invalid codec slot or non-increasing timestamp')
        if len(first) < 4:
            first.append({'word_index':index,'time_ns':when,'slot':slot})
        slots[slot] += 1
        if slot == previous_slot:
            discontinuities += 1
            if len(examples) < 8:
                examples.append({'word_index':index,'time_ns':when,'slot':slot})
        if slot == 0:
            if pending is not None:
                orphans += 1
            pending = when
        elif pending is None:
            orphans += 1
        else:
            frames += 1
            pair_intervals[when-pending] += 1
            if last_frame is not None:
                frame_intervals[pending-last_frame] += 1
            last_frame = pending
            pending = None
        previous_time,previous_slot = when,slot
    orphans += pending is not None
    return {'capture_integrity_validated':True,'records':word_count,
            'sha256':hashlib.sha256(data).hexdigest(),'observed_hash':row['hash'],
            'slot_counts':dict(slots),'observed_frame_pairs':frames,'orphan_words':orphans,
            'slot_discontinuities':discontinuities,'first_discontinuities':examples,
            'first_records':first,'pair_interval_ns':dict(pair_intervals),
            'frame_interval_ns':dict(frame_intervals),
            'scope':'Actual adjacent slot-0/slot-1 callbacks; no packet alignment, waveform, analog or speech acceptance'}


def framed_words(words, slot_data):
    """Yield actual adjacent 0/1 callback pairs, never inferred index parity."""
    if len(slot_data) != 9*len(words):
        raise ValueError('codec framing requires one metadata record per word')
    pending = None
    for index,(when,slot) in enumerate(struct.iter_unpack('<QB',slot_data)):
        if slot not in (0,1):
            raise ValueError('invalid codec slot')
        if slot == 0:
            pending = (index,when,words[index])
        elif pending is not None:
            yield pending[0],pending[1],(pending[2],words[index])
            pending = None


def compare_reference(words, slot_data, reference, rate):
    """Diagnostic first-signal comparison, not a fitted or accepted AAC alignment."""
    if not isinstance(rate,int) or rate <= 0 or len(reference) % 4:
        raise ValueError('reference requires a positive sample rate and complete LE16 stereo frames')
    reference_words = array('h')
    reference_words.frombytes(reference)
    if sys.byteorder != 'little':
        reference_words.byteswap()
    start = next((i for i in range(len(reference_words)//2) if any(reference_words[2*i:2*i+2])),None)
    if start is None:
        raise ValueError('reference has no nonzero frame')
    samples = [[],[]]
    native_start = previous = None
    reason = 'reference_exhausted'
    target = len(reference_words)//2-start
    for ordinal,(index,when,pair) in enumerate(framed_words(words,slot_data)):
        if native_start is None:
            if not any(pair):
                continue
            native_start = {'frame_ordinal':ordinal,'word_index':index,'time_ns':when}
        elif abs((when-previous)*rate-1_000_000_000) > rate:
            reason = 'native_cadence_discontinuity'
            break
        for channel in (0,1):
            word = pair[channel]
            samples[channel].append(word-65536 if word & 0x8000 else word)
        previous = when
        if len(samples[0]) == target:
            break
    else:
        reason = 'native_exhausted'
    if native_start is None or len(samples[0]) < 2:
        raise ValueError('insufficient framed native signal for reference comparison')
    count = len(samples[0])
    refs = [list(reference_words[2*start+channel:2*(start+count):2]) for channel in (0,1)]
    metrics = {}
    for native_slot in (0,1):
        for reference_channel in (0,1):
            observed,expected = samples[native_slot],refs[reference_channel]
            xsum,ysum = sum(observed),sum(expected)
            xx,yy = sum(x*x for x in observed),sum(y*y for y in expected)
            xy = sum(x*y for x,y in zip(observed,expected))
            variance = (count*xx-xsum*xsum)*(count*yy-ysum*ysum)
            error = sum((x-y)**2 for x,y in zip(observed,expected))
            metrics[f'slot_{native_slot}_reference_{reference_channel}'] = {
                'mismatched_samples':sum(x != y for x,y in zip(observed,expected)),
                'native_rms':math.sqrt(xx/count),'reference_rms':math.sqrt(yy/count),
                'error_rms':math.sqrt(error/count),
                'correlation':(count*xy-xsum*ysum)/math.sqrt(variance) if variance > 0 else None}
    return {'reference_sha256':hashlib.sha256(reference).hexdigest(),
            'reference_rate':rate,'reference_first_nonzero_frame':start,
            'native_first_nonzero_frame':native_start,'compared_frames':count,
            'window_stop':reason,'metrics':metrics,
            'alignment':'First chronological nonzero frame, no lag search, scaling or channel permutation applied',
            'cross_channel_metrics':'Diagnostic only; not a selected mapping',
            'packet_frame_alignment_validated':False,'native_audio_validated':False}


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


def locate_sink_mismatches(comparison, slot_data, word_count):
    """Locate the existing fixed-offset diagnostic, without realigning it."""
    if len(slot_data) != 9*word_count:
        raise ValueError('mismatch localization requires one slot record per sink word')
    result = []
    for mismatch in comparison.get('first_mismatches', []):
        index = mismatch['sink_index']
        if not 0 <= index < word_count:
            raise ValueError('mismatch sink index outside codec capture')
        when, slot = struct.unpack_from('<QB', slot_data, 9*index)
        if slot not in (0, 1):
            raise ValueError('invalid codec slot at mismatch')
        result.append(dict(mismatch, sink_time_ns=when, sink_slot=slot))
    return result


def verify(text, directory, *, require_framing=False, reference=None, reference_rate=44100):
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
    result = {'capture_integrity_validated':True,
            'streams':reports, 'tx_to_din':compare_streams(streams['mcbsp_tx'],streams['codec_din']),
            'dma_to_tx':compare_streams(streams['dma_source'],streams['mcbsp_tx']),
            'native_audio_validated':False,
            'scope':'Complete fresh-run observed words; no save/restore capture, waveform quality, analog audio or speech claim'}
    slot_row = parse_slot_row(text,require_framing or reference is not None)
    if slot_row is not None:
        data = (directory/SLOT_FILE).read_bytes()
        result['codec_slots'] = validate_slots(slot_row,data,len(streams['codec_din']))
        result['tx_to_din']['located_mismatches'] = locate_sink_mismatches(
            result['tx_to_din'], data, len(streams['codec_din']))
        if reference is not None:
            result['reference_comparison'] = compare_reference(streams['codec_din'],data,reference,reference_rate)
    else:
        result['codec_slots'] = {'capture_integrity_validated':False,'scope':'Legacy capture without slot metadata'}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--require-framing',action='store_true')
    parser.add_argument('--reference-pcm',type=Path,help='independently decoded signed LE16 stereo; diagnostic comparison only')
    parser.add_argument('--reference-rate',type=int,default=44100)
    args = parser.parse_args()
    try:
        reference = args.reference_pcm.read_bytes() if args.reference_pcm else None
        report = verify((args.directory/'error.log').read_text(),args.directory,
                        require_framing=args.require_framing,reference=reference,reference_rate=args.reference_rate)
    except (OSError,ValueError) as error:
        print(f'MU4 playback capture: {error}',file=sys.stderr)
        return 1
    print(json.dumps(report,indent=2,sort_keys=True))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

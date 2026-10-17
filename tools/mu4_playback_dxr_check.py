"""Bind actual channel-3 source reads to owned DXR writes, not transmission."""
import argparse
from array import array
from collections import Counter, deque
from decimal import Decimal
import heapq
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

if __package__:
    from .mu4_playback_capture_check import verify as verify_capture
else:
    from mu4_playback_capture_check import verify as verify_capture

FILE = 'mu4_playback_dxr.writes'
ROW = re.compile(
    r'mu4_playback_dxr_capture: records=(\d+) bytes=(\d+) fnv1a=([0-9a-f]{8}) '
    r'encoding=le64ns_u32ordinal_u16word_u8owner_u8reserved file=mu4_playback_dxr.writes '
    r'save_restore=unsupported observer_writes=0 audio_acceptance=0$')
CONTROL_ROW = re.compile(
    r'mu4_transport_config: address=(0038|0039) index=([0-9a-f]{2}) '
    r'value=([0-9a-f]{4}) mask=ffff t=(\d+\.\d{9}) context_pc=[0-9a-f]{6} observer_writes=0$')
CONTROL_END = re.compile(
    r'mu4_transport_terminal: configs=(\d+) dxr=\d+ first=(\d+\.\d{9}) '
    r'last=(\d+\.\d{9}) t=(\d+\.\d{9}) observer_writes=0$')


def nanoseconds(text):
    return int(Decimal(text)*1_000_000_000)


def parse_controls(text):
    lines = [line for line in text.splitlines() if 'mu4_transport_config:' in line]
    ends = [line for line in text.splitlines() if 'mu4_transport_terminal:' in line]
    if len(ends) != 1 or not (end := CONTROL_END.search(ends[0])):
        raise ValueError('missing or malformed transport control terminal')
    first, last, stop = map(nanoseconds, end.group(2,3,4))
    if len(lines) != int(end[1]) or not lines or not 0 <= first < last <= stop:
        raise ValueError('control observation count or coverage mismatch')
    selected = previous = None
    controls = []
    for line in lines:
        match = CONTROL_ROW.search(line)
        if match is None:
            raise ValueError('malformed transport control observation')
        address, index, value = (int(match[i],16) for i in (1,2,3))
        when = nanoseconds(match[4])
        if when > stop or (previous is not None and when < previous):
            raise ValueError('transport control time order or endpoint mismatch')
        if address == 0x38:
            selected = value & 31
        if index != selected:
            raise ValueError('control observation lost its indexed-register selection')
        if address == 0x39 and index == 1:
            controls.append((when,value))
        previous = when
    if not controls:
        raise ValueError('no SPCR2 writes in control observation')
    return controls, stop


def check_buffer_sequence(controls, stop, writes, tx, times):
    """Reset-aware bounded XSR/DXR sequence accounting, not load observation."""
    if not writes or not tx or len(tx) != len(times):
        raise ValueError('buffer accounting requires complete write/TX streams')
    if any(owner != 4 for when,ordinal,word,owner,reserved in writes):
        raise ValueError('buffer sequence currently requires exclusively owned channel-3 writes')
    if max(writes[-1][0], times[-1]) > stop:
        raise ValueError('control observation ends before the serial capture')
    start = min(writes[0][0],times[0])
    events = heapq.merge(((when,0,value) for when,value in controls),
        ((when,1,(ordinal,word)) for when,ordinal,word,owner,reserved in writes),
        ((when,2,word) for when,word in zip(times,tx)), key=lambda event:(event[0],event[1]))
    pending = deque()
    known = enabled = False
    retained = 0
    fresh = repeated = prefix_tx = prefix_writes = max_depth = 0
    discarded, ignored = [], []
    previous_time = previous_kind = None
    for when, kind, value in events:
        if when == previous_time and kind != previous_kind:
            raise ValueError('nanosecond precision cannot order control/write/TX events')
        previous_time, previous_kind = when, kind
        if kind == 0:
            enabled = bool(value & 1)
            if not enabled and when >= start:
                known = True
                discarded.extend(ordinal for ordinal,word in pending)
                pending.clear()
                retained = 0
        elif kind == 1:
            ordinal,word = value
            if not known:
                prefix_writes += 1
            elif not enabled:
                ignored.append(ordinal)
            else:
                pending.append((ordinal,word))
                retained = word
                max_depth = max(max_depth,len(pending))
                if len(pending) > 2:
                    raise ValueError('more than XSR+DXR pending; overwrite needs separate accounting')
        else:
            if not known:
                prefix_tx += 1
                continue
            if not enabled:
                raise ValueError('completed TX word while observed XRST is clear')
            if pending:
                ordinal,expected = pending.popleft()
                fresh += 1
            else:
                expected = retained
                repeated += 1
            if value != expected:
                raise ValueError(f'reset-aware buffer sequence differs from TX at {when} ns')
    if fresh+len(discarded)+len(ignored)+len(pending)+prefix_writes != len(writes):
        raise ValueError('buffer source-write accounting mismatch')
    if not known or not fresh+repeated:
        raise ValueError('no post-reset TX sequence coverage')
    return {'fresh_write_completions':fresh, 'retained_word_completions':repeated,
        'pre_reset_tx_prefix_unattributed':prefix_tx, 'pre_reset_writes_unattributed':prefix_writes,
        'reset_discarded_ordinals':discarded, 'reset_ignored_ordinals':ignored,
        'endpoint_pending_ordinals':[ordinal for ordinal,word in pending],
        'maximum_pending_words':max_depth, 'reset_aware_sequence_validated':True,
        'internal_buffer_loads_observed':False, 'native_audio_validated':False,
        'scope':'Ordered bounded XSR/DXR model after the first captured reset; no sample fitting, prefix ownership or direct internal-load evidence'}


def parse_row(text):
    lines = [line for line in text.splitlines() if 'mu4_playback_dxr_capture:' in line]
    if len(lines) != 1 or not (match := ROW.search(lines[0])):
        raise ValueError('missing, malformed or duplicate DXR ownership capture')
    return {'records':int(match[1]), 'bytes':int(match[2]), 'hash':match[3]}


def check_writes(row, data, source):
    if not source or not row['records'] or len(data) != row['bytes'] or len(data) != 16*row['records']:
        raise ValueError('DXR capture/source extent mismatch')
    fingerprint = 2166136261
    for byte in data:
        fingerprint = ((fingerprint ^ byte)*16777619) & 0xffffffff
    if f'{fingerprint:08x}' != row['hash']:
        raise ValueError('DXR capture hash mismatch')
    previous = None
    owners = Counter()
    consumed = 0
    for when, ordinal, word, owner, reserved in struct.iter_unpack('<QIHBB', data):
        if reserved or owner > 6 or (previous is not None and when < previous):
            raise ValueError('invalid DXR owner, reserved byte or timestamp order')
        owners['no_active_dma' if not owner else f'dma_channel_{owner-1}'] += 1
        if owner == 4:
            if ordinal != consumed+1 or consumed >= len(source):
                raise ValueError('channel-3 DXR write does not identify the next actual source read')
            if word != source[consumed]:
                raise ValueError('owned DXR write differs from its actual DMA source word')
            consumed += 1
        elif ordinal:
            raise ValueError('non-channel-3 DXR write carries a channel-3 source ordinal')
        previous = when
    if consumed != len(source):
        raise ValueError('actual channel-3 source reads lack owned DXR writes')
    return {'records':row['records'], 'sha256':hashlib.sha256(data).hexdigest(),
        'owner_counts':dict(owners), 'channel_3_source_reads_bound':consumed,
        'dma_to_dxr_validated':True, 'dxr_to_tx_validated':False,
        'native_audio_validated':False,
        'scope':'Actual DMA bus ownership and ordered source/write values only; DXR acceptance, overwrite, reset and transmission remain separate'}


def verify(directory, *, buffer_sequence=False):
    text = (directory/'error.log').read_text()
    capture = verify_capture(text, directory, require_framing=True, require_tx_times=True)
    source = array('H')
    source.frombytes((directory/'mu4_playback_dma.le16').read_bytes())
    if sys.byteorder != 'little':
        source.byteswap()
    result = check_writes(parse_row(text), (directory/FILE).read_bytes(), source)
    result['source_sha256'] = capture['streams']['dma_source']['sha256']
    result['serial_callbacks'] = capture['serial_callbacks']
    if buffer_sequence:
        controls, stop = parse_controls(text)
        tx = array('H')
        tx.frombytes((directory/'mu4_playback_tx.le16').read_bytes())
        times = array('Q')
        times.frombytes((directory/'mu4_playback_tx.times').read_bytes())
        if sys.byteorder != 'little':
            tx.byteswap()
            times.byteswap()
        writes = list(struct.iter_unpack('<QIHBB',(directory/FILE).read_bytes()))
        result['buffer_sequence'] = check_buffer_sequence(controls,stop,writes,tx,times)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--check-buffer-sequence', action='store_true',help='require full passive SPCR2 observation and reset-aware sequence accounting')
    args = parser.parse_args()
    try:
        result = verify(args.directory, buffer_sequence=args.check_buffer_sequence)
    except (OSError, ValueError) as error:
        print(f'MU4 DXR ownership: {error}', file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

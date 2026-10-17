"""Check bounded raw-word copy chronology, not arithmetic ownership or audio."""
import argparse
import json
from pathlib import Path
import sys

if __package__:
    from .mu4_playback_frame_check import rows
else:
    from mu4_playback_frame_check import rows


TRANSFERS = ((0x787e, 0x7800, 0xfff59b08), (0x7800, 0xf300, 0xfff59b08),
             (0xf300, 0x7b80, 0xfff59b08), (0xe87e, 0xe800, 0xffbd0b70),
             (0xe800, 0xf300, 0xffbd0b70), (0xf300, 0xeb80, 0xffbd0b70),
             (0x7b80, 0xf380, 0xfff59b08))


def verify(text):
    events = []
    for line in text.splitlines():
        for kind in ('reader', 'writer'):
            marker = 'mu4_raw_' + kind
            if marker + ':' not in line:
                continue
            row = rows(line, marker)[0]
            if row.get('ownership') != 'data_bus' or int(row['mask'], 16) != 0xffff:
                raise ValueError('raw observation ownership or bus mask changed')
            address, value, time = int(row['address'], 16), int(row['value'], 16), float(row['t'])
            if not 0 <= address <= 0xffff or not 0 <= value <= 0xffff or not 1.0168 <= time <= 1.023:
                raise ValueError('raw observation outside bounded word/time domain')
            events.append((kind, address, value, time))
    terminal = rows(text, 'mu4_raw_terminal')
    if len(terminal) != 1 or terminal[0].get('closed') != '1' or terminal[0].get('capped') != '0':
        raise ValueError('raw observation incomplete or capped')
    if int(terminal[0]['records']) != len(events) or not events:
        raise ValueError('raw observation record count mismatch')
    if any(a[3] > b[3] for a, b in zip(events, events[1:])):
        raise ValueError('raw bus events are not chronological')
    proofs, previous = [], -1
    for source, destination, value in TRANSFERS:
        words = (value >> 16, value & 0xffff)
        found = None
        for index in range(previous + 1, len(events)):
            kind, address, word, time = events[index]
            if (kind, address, word) != ('reader', source, words[0]):
                continue
            # Require material published before each read, not just a later match.
            history = {a: v for k, a, v, t in events[:index] if k == 'writer'}
            if any(history.get(source + i) != w for i, w in enumerate(words)):
                continue
            selected = [e for e in events[index:] if e[3] - time <= 0.000001
                        and ((e[0] == 'reader' and source <= e[1] <= source + 1)
                             or (e[0] == 'writer' and destination <= e[1] <= destination + 1))]
            expected = [('reader', source, words[0]), ('reader', source + 1, words[1]),
                        ('writer', destination, words[0]), ('writer', destination + 1, words[1])]
            # Two MVDDs interleave read/write; DLD/DST reads both words first.
            single_word_order = [expected[i] for i in (0, 2, 1, 3)]
            if [e[:3] for e in selected[:4]] in (expected, single_word_order):
                found = index
                proofs.append({'source': f'{source:04x}', 'destination': f'{destination:04x}',
                               'words': f'{value:08x}', 'read_time': time})
                break
        if found is None:
            raise ValueError(f'raw transfer {source:04x}->{destination:04x} lacks ordered material/read/write evidence')
        previous = found
    return {'transfers': proofs, 'raw_copy_chronology_validated': True,
            'arithmetic_ownership_validated': False, 'native_audio_validated': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(verify(args.log.read_text()), indent=2))
    except (ValueError, KeyError, OSError) as error:
        print(f'FAIL: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())

"""Compare original predictor arguments/history without changing decoder state."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.c54x_rom4_efr_excitation_check import native_records as excitation_native
from tools.c54x_rom4_efr_excitation_check import reference_records as excitation_reference

CALL = re.compile(r'efr_predictor_call: return=([0-9a-f]{4}) output=([0-9a-f]{4}) coefficients=([0-9a-f]{4}) lag=([0-9a-f]{4}) fraction=([0-9a-f]{4}) saved_lag=([0-9a-f]{4}) saved_fraction=([0-9a-f]{4})')
HISTORY = re.compile(r'efr_predictor: kind=HISTORY values=([0-9a-f]{616})')
REF_CALL = re.compile(r'EFRPREDINPUT frame=(\d+) subframe=(\d+) lag=([0-9a-f]{4}) fraction=([0-9a-f]{4}) length=40')
REF_HISTORY = re.compile(r'EFRPREDHISTORY frame=(\d+) subframe=(\d+) values=([0-9a-f]{616})')


def words(text):
    return tuple(int(text[i:i + 4], 16) for i in range(0, len(text), 4))


def valid_arguments(lag, fraction):
    if not 17 <= lag <= 143 or fraction not in (0, 1, 2, 3, 0xfffe, 0xffff):
        raise ValueError('predictor lag/fraction outside the decoded contract')


def reference_records(text, frames=518):
    controls, _ = excitation_reference(text, frames)
    expected = [key for key, value in controls.items() if value & 1]
    lines = [line for line in text.splitlines() if line.startswith(('EFRPREDINPUT ', 'EFRPREDHISTORY '))]
    if len(lines) != len(expected) * 2:
        raise ValueError('incomplete reference predictor inputs')
    records = {}
    for index, key in enumerate(expected):
        call, history = REF_CALL.fullmatch(lines[index * 2]), REF_HISTORY.fullmatch(lines[index * 2 + 1])
        if not call or not history or tuple(map(int, call.groups()[:2])) != key or tuple(map(int, history.groups()[:2])) != key:
            raise ValueError('reference predictor input order/width differs')
        lag, fraction = int(call[3], 16), int(call[4], 16)
        valid_arguments(lag, fraction)
        records[key] = (lag, fraction, words(history[3]))
    return records


def native_records(text, frames=518):
    controls, _ = excitation_native(text, frames)
    groups = []
    for line in text.splitlines():
        if line == 'efr_boundary: kind=entry':
            groups.append([])
        elif line.startswith(('efr_predictor_call:', 'efr_predictor:')):
            if not groups:
                raise ValueError('unowned predictor observation')
            groups[-1].append(line)
    if len(groups) != frames * 2:
        raise ValueError('incomplete native predictor frames')
    records = {}
    for frame in range(frames):
        group = groups[frame * 2]
        if group != groups[frame * 2 + 1]:
            raise ValueError('native predictor replay differs')
        subs = [sub for sub in range(4) if controls[frame, sub] & 1]
        if len(group) != len(subs) * 2:
            raise ValueError('native predictor calls disagree with speech admission')
        for index, sub in enumerate(subs):
            call, history = CALL.fullmatch(group[index * 2]), HISTORY.fullmatch(group[index * 2 + 1])
            if not call or not history:
                raise ValueError('malformed native predictor input')
            ret, output, coefficients, lag, fraction, saved_lag, saved_fraction = (int(x, 16) for x in call.groups())
            if (ret != 0xc7fa or output != 0x1514 + sub * 40 or coefficients != 0xeaaa or
                    (lag, fraction) != (saved_lag, saved_fraction)):
                raise ValueError('native predictor argument ownership differs')
            valid_arguments(lag, fraction)
            records[frame, sub] = (lag, fraction, words(history[1]))
    return records


def compare(native, reference, *, frames=518):
    actual, expected = native_records(native, frames), reference_records(reference, frames)
    if actual.keys() != expected.keys():
        raise ValueError('native/reference predictor admission differs')
    differences = []
    for key, (lag, fraction, history) in expected.items():
        if actual[key][:2] != (lag, fraction):
            raise ValueError('native/reference lag or fraction differs')
        count = sum(a != b for a, b in zip(actual[key][2], history))
        if count:
            differences.append(dict(frame=key[0], subframe=key[1], words=count))
    return dict(status='characterized', frames=frames, calls=len(expected),
                matched_lag_fraction_pairs=len(expected), history_words=len(expected) * 154,
                differing_history_words=sum(row['words'] for row in differences),
                first=differences[0] if differences else None,
                last=differences[-1] if differences else None, differences=differences,
                conformance_claim=False, handset_audio_claim=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('native', type=Path)
    parser.add_argument('reference', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        native, reference = args.native.read_bytes(), args.reference.read_bytes()
        report = compare(native.decode('utf-8-sig'), reference.decode())
        report['input_sha256'] = dict(native=hashlib.sha256(native).hexdigest(),
                                      reference=hashlib.sha256(reference).hexdigest())
        args.output.write_text(json.dumps(report, indent=2) + '\n')
    except (OSError, ValueError) as error:
        parser.exit(1, f'EFR predictor input FAIL: {error}\n')
    print(json.dumps({key: value for key, value in report.items() if key != 'differences'}, indent=2))


if __name__ == '__main__':
    main()

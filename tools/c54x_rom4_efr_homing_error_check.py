"""Characterize acquired homing/error policy without claiming conformance."""
import hashlib
import re
import struct

FRAMES = 49
BAD = (0, 23, 46)
RESETS = (1, 2, 24, 25, 47, 48)
UNSETTLED = (0, 1, 23, 24, 46, 47)
ROW = re.compile(r'ROM4 EFR homing error frame: frame=(\d+) mismatches=(\d+) resets=(\d+) reference_settled=([01]) pcm=([0-9a-f]{640})')
REPLAY = 'TMS320C54x ROM4 EFR homing error replay: COMPLETE frames=49 ram_words=65440 checkpoints=frame_input handset_audio_claim=0'
OBSERVED = 'TMS320C54x ROM4 EFR homing error observation: COMPLETE frames=49 samples=7840 conformance_claim=0 handset_audio_claim=0'
COMPLETE = 'Fatal error: TMS320C54x ROM4 EFR homing error complete'


def check(text, returncode, reference, strict=False, staged=False):
    if re.search(r'^Driver .+: [1-9]\d* errors?,', text, re.MULTILINE):
        raise ValueError('MAME driver validity errors invalidate homing/error characterization')
    if staged and text.splitlines().count('ROM4 EFR homing decoder frontend: staged handset_audio_claim=0') != 1:
        raise ValueError('missing unique staged homing/error frontend selection')
    if len(reference) != FRAMES * 320:
        raise ValueError('incomplete independent homing/error PCM')
    lines = text.splitlines()
    if lines and re.fullmatch(r'Average speed: \d+(?:\.\d+)?% \(\d+ seconds\)', lines[-1]):
        lines.pop()
    if returncode != 3 or not lines or lines[-1] != COMPLETE:
        raise ValueError('missing terminal native homing/error completion')
    if any(token in text for token in ('WRONG CHECKSUMS', 'NOT FOUND', 'Errors:')):
        raise ValueError('invalid native homing/error inputs or machine')
    if any(line.startswith('Fatal error:') and line != COMPLETE for line in lines):
        raise ValueError('unexpected homing/error fatal error')
    for marker in (REPLAY, OBSERVED, COMPLETE):
        if lines.count(marker) != 1:
            raise ValueError('missing or duplicate homing/error scope or replay marker')
    rows = [line for line in lines if line.startswith('ROM4 EFR homing error frame:')]
    if len(rows) != FRAMES:
        raise ValueError('incomplete homing/error frame coverage')
    records = []
    native = bytearray()
    for frame, row in enumerate(rows):
        match = ROW.fullmatch(row)
        if not match or int(match[1]) != frame:
            raise ValueError('malformed or unordered homing/error frames')
        mismatches, resets, settled = map(int, match.groups()[1:4])
        actual = [int(match[5][i:i + 4], 16) for i in range(0, 640, 4)]
        native.extend(struct.pack('<160H', *actual))
        expected = struct.unpack_from('<160H', reference, frame * 320)
        if mismatches != sum(a != b for a, b in zip(actual, expected)):
            raise ValueError('reported homing/error PCM differences disagree with capture')
        if resets > 1 or settled != int(frame not in UNSETTLED):
            raise ValueError('invalid homing/error reset count or settled domain')
        records.append(dict(frame=frame, mismatches=mismatches, reset_entries=resets,
                            reference_settled=bool(settled), ehf=all(word == 8 for word in actual)))
    total = sum(row['mismatches'] for row in records)
    aggregate = f'ROM4 EFR homing error total: mismatches={total}'
    if lines[-4:] != [REPLAY, OBSERVED, aggregate, COMPLETE]:
        raise ValueError('inconsistent homing/error aggregate or completion ordering')
    reset_differences = [row['frame'] for row in records if row['reset_entries'] != int(row['frame'] in RESETS)]
    settled_differences = [row['frame'] for row in records if row['reference_settled'] and row['mismatches']]
    if any(records[frame]['reset_entries'] for frame in BAD):
        raise ValueError('bad homing frame unexpectedly resets the decoder')
    recovery = []
    for frame in (1, 24, 47):
        recovery.extend([
            f'ROM4 homing recovery parameter: frame={frame} pc=c8dc actual=000b table=000b',
            f'ROM4 homing recovery store: frame={frame} a=0000000000 b=0000000000 parameter=000b',
            f'ROM4 homing recovery parameter: frame={frame} pc=c930 actual=0000 table=000b',
        ])
    if [line for line in lines if line.startswith('ROM4 homing recovery ')] != recovery:
        raise ValueError('missing or changed original recovery parameter observations')
    if strict and (total or reset_differences):
        raise ValueError(f'strict homing/error reference differs: samples={total} reset_frames={reset_differences}')
    return dict(schema=1, status='characterized', frames=FRAMES, frontend='staged' if staged else 'atomic',
                native_pcm_sha256=hashlib.sha256(native).hexdigest(),
                total_mismatches=total, full_reference_equal=not total,
                reference_reset_equal=not reset_differences,
                reset_mismatch_frames=reset_differences, reference_settled_mismatch_frames=settled_differences,
                recovery_parameter_frames=[1, 24, 47],
                strict_reference_required=strict, conformance_claim=False,
                handset_audio_claim=False, records=records)

"""Characterize native EFR losses without treating differences as conformance."""
import hashlib
import math
import re
import struct


FRAMES = 163
SAMPLES = 160
ROW = re.compile(r'ROM4 EFR loss observation: frame=(\d+) mismatches=(\d+) pcm=([0-9a-f]{640})')
REPLAY = 'TMS320C54x ROM4 EFR loss observation replay: COMPLETE frames=163 ram_words=65440 checkpoints=frame_input'
SUMMARY = re.compile(r'TMS320C54x ROM4 EFR loss observation: COMPLETE frames=163 samples=26080 erased=28 bursts=1,3,8,16 mismatches=(\d+) conformance_claim=0 handset_audio_claim=0')
COMPLETE = 'Fatal error: TMS320C54x ROM4 EFR loss observation complete'


def lost(frame):
    return frame == 8 or 32 <= frame < 35 or 80 <= frame < 88 or 120 <= frame < 136


def characterize(text, returncode, reference, serial, verbose_teardown=False):
    if re.search(r'^Driver .+: [1-9]\d* errors?,', text, re.MULTILINE):
        raise ValueError('MAME driver validity errors invalidate codec characterization')
    if len(reference) != FRAMES * SAMPLES * 2 or len(serial) != FRAMES * 494:
        raise ValueError('incomplete EFR reference or serial input')
    lines = text.splitlines()
    if verbose_teardown and lines[-2:] == ['Enter sdlwindow_exit', 'Leave sdlwindow_exit']:
        lines = lines[:-2]
    if lines and re.fullmatch(r'Average speed: \d+(?:\.\d+)?% \(\d+ seconds\)', lines[-1]):
        lines.pop()
    if returncode != 3 or not lines or lines[-1] != COMPLETE:
        raise ValueError('missing terminal EFR observation completion')
    if any(line.startswith('Fatal error:') and line != COMPLETE for line in lines):
        raise ValueError('unexpected EFR observation fatal error')
    if any(token in text for token in ('WRONG CHECKSUMS', 'NOT FOUND', 'Errors:')):
        raise ValueError('invalid EFR observation inputs or machine')
    if len(lines) < 3 or lines.count(REPLAY) != 1 or lines.count(COMPLETE) != 1 or lines[-3] != REPLAY:
        raise ValueError('missing unique complete EFR frame/history replay')
    rows = [line for line in lines if line.startswith('ROM4 EFR loss observation:')]
    if len(rows) != FRAMES:
        raise ValueError('missing or duplicate EFR observation frame')
    records, pcm = [], bytearray()
    for frame, line in enumerate(rows):
        match = ROW.fullmatch(line)
        if not match or int(match[1]) != frame:
            raise ValueError('malformed or unordered EFR observation frame')
        words = struct.unpack_from('<247H', serial, frame * 494)
        if words[0] != int(lost(frame)) or words[245] != 0 or words[246] not in (0, 1):
            raise ValueError('EFR input differs from the fixed active-speech loss schedule')
        if any(bit not in (0, 1) for bit in words[1:245]):
            raise ValueError('nonbinary EFR serial speech input')
        raw = [int(match[3][i:i + 4], 16) for i in range(0, 640, 4)]
        actual = [word if word < 32768 else word - 65536 for word in raw]
        expected = struct.unpack_from('<160h', reference, frame * 320)
        differences = [sample - other for sample, other in zip(actual, expected)]
        mismatches = sum(delta != 0 for delta in differences)
        if mismatches != int(match[2]):
            raise ValueError('reported EFR difference count disagrees with captured PCM')
        pcm.extend(struct.pack('<160h', *actual))
        records.append(dict(frame=frame, erased=lost(frame), mismatches=mismatches,
                            max_abs=max(map(abs, differences)),
                            sum_squared=sum(delta * delta for delta in differences),
                            nonzero_samples=sum(sample != 0 for sample in actual)))
    summaries = [line for line in lines if line.startswith('TMS320C54x ROM4 EFR loss observation:')]
    summary = SUMMARY.fullmatch(summaries[0]) if len(summaries) == 1 else None
    mismatches = sum(row['mismatches'] for row in records)
    if not summary or lines[-2] != summaries[0] or int(summary[1]) != mismatches:
        raise ValueError('missing or inconsistent EFR observation aggregate')
    changed = [row['frame'] for row in records if row['mismatches']]
    report = dict(schema=1, status='characterized', frames=FRAMES, samples=FRAMES * SAMPLES,
                  mismatches=mismatches, differing_frames=len(changed),
                  first_differing_frame=changed[0] if changed else None,
                  max_abs=max(row['max_abs'] for row in records),
                  rms=math.sqrt(sum(row['sum_squared'] for row in records) / (FRAMES * SAMPLES)),
                  native_pcm_sha256=hashlib.sha256(pcm).hexdigest(),
                  reference_pcm_sha256=hashlib.sha256(reference).hexdigest(),
                  serial_sha256=hashlib.sha256(serial).hexdigest(),
                  reference_equal=not mismatches, conformance_claim=False,
                  handset_audio_claim=False, records=records)
    return report, bytes(pcm)

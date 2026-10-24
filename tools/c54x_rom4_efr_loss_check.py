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
LOST_SID_ROW = re.compile(r'ROM4 EFR lost SID observation: frame=(\d+) mismatches=(\d+) pcm=([0-9a-f]{640})')
LOST_SID_REPLAY = 'TMS320C54x ROM4 EFR lost SID observation replay: COMPLETE frames=518 ram_words=65440 checkpoints=frame_input'
LOST_SID_SUMMARY = re.compile(r'TMS320C54x ROM4 EFR lost SID observation: COMPLETE frames=518 samples=82880 lost_sid=2 mismatches=(\d+) conformance_claim=0 handset_audio_claim=0')
LOST_SID_COMPLETE = 'Fatal error: TMS320C54x ROM4 EFR lost SID observation complete'


def lost(frame):
    return frame == 8 or 32 <= frame < 35 or 80 <= frame < 88 or 120 <= frame < 136


def characterize(text, returncode, reference, serial, verbose_teardown=False, lost_sid=False, staged=False):
    frames = 518 if lost_sid else FRAMES
    row_pattern = LOST_SID_ROW if lost_sid else ROW
    row_prefix = 'ROM4 EFR lost SID observation:' if lost_sid else 'ROM4 EFR loss observation:'
    summary_prefix = 'TMS320C54x ROM4 EFR lost SID observation:' if lost_sid else 'TMS320C54x ROM4 EFR loss observation:'
    replay = LOST_SID_REPLAY if lost_sid else REPLAY
    summary_pattern = LOST_SID_SUMMARY if lost_sid else SUMMARY
    complete = LOST_SID_COMPLETE if lost_sid else COMPLETE
    if re.search(r'^Driver .+: [1-9]\d* errors?,', text, re.MULTILINE):
        raise ValueError('MAME driver validity errors invalidate codec characterization')
    if len(reference) != frames * SAMPLES * 2 or len(serial) != frames * 494:
        raise ValueError('incomplete EFR reference or serial input')
    lines = text.splitlines()
    if verbose_teardown and lines[-2:] == ['Enter sdlwindow_exit', 'Leave sdlwindow_exit']:
        lines = lines[:-2]
    if lines and re.fullmatch(r'Average speed: \d+(?:\.\d+)?% \(\d+ seconds\)', lines[-1]):
        lines.pop()
    if returncode != 3 or not lines or lines[-1] != complete:
        raise ValueError('missing terminal EFR observation completion')
    if any(line.startswith('Fatal error:') and line != complete for line in lines):
        raise ValueError('unexpected EFR observation fatal error')
    if any(token in text for token in ('WRONG CHECKSUMS', 'NOT FOUND', 'Errors:')):
        raise ValueError('invalid EFR observation inputs or machine')
    if len(lines) < 3 or lines.count(replay) != 1 or lines.count(complete) != 1 or lines[-3] != replay:
        raise ValueError('missing unique complete EFR frame/history replay')
    rows = [line for line in lines if line.startswith(row_prefix)]
    if len(rows) != frames:
        raise ValueError('missing or duplicate EFR observation frame')
    records, pcm = [], bytearray()
    for frame, line in enumerate(rows):
        match = row_pattern.fullmatch(line)
        if not match or int(match[1]) != frame:
            raise ValueError('malformed or unordered EFR observation frame')
        words = struct.unpack_from('<247H', serial, frame * 494)
        if (not lost_sid and (words[0] != int(lost(frame)) or words[245] != 0)) or words[0] not in (0, 1) or words[245] not in (0, 2) or words[246] not in (0, 1):
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
        records.append(dict(frame=frame, erased=bool(words[0]), mismatches=mismatches,
                            max_abs=max(map(abs, differences)),
                            sum_squared=sum(delta * delta for delta in differences),
                            nonzero_samples=sum(sample != 0 for sample in actual)))
    summaries = [line for line in lines if line.startswith(summary_prefix)]
    summary = summary_pattern.fullmatch(summaries[0]) if len(summaries) == 1 else None
    mismatches = sum(row['mismatches'] for row in records)
    if not summary or lines[-2] != summaries[0] or int(summary[1]) != mismatches:
        raise ValueError('missing or inconsistent EFR observation aggregate')
    if lost_sid:
        serial_frames = list(struct.iter_unpack('<247H', serial))
        if sum(row[0] for row in serial_frames) != 137 or sum(row[245] == 2 for row in serial_frames) != 6:
            raise ValueError('lost-SID input schedule differs')
        if any((serial_frames[i][0], serial_frames[i][245], serial_frames[i][246]) != (1, 0, 1) for i in (71, 95)):
            raise ValueError('missing scheduled lost SID updates')
        flags = re.findall(r'^ROM4 EFR lost SID flags: frame=(\d+) native=([0-9a-f]{4}) reference=([0-9a-f]{4})$', text, re.M)
        if [(int(i), int(a, 16), int(b, 16)) for i, a, b in flags] != [(95, 0x290, 0x290), (96, 0x2c0, 0x2c0), (97, 0x401, 0x501), (356, 0x401, 0x401)]:
            raise ValueError('original lost-SID flag-policy observations differ')
        frontend = 'ROM4 EFR lost SID decoder frontend: staged handset_audio_claim=0'
        if lines.count(frontend) != int(staged):
            raise ValueError('lost-SID frontend selection differs')
    changed = [row['frame'] for row in records if row['mismatches']]
    report = dict(schema=1, status='characterized', frames=frames, samples=frames * SAMPLES,
                  mismatches=mismatches, differing_frames=len(changed),
                  first_differing_frame=changed[0] if changed else None,
                  max_abs=max(row['max_abs'] for row in records),
                  rms=math.sqrt(sum(row['sum_squared'] for row in records) / (frames * SAMPLES)),
                  native_pcm_sha256=hashlib.sha256(pcm).hexdigest(),
                  reference_pcm_sha256=hashlib.sha256(reference).hexdigest(),
                  serial_sha256=hashlib.sha256(serial).hexdigest(),
                  reference_equal=not mismatches, conformance_claim=False,
                  handset_audio_claim=False, records=records)
    return report, bytes(pcm)

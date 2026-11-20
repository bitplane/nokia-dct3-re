"""Degraded traffic-channel frames on the native lab call, and recovery.

The lab BTS gives the call's speech/FACCH bursts 1/3 bit errors both ways for
208 frames (npe3_tch_impairment) and leaves SACCH intact. The evidence is the
two receivers' own judgement, not a lab flag:

- downlink: the handset's SACCH Measurement Reports (RR 0x15) rise to a high
  RXQUAL-FULL for the impaired periods and fall back afterwards, so the native
  DSP's burst decoder saw the errors and recovered;
- uplink: the lab's GSM-FR channel decoder counts bad speech frames inside
  the window, few after it, while good speech resumes.
"""
import re

IMPAIRMENT = re.compile(r'npe3_tch_impairment: first_frame=(\d+) last_frame=(\d+) bit_errors=1/3 '
                        r'directions=downlink,uplink sacch=intact physical_claim=0')
SACCH = re.compile(r'npe3_tch_uplink: channel=sacch start_frame=(\d+) octets=([0-9a-f]+) exact_code=1 '
                   r'speech_blocks=(\d+) bad_speech_blocks=(\d+) ')
PERIOD = 104


def measurement_reports(text):
    """(start_frame, rxqual_full, speech_blocks, bad_speech_blocks) per uplink Measurement Report."""
    reports = []
    for match in SACCH.finditer(text):
        layer3 = bytes.fromhex(match[2])[5:]
        if len(layer3) >= 5 and layer3[0] & 0x0f == 6 and layer3[1] == 0x15:
            reports.append((int(match[1]), (layer3[4] >> 4) & 7, int(match[3]), int(match[4])))
    return reports


def check(text):
    windows = IMPAIRMENT.findall(text)
    if len(windows) != 1:
        raise ValueError('missing unique traffic-channel impairment window')
    first, last = map(int, windows[0])
    reports = measurement_reports(text)
    before = [r for r in reports if r[0] <= first]
    during = [r for r in reports if first < r[0] <= last + PERIOD]
    after = [r for r in reports if r[0] > last + PERIOD]
    if not before or len(during) < 2 or len(after) < 3:
        raise ValueError('Measurement Reports do not span the impairment window')
    worst = max(r[1] for r in during)
    if worst < 6:
        raise ValueError(f'handset RXQUAL peaked at {worst} in the impaired periods')
    if max(r[1] for r in after) > 4:
        raise ValueError('handset RXQUAL did not recover after the impairment')
    bad_inside = during[-1][3] - before[-1][3]
    bad_after = after[-1][3] - during[-1][3]
    speech_after = after[-1][2] - during[-1][2]
    if bad_inside < 30:
        raise ValueError(f'lab decoder saw only {bad_inside} bad uplink frames in the window')
    if bad_after > 5 or speech_after < 50:
        raise ValueError('uplink speech did not recover after the impairment')
    return dict(impaired_frames=last - first, peak_rxqual_full=worst,
                recovered_rxqual_full=max(r[1] for r in after),
                bad_uplink_frames_in_window=bad_inside, good_uplink_frames_after=speech_after,
                degraded_frames_qualified=True, physical_channel_claim=False)

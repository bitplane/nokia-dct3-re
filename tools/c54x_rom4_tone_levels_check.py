"""Check physical keypad-tone settings at the native DSP control boundary."""

import re
import hashlib

try:
    from .c54x_rom4_codec_contract import check_trace
except ImportError:
    from c54x_rom4_codec_contract import check_trace


DIGIT_TWO_PIXELS = 'cf44d8d2cf6398d7d69aa5cb9b766d3b8a2905169c6edda6e313b4fc2d6b8cd6'


def check_frame(path):
    from PIL import Image
    with Image.open(path) as image:
        if image.size != (84, 48) or hashlib.sha256(image.convert('L').tobytes()).hexdigest() != DIGIT_TWO_PIXELS:
            raise ValueError('missing final digit-2 frame; input acceptance is not proved')


def check_level(text, option):
    if option not in (1, 2, 3, 4):
        raise ValueError('unknown keypad-tone setting')
    check_trace(text)
    if re.search(r'Lua error|rom4_reset_request|TMS320C54x.*illegal', text):
        raise ValueError('native tone-level fixture failed')
    presses = re.findall(r'input-press: t=([0-9.]+) name=(\w+)\b', text)
    releases = re.findall(r'input-release: t=([0-9.]+) name=(\w+)\b', text)
    keys = ['menu', '9', '5', str(option), 'clear', 'clear', 'clear', '2']
    if [key for _, key in presses] != keys or [key for _, key in releases] != keys:
        raise ValueError('wrong physical tone-level key sequence')
    previous = -1.0
    for (press, _), (release, _) in zip(presses, releases):
        if not previous < float(press) < float(release):
            raise ValueError('unordered physical key edges')
        previous = float(release)
    start, end = float(presses[-1][0]), float(releases[-1][0])
    calls = re.findall(
        r'rom4_codec_control_call: hit=(\d+) a=([0-9a-f]{10}) b=([0-9a-f]{10}) '
        r'sp=[0-9a-f]{4} return=([0-9a-f]{4}) t=([0-9.]+)', text)
    summaries = re.findall(r'rom4_codec_control_summary: calls=(\d+) t=([0-9.]+)', text)
    if (len(summaries) != 1 or not calls or
            [int(row[0]) for row in calls] != list(range(1, int(summaries[0][0]) + 1))):
        raise ValueError('truncated or duplicated native control observations')
    stop = float(summaries[0][1])
    if stop < end + 1:
        raise ValueError('insufficient post-key observation')
    copies = re.findall(
        r'rom4_tone_access: owner=dsp direction=write address=0000fe '
        r'value=([0-9a-f]+) mask=ffff pc=([0-9a-f]{6}) t=([0-9.]+)', text)
    totals = re.findall(
        r'rom4_tone_summary: tx_words=(\d+) rx_reads=(\d+) tone_reads=\d+ tone_copies=(\d+)', text)
    if len(totals) != 1 or tuple(map(int, totals[0][:2])) != (2, 2):
        raise ValueError('unexpected serial sample traffic; this is not an audio gate')
    if int(totals[0][2]) != len(copies):
        raise ValueError('truncated tone-copy observations')
    # Earlier enabled menu keys are a positive control for Off's absence test.
    if not any(int(value, 16) == 0xe1 and pc == '00a5de' and 8 <= float(time) < start
               for value, pc, time in copies):
        raise ValueError('missing pre-selection tone positive control')
    active = [float(time) for _, a, b, ret, time in calls
              if a == '0000000626' and b == '0000000008' and ret == 'a4bb'
              and start <= float(time) < stop]
    tone = [float(time) for value, pc, time in copies
            if int(value, 16) == 0xe1 and start <= float(time) < stop]
    if option == 1:
        if active or tone:
            raise ValueError('Off still emits tone/activation after the final key')
        return
    if not any(start <= time < end for time in tone) or not active:
        raise ValueError('enabled setting lacks organic tone/activation')
    if option in (3, 4):
        word = '920c' if option == 3 else '9209'
        updates = re.findall(
            r'rom4_tone_access: owner=codec_shared_mcu direction=write address=0100a8 '
            rf'value={word}0000 mask=ffff0000 pc=272034 t=([0-9.]+)', text)
        if not any(start <= float(time) < min(tone) for time in updates):
            raise ValueError('missing ordered level-dependent packed control')

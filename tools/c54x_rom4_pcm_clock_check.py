"""Check externally clocked native NSE-1 tone words, not product audio wiring."""
import math
import re


WORDS = re.compile(r'native_pcm_fixture_word: value=([0-9a-f]{4}) t=([0-9.]+)')
SUMMARY = re.compile(r'rom4_tone_summary: tx_words=(\d+) rx_reads=(\d+) tone_reads=(\d+) tone_copies=(\d+) t=([0-9.]+)')


def control_timeline(text):
    """Recover complete helper-entry observations, not physical bit meanings."""
    calls = re.findall(r'rom4_codec_control_call: hit=(\d+) a=([0-9a-f]{10}) b=([0-9a-f]{10}) sp=[0-9a-f]{4} return=[0-9a-f]{4} t=([0-9.]+)', text)
    totals = re.findall(r'rom4_codec_control_summary: calls=(\d+) t=([0-9.]+)', text)
    if len(totals) != 1 or len(calls) != int(totals[0][0]) or not calls:
        raise ValueError('missing or truncated codec helper timeline')
    if [int(hit) for hit, *_ in calls] != list(range(1, len(calls) + 1)):
        raise ValueError('duplicate or unordered codec helper observations')
    times = [float(call[3]) for call in calls]
    if times != sorted(times) or float(totals[0][1]) < max(9.0, times[-1]):
        raise ValueError('codec helper timeline does not cover waveform intervals')
    register8 = [(float(time), int(value, 16) & 0xfff)
                 for _, value, address, time in calls if int(address, 16) == 8]
    result = {}
    for name, begin, end in (('before', 7.5, 7.9), ('active', 8.13, 8.19), ('after', 8.6, 9.0)):
        preceding = [value for time, value in register8 if time <= begin]
        if not preceding:
            raise ValueError('codec register 8 lacks an observed initial value')
        values = {preceding[-1], *(value for time, value in register8 if begin < time < end)}
        result[name] = sorted(values)
    return dict(helper_calls=len(calls), register8_values=result,
                physical_clock_enable_claim=False)


def tone_fraction(samples, frequency=900, rate=8000):
    energy = sum(value * value for value in samples)
    if not energy:
        return 0.0
    real = sum(value * math.cos(2 * math.pi * frequency * n / rate) for n, value in enumerate(samples))
    imaginary = sum(value * math.sin(2 * math.pi * frequency * n / rate) for n, value in enumerate(samples))
    return 2 * (real * real + imaginary * imaginary) / (len(samples) * energy)


def check(text, *, clock):
    summaries = SUMMARY.findall(text)
    if len(summaries) != 1:
        raise ValueError('missing or duplicate native tone summary')
    tx, rx, tone_reads, copies = map(int, summaries[0][:4])
    if not re.search(r'input-press: t=8\.000000 name=2\b', text) or not re.search(r'input-release: t=8\.220000 name=2\b', text):
        raise ValueError('missing physical key-2 press/release')
    if not tone_reads or not copies:
        raise ValueError('native shared-tone consumption absent')
    controls = re.findall(r'rom4_tone_access: owner=(mcu|mcu_frequency) direction=write address=(0100ac|0100b0) value=([0-9a-f]+) mask=([0-9a-f]+) pc=272034 t=([0-9.]+)', text)
    def publication(owner, address, value, mask):
        return [float(time) for who, where, word, lane, time in controls
                if (who, where, int(word, 16), int(lane, 16)) == (owner, address, value, mask) and 8 <= float(time) < 8.22]
    commands = publication('mcu', '0100ac', 0xe10000, 0xffff0000)
    high = publication('mcu', '0100ac', 0x14e0, 0xffff)
    low = publication('mcu_frequency', '0100b0', 0x0ae40000, 0xffff0000)
    if len(commands) != 1 or len(high) != 1 or len(low) != 1 or not commands[0] < high[0] < low[0]:
        raise ValueError('missing ordered native DTMF command and 1336/697 Hz inputs')
    words = [(float(time), int(value, 16) - (65536 if int(value, 16) & 0x8000 else 0))
             for value, time in WORDS.findall(text)]
    if not clock:
        if words or (tx, rx) != (2, 2):
            raise ValueError('unclocked control differs from two boot transfers')
        return dict(clock=False, tx_words=tx, rx_reads=rx, handset_clock_claim=False)
    if tx < 50000 or rx < 50000 or len(words) < 50000:
        raise ValueError('external frames do not sustain native serial traffic')
    for (first, _), (second, _) in zip(words, words[1:]):
        ticks = (second - first) * 8000
        if ticks <= 0 or abs(ticks - round(ticks)) > 0.00002:
            raise ValueError('native output is not on the supplied 8 kHz frame grid')

    def interval(begin, end):
        selected = [(time, value) for time, value in words if begin <= time < end]
        if len(selected) != round((end - begin) * 8000):
            raise ValueError('missing or duplicate output frames in analysis interval')
        if any(abs(b[0] - a[0] - 1 / 8000) > 2e-9 for a, b in zip(selected, selected[1:])):
            raise ValueError('analysis interval has a serial discontinuity')
        return [value for _, value in selected]

    before, active, after = interval(7.5, 7.9), interval(8.13, 8.19), interval(8.6, 9.0)
    if any(before) or any(after):
        raise ValueError('output is not silent outside the key-tone interval')
    fractions = [tone_fraction(active, frequency) for frequency in (697, 1336)]
    # Finite-window projections are not orthogonal; tolerate phase-dependent
    # leakage and the native unequal component levels. This is no Q.23 mask.
    if sum(fractions) < 0.95 or min(fractions) < 0.15:
        raise ValueError('native output does not reproduce both key-2 DTMF frequencies')
    return dict(clock=True, tx_words=tx, rx_reads=rx, captured_words=len(words),
                active_samples=len(active), frequencies=[697, 1336], sinusoidal_energy_fractions=fractions,
                control_timeline=control_timeline(text),
                handset_clock_claim=False, native_call_speech_claim=False)


def check_restore(text):
    marker = re.compile(r'native_pcm_restore: phase=(saved|restored|complete) ([^\n]+)')
    markers = list(marker.finditer(text))
    if [match[1] for match in markers] != ['saved', 'restored', 'complete']:
        raise ValueError('missing, duplicate or unordered native PCM restore boundaries')
    saved, restored, complete = markers
    expected = {'t': r'\d+\.\d{9}', 'mcu_PC': r'[0-9a-f]{8}', 'mcu_CPSR': r'[0-9a-f]{8}'}
    for name in ('PC', 'SP', 'ST0', 'ST1', 'A', 'B', 'T',
                 *(f'AR{i}' for i in range(8)), 'IFR', 'IMR'):
        expected['dsp_' + name] = r'[0-9a-f]{10}'
    expected.update({f'mcu_R{i}': r'[0-9a-f]{8}' for i in range(16)})
    expected.update({f'data_{address:04x}': r'[0-9a-f]{4}' for address in (0x21, 0x22, 0x23, *range(0xf0, 0xff))})
    for match in (saved, restored):
        parts = match[2].split()
        fields = dict(part.split('=', 1) for part in parts if '=' in part)
        if len(parts) != len(expected) or set(fields) != set(expected) or any(not re.fullmatch(pattern, fields[key]) for key, pattern in expected.items()):
            raise ValueError('incomplete or malformed native PCM checkpoint snapshot')
    if saved[2] != restored[2]:
        raise ValueError('native PCM checkpoint snapshot differs after restoration')
    saved_time = float(fields['t'])
    ending = re.fullmatch(r't=([0-9]+\.[0-9]{9}) physical_release_replayed=0', complete[2])
    if not ending:
        raise ValueError('malformed native PCM completion boundary')
    end_time = float(ending[1])
    if not 8.14 <= saved_time <= 8.17 or not 8.19 <= end_time < 8.22 or 'physical_release_replayed=0' not in complete[2]:
        raise ValueError('native PCM replay must finish while the physical key remains held')
    reference = WORDS.findall(text[saved.end():restored.start()])
    replay = WORDS.findall(text[restored.end():complete.start()])
    if reference != replay or len(reference) < 240 or not any(int(value, 16) for value, _ in reference):
        raise ValueError('native transmitted words or absolute frame times differ after load')
    if any(not saved_time <= float(time) < end_time for _, time in reference):
        raise ValueError('native PCM suffix escapes its checkpoint interval')
    keys = re.findall(r'input-(press|release): t=([0-9.]+) name=2\b', text)
    if [(kind, float(time)) for kind, time in keys] != [('press', 8.0), ('release', 8.22)]:
        raise ValueError('physical key is not held unchanged across both native PCM legs')
    # Remove only the replay leg when checking the ordinary complete run;
    # that checker must still reject duplicate/missing samples on one timeline.
    canonical = text[:restored.start()] + text[complete.end():]
    result = check(canonical, clock=True)
    result['canonical_captured_words'] = result['captured_words']
    result['captured_words'] = len(WORDS.findall(text))
    result.update(replayed_words=len(reference), snapshot_equal=True,
                  absolute_frame_times_equal=True, physical_release_replayed=False)
    return result

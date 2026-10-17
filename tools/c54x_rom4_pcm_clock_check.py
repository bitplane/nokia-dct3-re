"""Check externally clocked native NSE-1 tone words, not product audio wiring."""
import math
import re


WORDS = re.compile(r'native_pcm_fixture_word: value=([0-9a-f]{4}) t=([0-9.]+)')
SUMMARY = re.compile(r'rom4_tone_summary: tx_words=(\d+) rx_reads=(\d+) tone_reads=(\d+) tone_copies=(\d+) t=([0-9.]+)')


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
                handset_clock_claim=False, native_call_speech_claim=False)

"""Check externally clocked native NSE-1 tone words, not product audio wiring."""
import math
import re


WORDS = re.compile(r'native_pcm_fixture_word: value=([0-9a-f]{4}) t=([0-9.]+)')
SUMMARY = re.compile(r'rom4_tone_summary: tx_words=(\d+) rx_reads=(\d+) tone_reads=(\d+) tone_copies=(\d+) t=([0-9.]+)')


def reject_native_faults(text):
    if re.search(r'unimplemented|unsupported|rom4_reset_request|TMS320C54x[^\n]*illegal', text, re.IGNORECASE):
        raise ValueError('native DSP fault or baseband reset during clocked comparison')


def check_clocked_boot(text, *, staged=False):
    """Require a pin-completed native self-test, not the untimed word echo."""
    summaries = re.findall(r'native_pcm_fixture_echo_summary: words=(\d+) untimed=(\d+) handset_clock_claim=0', text)
    echoes = re.findall(r'native_pcm_fixture_echo_word: value=([0-9a-f]{4}) t=([0-9.]+)', text)
    if len(summaries) != 1 or not echoes or tuple(map(int, summaries[0])) != (len(echoes), 0):
        raise ValueError('missing clocked boot echo or nonzero untimed loopback')
    events = re.compile(r'rom4_serial_audit: space=data direction=(write|read) address=(0021|0020) value=0aaa mask=ffff pc=([0-9a-f]{4}) t=([0-9.]+)')
    writes = [float(time) for direction, address, pc, time in events.findall(text)
              if (direction, address, pc) == ('write', '0021', '0e31')]
    # A memory tap reports the frontend's current PC, not necessarily the
    # instruction owning its retained operand. Keep the observed contexts
    # explicit instead of interpreting staged e60 as another LDM instruction.
    read_pc = '0e60' if staged else '0e5d'
    reads = [float(time) for time in re.findall(r'rom4_codec_boot_echo: value=0aaa pc=' + read_pc + r' t=([0-9.]+)', text)]
    completed = [float(time) for value, time in echoes if value == '0aaa']
    if not writes or len(reads) != 1 or not completed or not writes[0] < completed[0] <= reads[0] or reads[0] - writes[0] < 16e-6 - 2e-9:
        raise ValueError('native self-test did not consume a delayed pin-completed 0aaa word')
    for _, time in echoes:
        clocks = float(time) * 1_000_000
        if abs(clocks - round(clocks)) > 0.002 or round(clocks) % 125 != 16:
            raise ValueError('clocked boot echo is outside the supplied wire phase')
    return dict(clocked_echo_words=len(echoes), untimed_loopbacks=0,
                first_echo_latency_us=(reads[0] - writes[0]) * 1_000_000,
                handset_clock_claim=False)


def demand_transitions(text, *, staged=False):
    entries = re.compile(r'rom4_codec_demand_entry: hit=(\d+) operation=(acquire|release) mask=([0-9a-f]{4}) previous=([0-9a-f]{4}) t=([0-9.]+)')
    acquire_pc, release_pc = ('00a4c3', '00a4df') if staged else ('00a4c4', '00a4e0')
    writes = re.compile(r'rom4_tone_access: owner=codec_demand direction=write address=0000ef value=([0-9a-f]{4}) mask=ffff pc=(' + acquire_pc + '|' + release_pc + r') t=([0-9.]+)')
    totals = re.findall(r'rom4_codec_demand_summary: entries=(\d+) t=([0-9.]+)', text)
    events = sorted([(match.start(), 'entry', match) for match in entries.finditer(text)] +
                    [(match.start(), 'write', match) for match in writes.finditer(text)])
    pending, count, previous_time = None, 0, -1.0
    masks = {'acquire': set(), 'release': set()}
    for _, kind, event in events:
        time = float(event[5] if kind == 'entry' else event[3])
        if time < previous_time:
            raise ValueError('unordered codec demand events')
        previous_time = time
        if kind == 'entry':
            count += 1
            if pending or int(event[1]) != count:
                raise ValueError('missing codec demand commit or nonsequential entry')
            operation, mask, old = event[2], int(event[3], 16), int(event[4], 16)
            masks[operation].add(mask)
            pending = ((old | mask) if operation == 'acquire' else (old & ~mask),
                       acquire_pc if operation == 'acquire' else release_pc)
        else:
            if pending != (int(event[1], 16), event[2]):
                raise ValueError('codec demand write does not match entry bitmap operation')
            pending = None
    if pending or not count or len(totals) != 1 or int(totals[0][0]) != count or float(totals[0][1]) < previous_time:
        raise ValueError('missing or truncated codec demand observations')
    return dict(transitions=count, masks={key: sorted(values) for key, values in masks.items()},
                physical_clock_enable_claim=False)


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


def check_wire(text):
    result = check(text, clock=True)
    # This laboratory source has a fixed phase: the 16th sampled bit is at
    # +16 us in each 125-us frame. It is not a measured NSE-1 pulse shape.
    for value, time in WORDS.findall(text):
        cycles = float(time) * 1_000_000
        if abs(cycles - round(cycles)) > 0.002 or round(cycles) % 125 != 16:
            raise ValueError('wire output does not complete on the assumed 1 MHz/125-clock phase')
        word = int(value, 16)
        signed = word - (65536 if word & 0x8000 else 0)
        if not -4096 <= signed <= 4095:
            raise ValueError('wire output is not a sign-extended 13-bit converter word')
    result.update(bit_clock_hz=1_000_000, frame_clocks=125,
                  converter_word_bits=13, assumed_sync_clocks=1,
                  assumed_receive_transmit_fanout=True)
    return result


def check_restore(text, *, wire=False):
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
    if wire and abs(saved_time - 8.150008) > 1e-9:
        raise ValueError('wire checkpoint does not capture the selected partial word')
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
    result = check_wire(canonical) if wire else check(canonical, clock=True)
    result['canonical_captured_words'] = result['captured_words']
    result['captured_words'] = len(WORDS.findall(text))
    result.update(replayed_words=len(reference), snapshot_equal=True,
                  absolute_frame_times_equal=True, physical_release_replayed=False)
    return result

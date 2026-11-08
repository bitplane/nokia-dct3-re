"""Validate observed sample provenance, not SCH reception or fitted timing."""
import hashlib
import re

from tools.rom6_adc_lab_check import reference_word
from tools.rom6_timed_output_check import nanoseconds


RAW = re.compile(r'npe3_sch_raw: ordinal=(\d+) t=([\d.]+) value=([0-9a-f]{4}) source_tick=(\d+) capture_sequence=(\d+) owner=([0-9a-f]{6}) observer_writes=0')
WRITER = re.compile(r'npe3_sch_writer: address=([0-9a-f]{4}) value=([0-9a-f]{4}) t=([\d.]+) owner=([0-9a-f]{6}) ar2=([0-9a-f]{4}) ar3=([0-9a-f]{4}) observer_writes=0')
SUMMARY = re.compile(r'npe3_sch_provenance: raw_total=(\d+) raw_cap=1024 writers=192 begin=3 end=3.4 aperture_t=([\d.]+) observer_writes=0')
APERTURE = re.compile(r'npe3_sch_aperture: t=([\d.]+) owner=([0-9a-f]{6}) pointer=00d8 current=([0-9a-f]{4},[0-9a-f]{4}) next=([0-9a-f]{4},[0-9a-f]{4}) samples=([0-9a-f,]+) observer_writes=0')
FILTER = re.compile(r'npe3_sch_filter_input: t=([\d.]+) source=0082 samples=([0-9a-f,]+) coefficients=df85,6d1b,df85 history=([0-9a-f,]+) st1=c100 observer_writes=0')
CONTROL = re.compile(r'npe3_fcch_control: ordinal=(\d+) t=([\d.]+) kind=(port|data) address=([0-9a-f]{4}) value=([0-9a-f]{4}) owner=([0-9a-f]{6}) counter=([0-9a-f]{4}) counter_t=([\d.]+) frame=([0-9a-f]{4}) offset=([0-9a-f]{4},[0-9a-f]{4}) shift=([0-9a-f]{4}) observer_writes=0')


def check_capture_compare(text, source):
    """Bound two compare hypotheses under the declared counter, not silicon."""
    controls = rows(CONTROL, text, 'npe3_fcch_control', 64)
    ordinals = [int(row[0]) for row in controls]
    if ordinals != list(range(ordinals[0], ordinals[0] + 64)):
        raise ValueError('capture-control suffix is not consecutive')
    enable = [row for row in controls if row[2:5] == ('port', '0003', '0105')]
    if len(enable) != 1:
        raise ValueError('final capture enable is ambiguous')
    enable = enable[0]
    before = [row for row in controls if int(row[0]) < int(enable[0])]
    target = [row for row in before if row[2:4] == ('port', '0009')]
    reloads = [row for row in before if row[2:4] == ('port', '000e')]
    if not target or not reloads or reloads[-1][4] != '1387':
        raise ValueError('capture compare lacks the declared 5000-quarter period')
    start = source['source_first_tick']
    counter_time = nanoseconds(enable[7])
    if not counter_time < nanoseconds(enable[1]) <= (start * 1_000_000_000) // 13_000_000:
        raise ValueError('capture enable/counter/source ordering differs')
    if any(row[2] == 'port' and row[3] in ('000d', '000e')
           and counter_time <= nanoseconds(row[1]) <= start * 1_000_000_000 // 13_000_000
           for row in controls):
        raise ValueError('counter was reloaded after the passive read')
    elapsed = (start * 1_000_000_000 - counter_time * 13_000_000) // (12 * 1_000_000_000)
    counter_min = int(enable[6], 16) + elapsed
    counter_max = counter_min + 1
    compare = int(target[-1][4], 16)
    if not 0 <= counter_min <= counter_max < compare < 5000:
        raise ValueError('capture compare would wrap or has an unqualified counter bound')
    # The passive read does not recover the divider's fractional phase.
    # Widen by a whole quarter rather than inventing that missing phase.
    lower = (compare - counter_max - 1) * 12
    upper = (compare - counter_min) * 12
    phase = source['source_frame_phase']
    return dict(counter_at_first_word_bounds=[counter_min, counter_max],
                compare=compare, compare_source_phase_bounds=[phase + lower, phase + upper],
                completed_block_first_word_phase_bounds=[phase + lower - 31 * 12, phase + upper - 31 * 12],
                independent_sch_envelope_end=7104, physical_compare_owner_qualified=False)


def rows(pattern, text, prefix, count):
    result = list(pattern.finditer(text))
    if len(result) != count or text.count(prefix + ':') != count:
        raise ValueError(f'incomplete or malformed {prefix}')
    return [match.groups() for match in result]


def words(value, count):
    if not re.fullmatch(r'[0-9a-f]{4}(?:,[0-9a-f]{4})*', value):
        raise ValueError('malformed sample words')
    result = [int(word, 16) for word in value.split(',')]
    if len(result) != count:
        raise ValueError('sample word count differs')
    return result


def check(text, *, receive_words=960):
    if receive_words not in (960, 992):
        raise ValueError('unqualified SCH receive-word contract')
    total, aperture_time = rows(SUMMARY, text, 'npe3_sch_provenance', 1)[0]
    total, aperture_time = int(total), float(aperture_time)
    raw = rows(RAW, text, 'npe3_sch_raw', 1024)
    previous = None
    epochs = []
    owners = {0x247e, 0x2484, 0x248a, 0x2490, 0x2441, 0x2444, 0x2447,
              0x244a, 0x18a4, 0x18aa, 0x18b0, 0x18b6, 0x2592, 0x2598, 0x259e, 0x25a4}
    for offset, row in enumerate(raw):
        ordinal, time, value, tick, sequence, owner = row
        ordinal, tick, sequence = int(ordinal), int(tick), int(sequence)
        time, value, owner = float(time), int(value, 16), int(owner, 16)
        if (ordinal != total - 1023 + offset or not 3 <= time < aperture_time <= 3.4
                or tick / 13_000_000 > time or owner not in owners
                or value != reference_word(sequence, tick, True, 12, True, True)):
            raise ValueError('raw sample does not match observed source/reader contract')
        if previous is None or sequence == 1:
            if previous is not None and tick <= previous[1]:
                raise ValueError('receive epoch source time went backwards')
            epochs.append([])
        elif sequence != previous[0] + 1 or tick != previous[1] + 12:
            raise ValueError('raw capture has a missing sample or unknown epoch')
        if previous is not None and time < previous[2]:
            raise ValueError('raw reads are out of order')
        epochs[-1].append((sequence, tick, value))
        previous = sequence, tick, time
    current = epochs[-1]
    if current[0][0] != 1 or current[-1][0] != receive_words:
        raise ValueError('last SCH receive epoch is incomplete')
    if not re.search(rf'mfi_rx_epoch: first_tick={current[0][1]} divider=12 physical_clock_claim=0', text):
        raise ValueError('source ticks lack the independently logged enable epoch')
    when, owner, frame, next_frame, samples = rows(APERTURE, text, 'npe3_sch_aperture', 1)[0]
    samples = words(samples, 192)
    if (float(when) != aperture_time or int(owner, 16) != 0x76ae
            or int(next_frame.replace(',', ''), 16) != int(frame.replace(',', ''), 16) + 1):
        raise ValueError('native correlator aperture contract differs')
    when, source, history = rows(FILTER, text, 'npe3_sch_filter_input', 1)[0]
    source, history = words(source, 196), words(history, 4)
    filter_time = float(when)
    if not previous[2] < filter_time < aperture_time:
        raise ValueError('filter input was not captured between receive and correlation')
    writer_owners = (0x794a, 0x794b, 0x794f, 0x7950, 0x7955, 0x7956, 0x7958, 0x7959)
    for index, row in enumerate(rows(WRITER, text, 'npe3_sch_writer', 192)):
        address, value, time, owner, ar2, ar3 = row
        address, value, owner, ar2, ar3 = (int(item, 16) for item in (address, value, owner, ar2, ar3))
        if (address != 0xd8 + index or value != samples[index]
                or owner != writer_owners[index % 8] or ar3 != (address & ~1) + 2
                or ar2 != ar3 + 2 or not filter_time <= float(time) < aperture_time):
            raise ValueError('aperture word lacks matching native builder provenance')
    start, finish = current[0][1], current[-1][1]
    phase = start % 60000
    sch_frame = start // 60000 % 51 in (1, 11, 21, 31, 41)
    return dict(raw_words_checked=1024, final_receive_words=receive_words,
                source_first_tick=start, source_last_tick=finish,
                source_frame=start // 60000, source_frame_phase=phase,
                starts_inside_sch=sch_frame and phase < 7104,
                missing_sch_prefix_complex_samples=phase / 24 if sch_frame and phase < 7104 else None,
                nonzero_source_words=sum(value != 0 for _, _, value in current),
                native_aperture_words_checked=192,
                aperture_sha256=hashlib.sha256(b''.join(word.to_bytes(2, 'big') for word in samples)).hexdigest(),
                filter_input_nonzero=sum(value != 0 for value in source),
                physical_timing_qualified=False, sch_qualified=False, camping_qualified=False)

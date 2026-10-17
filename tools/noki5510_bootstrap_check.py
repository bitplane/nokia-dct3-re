"""Check own-upload execution, not graphical boot or fitted mask identity."""
import argparse
import hashlib
import json
from pathlib import Path
import re

CHAIN = (
    '5510_startup_adc: sample=03ff',
    '5510_verifier_enter: descriptor=003ea428',
    'stage=verifier',
    'publication word0=0000 word1=0006',
    '5510_verifier_result: word0=0000 word1=0006',
    '5510_loader_enter: descriptor=003e3304',
    'stage=loader',
    'loader2_verified words=629 entry=0a00',
    'outside_uploaded_code pc=2c75',
    'observation_halt pc=2c75 ownership_retained=1',
    'model_scout: t=8.000',
)


RUNTIME_CHAIN = CHAIN[:9] + (
    'runtime_hle_handoff pc=2c75 native_suspended=1',
    'data=1eff00d000030101e000',
    'RX enqueue type=8e payload=10',
    'RX enqueue type=74 payload=2',
    '5510_service_reply: command=0d faults=00 flag=8c',
    'data=0a09',
    'model_scout: t=8.000',
)


def verify(text, runtime=False):
    failures = ('unavailable_program', '[LUA ERROR]',
                'Disk quota exceeded', 'No space left on device')
    if not runtime:
        failures += ('runtime_hle_handoff',)
    for failure in failures:
        if failure in text:
            raise ValueError('unexpected bootstrap failure/shortcut: ' + failure)
    cursor = 0
    for event in RUNTIME_CHAIN if runtime else CHAIN:
        found = text.find(event, cursor)
        if found < 0:
            raise ValueError('missing ordered bootstrap evidence: ' + event)
        cursor = found + len(event)


def verify_serial_readiness(text):
    """Require queue service, not merely a final ready-shaped observation."""
    cursor = 0
    for event in (
        '5510_input_tasks_created: task29_state=05',
        'address=00120c70 data=01',
        'address=0011cddc data=01',
        'address=0011cddc data=00',
        'address=00120c70 data=00',
        '5510_input_startup_gate: ready=01 busy=00,00,00,00 fiqmask=c8',
    ):
        found = text.find(event, cursor)
        if found < 0:
            raise ValueError('missing ordered serial-readiness evidence: ' + event)
        cursor = found + len(event)


def verify_input_lifecycle(text):
    cursor = 0
    for event in (
        '5510_task_enter: index=1d entry=00335f1e',
        '5510_input_serial_ui_init: mode=00 caller=00335f25',
        '5510_input_serial_ui_receive:',
    ):
        found = text.find(event, cursor)
        if found < 0:
            raise ValueError('missing ordered input-lifecycle evidence: ' + event)
        cursor = found + len(event)


def verify_input_timer(text):
    armed = text.find('5510_input_timer_armed:')
    if armed < 0:
        raise ValueError('missing MU4 timer-arm evidence')
    line = text[armed:].splitlines()[0]
    if 'state=02 owner=1d' not in line:
        raise ValueError('MU4 timer is not armed for task 29')
    if text.find('5510_input_timer_dispatch: event=01e7', armed) < 0:
        raise ValueError('missing subsequent MU4 timer delivery; extend observation window')


def verify_dsp_service_route(text):
    """Positive HLE discovery transport control; not a MU4 acceptance test."""
    forward = re.search(
        r'5510_input_dsp_forward: caller=003a53f7 object=[0-9a-f]{8} '
        r'length=0a type=8e transport=1e destination=00 source=02 class=d0\b', text)
    if forward is None:
        raise ValueError('missing task-4 DSP service forwarding evidence')
    cursor = forward.end()
    for frame_class in ('01', '04'):
        ingress = re.search(
            r'5510_input_serial_ingress: caller=002f58b1 object=[0-9a-f]{8} '
            r'length=0001 transport=1e destination=00 source=02 class=' + frame_class + r'\b',
            text[cursor:])
        if ingress is None:
            raise ValueError('missing subsequent sequenced service ingress class ' + frame_class)
        cursor += ingress.end()


def verify_resident_memory(text):
    """Check original AA22 program/data RAM contents, not register effects or boot."""
    for kind in ('program', 'data'):
        if text.count('mu4_original_resident_' + kind + ':') != 1:
            raise ValueError('missing or duplicated resident ' + kind + ' report')
    for failure in ('mu4_original_resident_program_mismatch:',
                    'mu4_original_resident_data_mismatch:', '[LUA ERROR]',
                    'Disk quota exceeded', 'No space left on device'):
        if failure in text:
            raise ValueError('resident memory audit failure: ' + failure)
    reports = (
        (r'mu4_original_resident_program: (PASS|MISMATCH) checked_words=(\d+) '
         r'mismatch_words=(\d+) excluded_data_words=(\d+) source_words=(\d+) '
         r'before_resident_execution=1 physical_mapping=unvalidated full_boot=0$',
         (48261, 0, 3660, 51921)),
        (r'mu4_original_resident_data: (PASS|MISMATCH) checked_words=(\d+) '
         r'mismatch_words=(\d+) excluded_register_words=(\d+) source_data_words=(\d+) '
         r'before_resident_execution=1 physical_mapping=unvalidated full_boot=0$',
         (3656, 0, 4, 3660)),
    )
    cursor = 0
    for pattern, expected in reports:
        matches = list(re.finditer(pattern, text, re.MULTILINE))
        if len(matches) != 1:
            raise ValueError('missing or duplicated complete resident memory report')
        match = matches[0]
        if match.start() < cursor or match[1] != 'PASS' or tuple(map(int, match.groups()[1:])) != expected:
            raise ValueError('resident memory scope/count/result mismatch')
        cursor = match.end()


def verify_playback_lifecycle(text):
    """A bounded raw-bus observation; context PC does not establish write ownership."""
    scope = ('mu4_playback_lifecycle_scope: boundary=data_write context_pc=not_owner '
             'observer_writes=0 audio_acceptance=0')
    if text.count(scope) != 1 or text.count('mu4_playback_lifecycle_scope:') != 1:
        raise ValueError('missing or duplicated passive playback lifecycle scope')
    writes = list(re.finditer(
        r'mu4_playback_lifecycle_write: address=(373e|3742|3750|3762) value=([0-9a-f]{4}) '
        r'mask=ffff ordinal=(\d+) t=(\d+\.\d{9}) context_pc=[0-9a-f]{6} observer_writes=0$',
        text, re.MULTILINE))
    reads = list(re.finditer(
        r'mu4_playback_control_read: value=([0-9a-f]{4}) t=(\d+\.\d{9}) '
        r'context_pc=(?P<pc>[0-9a-f]{6}) selection=context_range reader_identity=unvalidated observer_writes=0$',
        text, re.MULTILINE))
    reports = list(re.finditer(
        r'mu4_playback_lifecycle_terminal: pc=([0-9a-f]{6}) illegal=0 '
        r'lifecycle_writes=(\d+) control_writes=(\d+) control_last=0000 control_reads=(\d+) '
        r'cleanup_context_reads=(\d+) mode_writes=(\d+) continuation_writes=(\d+) '
        r'observer_writes=0 audio_acceptance=0$', text, re.MULTILINE))
    if len(reports) != 1 or text.count('mu4_playback_lifecycle_terminal:') != 1 or len(writes) != text.count('mu4_playback_lifecycle_write:') or len(reads) != text.count('mu4_playback_control_read:'):
        raise ValueError('incomplete, malformed or duplicated playback lifecycle observation')
    terminal = reports[0]
    expected = dict(zip(('373e', '3742', '3750', '3762'),
                        map(int, (terminal[2], terminal[3], terminal[6], terminal[7]))))
    seen = {field: 0 for field in expected}
    previous_time = -1
    states = []
    for event in writes:
        field = event[1]
        seen[field] += 1
        time = float(event[4])
        if int(event[3]) != seen[field] or time < previous_time or event.end() >= terminal.start():
            raise ValueError('playback lifecycle ordinal/time/report ordering mismatch')
        previous_time = time
        if field == '373e':
            states.append((int(event[2], 16), time))
    if seen != {field: min(count, 16) for field, count in expected.items()}:
        raise ValueError('incomplete bounded playback lifecycle writes')
    if len(reads) != min(int(terminal[5]), 4) or int(terminal[4]) < int(terminal[5]):
        raise ValueError('incomplete bounded playback control reads')
    if any(not 0x29560 <= int(read['pc'], 16) <= 0x29590 or read.end() >= terminal.start()
           for read in reads):
        raise ValueError('playback control read is outside the declared context selector')
    witnesses = [(float(read[2]), later) for read in reads if int(read[1], 16) == 0
                 for value, later in states if value == 5 and later >= float(read[2])
                 if any(state == 4 and before <= float(read[2]) for state, before in states)
                 and any(state == 4 and after > later for state, after in states)]
    if not witnesses:
        raise ValueError('missing ordered state-4/control-zero/state-5/re-entry observation')
    return {'pc': terminal[1], 'control_read_time': witnesses[0][0],
            'state5_time': witnesses[0][1], 'audio_acceptance': False}


def verify_playback_observation(text):
    """Validate observation scope, not decoding, aligned PCM or handset boot."""
    def report(pattern, label):
        matches = list(re.finditer(pattern + '$', text, re.MULTILINE))
        prefix = pattern.split(':', 1)[0] + ':'
        if len(matches) != 1 or text.count(prefix) != 1:
            raise ValueError('missing or duplicated playback ' + label)
        return matches[0]

    window = report(r'mu4_playback_audio_window: start=decoder_execute_entry t=(\d+\.\d{9}) '
                    r'sample_source=converted_sample_bench music_decode=0', 'window')
    execution = report(
        r'mu4_native_playback_execution: bootstrap_fetch=(\d+) bootstrap_execute=(\d+) '
        r'resident_fetch=(\d+) resident_execute=(\d+) filter_calls=(\d+) '
        r'filter_iterations=(\d+) filter_returns=(\d+) instruction_entries=(\d+) loader_execute=(\d+) '
        r'observer=Execute_entry retirement_claim=0', 'execution')
    audio = report(
        r'mu4_native_playback_audio: tx_words=(\d+) din_words=(\d+) tx_nonzero=(\d+) '
        r'din_nonzero=(\d+) tx_hash=([0-9a-f]{8}) din_hash=([0-9a-f]{8}) '
        r'clocks=(\d+) frames=(\d+) power=([0-9a-f]{4}) format=([0-9a-f]{4}) '
        r'rate=([0-9a-f]{4}) active=([0-9a-f]{4}) '
        r'tx_last_nonzero=(?P<tx_last>\d+\.\d{9}) din_last_nonzero=(?P<din_last>\d+\.\d{9}) '
        r'window=decoder_entry '
        r'stream_alignment=unvalidated music_decode=0 analog_audio=0', 'audio')
    playback = report(
        r'mu4_native_playback_observe: requests=1 inspections=1 decoder_loads=1 '
        r'replies=(\d+) input_cursor=12 title_verified=1 overlay_verified=1 '
        r'pc=([0-9a-f]{6}) illegal=0 audio_acceptance=0', 'result')
    if not window.start() < execution.start() < audio.start() < playback.start():
        raise ValueError('playback observation ordering mismatch')
    values = tuple(map(int, execution.groups()))
    calls, iterations, returns = values[4:7]
    if not values[1] or not values[3] or not values[7] or not values[8]:
        raise ValueError('playback lacks actual Execute-entry evidence')
    if returns > calls or not 64 * returns <= iterations <= 64 * calls:
        raise ValueError('AA99 filter loop observations contradict its 64-iteration contract')
    trail_pattern = (
        r'mu4_playback_entry_transfer: target=([0-9a-f]{6}) index=(\d+) '
        r'p0=([0-9a-f]{6})/([0-9a-f]{4}) p1=([0-9a-f]{6})/([0-9a-f]{4}) '
        r'p2=([0-9a-f]{6})/([0-9a-f]{4}) p3=([0-9a-f]{6})/([0-9a-f]{4}) '
        r'a=([0-9a-f]{10}) lifecycle=([0-9a-f]{4}) mode=([0-9a-f]{4}) continuation=([0-9a-f]{4}) '
        r'identity=Execute_entry$')
    trails = list(re.finditer(trail_pattern, text, re.MULTILINE))
    if len(trails) != text.count('mu4_playback_entry_transfer:'):
        raise ValueError('malformed playback transfer provenance')
    targets = {'000e41': min(values[1], 16), '026d62': min(values[3], 16),
               '002080': min(values[8], 16)}
    seen = {target: 0 for target in targets}
    for trail in trails:
        target, index = trail[1], int(trail[2])
        if target not in targets or index != seen[target] or trail.start() >= execution.start():
            raise ValueError('playback transfer target/index/order mismatch')
        seen[target] += 1
    if seen != targets:
        raise ValueError('incomplete bounded playback transfer provenance')
    tx, din, tx_nonzero, din_nonzero = map(int, audio.groups()[:4])
    if tx_nonzero > tx or din_nonzero > din:
        raise ValueError('playback nonzero count exceeds complete words')
    for count, time in ((tx_nonzero, float(audio['tx_last'])),
                        (din_nonzero, float(audio['din_last']))):
        if (not count and time != 0) or (count and time < float(window[1])):
            raise ValueError('playback nonzero time is outside its observation window')
    return {'bootstrap_fetch': values[0], 'bootstrap_execute': values[1],
            'resident_fetch': values[2], 'resident_execute': values[3],
            'loader_execute': values[8],
            'filter_calls': calls, 'filter_iterations': iterations, 'filter_returns': returns,
            'entry_transfer_reports': len(trails),
            'tx_words': tx, 'din_words': din, 'tx_nonzero': tx_nonzero,
            'tx_hash': audio[5], 'din_hash': audio[6],
            'din_nonzero': din_nonzero, 'tx_last_nonzero': audio['tx_last'],
            'din_last_nonzero': audio['din_last'], 'pc': playback[2]}


def verify_playback_dma(text):
    """Validate actual channel-3 data-read observations, not TX/media pairing."""
    result = verify_playback_observation(text)
    summaries = list(re.finditer(
        r'mu4_playback_dma_source: reads=(\d+) nonzero=(\d+) minimum=([0-9a-f]{4}) '
        r'maximum=([0-9a-f]{4}) last_nonzero=(\d+\.\d{9}) channel=3 space=data '
        r'ownership=active_source_read window=decoder_entry tx_pairing=unvalidated music_decode=0$',
        text, re.MULTILINE))
    events = list(re.finditer(
        r'mu4_playback_dma_word: kind=(prefix|nonzero) ordinal=(\d+) read=(\d+) '
        r'address=([0-9a-f]{4}) word=([0-9a-f]{4}) t=(\d+\.\d{9}) '
        r'channel=3 space=data ownership=active_source_read$', text, re.MULTILINE))
    if len(summaries) != 1 or text.count('mu4_playback_dma_source:') != 1 or len(events) != text.count('mu4_playback_dma_word:'):
        raise ValueError('missing, malformed or duplicated playback DMA observation')
    summary = summaries[0]
    reads, nonzero = int(summary[1]), int(summary[2])
    minimum, maximum, last = int(summary[3], 16), int(summary[4], 16), float(summary[5])
    window = re.search(r'mu4_playback_audio_window: start=decoder_execute_entry t=(\d+\.\d{9})', text)
    start = float(window[1])
    if not reads or nonzero > reads or minimum > maximum or (nonzero and last < start) or (not nonzero and last):
        raise ValueError('inconsistent playback DMA counts, addresses or time')
    if not text.index('mu4_native_playback_audio:') < summary.start() < text.index('mu4_native_playback_observe:'):
        raise ValueError('playback DMA summary is outside its declared terminal order')
    counts = {'prefix': 0, 'nonzero': 0}
    previous_read = {'prefix': 0, 'nonzero': 0}
    samples = {}
    prefix_nonzero, reported_nonzero = [], []
    previous_time = start
    for event in events:
        kind, ordinal, index = event[1], int(event[2]), int(event[3])
        address, value, time = int(event[4], 16), int(event[5], 16), float(event[6])
        counts[kind] += 1
        if (ordinal != counts[kind] or not previous_read[kind] < index <= reads or
                not minimum <= address <= maximum or time < previous_time or
                event.start() < window.end() or event.end() >= summary.start() or
                (kind == 'prefix' and index != ordinal) or
                (kind == 'nonzero' and (not value or time > last))):
            raise ValueError('playback DMA sample ordinal, scope or ordering mismatch')
        sample = (address, value, time)
        if index in samples and samples[index] != sample:
            raise ValueError('playback DMA prefix and nonzero disagree on the same source read')
        samples[index] = sample
        if kind == 'nonzero':
            reported_nonzero.append(index)
        elif value:
            prefix_nonzero.append(index)
        previous_read[kind], previous_time = index, time
    if counts != {'prefix': min(reads, 4), 'nonzero': min(nonzero, 4)}:
        raise ValueError('incomplete bounded playback DMA samples')
    if prefix_nonzero != reported_nonzero[:len(prefix_nonzero)]:
        raise ValueError('playback DMA nonzero prefix is missing from its nonzero sample stream')
    return {**result, 'dma_reads': reads, 'dma_nonzero': nonzero,
            'dma_minimum': summary[3], 'dma_maximum': summary[4],
            'tx_pairing': 'unvalidated', 'music_decode': False}


def verify_playback_banks(text):
    """Data-space observation coverage only; contexts are not writer ownership."""
    scope = ('mu4_playback_banks_scope: first=1000 last=107f upstream_first=7000 '
             'upstream_last=73ff start=1.000000000 finish=2.000000000 boundary=data_write '
             'context_pc=not_owner observer_writes=0 audio_acceptance=0')
    if text.count(scope) != 1 or text.count('mu4_playback_banks_scope:') != 1:
        raise ValueError('missing or changed playback bank observation scope')
    reports = {}
    for kind in ('banks', 'upstream'):
        prefix = 'mu4_playback_' + kind
        head = r'pc=([0-9a-f]{6}) illegal=0 ' if kind == 'banks' else ''
        tail = r' t=(\d+\.\d{9})' if kind == 'banks' else ''
        matches = list(re.finditer(prefix + r'_terminal: ' + head +
            r'writes=(\d+) nonzero_driven=(\d+) partial=0 contexts=(\d+) overflow=0' + tail +
            r' observer_writes=0 audio_acceptance=0$', text, re.MULTILINE))
        if len(matches) != 1 or text.count(prefix + '_terminal:') != 1:
            raise ValueError('missing, malformed or duplicated playback bank terminal')
        report = matches[0]
        offset = 1 if kind == 'banks' else 0
        writes, nonzero, count = map(int, report.groups()[offset:offset + 3])
        if nonzero > writes or count > 512 or bool(writes) != bool(count) or (kind == 'banks' and writes < 4):
            raise ValueError('inconsistent playback bank counts')
        if kind == 'banks' and float(report[5]) < 2:
            raise ValueError('playback bank observation stopped before its window ended')
        label = 'bank' if kind == 'banks' else 'upstream'
        contexts = list(re.finditer(r'mu4_playback_' + label + r'_context: '
            r'context_pc=([0-9a-f]{6}) writes=(\d+) nonzero_driven=(\d+) identity=unvalidated$',
            text, re.MULTILINE))
        if (len(contexts) != count or text.count('mu4_playback_' + label + '_context:') != count or
                len({item[1] for item in contexts}) != count or
                sum(int(item[2]) for item in contexts) != writes or
                sum(int(item[3]) for item in contexts) != nonzero or
                any(not 0 <= int(item[3]) <= int(item[2]) or item.end() >= report.start() or
                    item.start() <= text.index(scope) for item in contexts)):
            raise ValueError('incomplete or inconsistent playback bank context census')
        reports[kind] = {'writes': writes, 'nonzero_driven': nonzero, 'contexts': count}
    copies = list(re.finditer(r'mu4_playback_copy_context: ordinal=(\d+) destination=([0-9a-f]{4}) '
        r'ar3=([0-9a-f]{4}) current=([0-9a-f]{4}) previous=([0-9a-f]{4}) ar4=([0-9a-f]{4}) '
        r'fields=([0-9a-f]{4}(?:,[0-9a-f]{4}){13}) context_pc=02a367 '
        r'source_identity=unvalidated observer_writes=0$', text, re.MULTILINE))
    if len(copies) != 4 or text.count('mu4_playback_copy_context:') != 4:
        raise ValueError('incomplete bounded playback copy contexts')
    for index, copy in enumerate(copies, 1):
        fields = [int(value, 16) for value in copy[7].split(',')]
        if (copy.start() <= text.index(scope) or copy.end() >= text.index('mu4_playback_banks_terminal:') or
                int(copy[1]) != index or not 0x1000 <= int(copy[2], 16) <= 0x107f or
                not 0x7000 <= int(copy[3], 16) <= 0x73ff or not 0x60 <= int(copy[6], 16) <= 0xfff2 or
                fields[6:8] != [0x7000, 0x400] or fields[10] != 0x1000):
            raise ValueError('playback copy context is outside its observed buffer selection')
    return {**reports, 'copy_contexts': len(copies), 'audio_acceptance': False,
            'writer_ownership': 'unvalidated'}


def verify_playback_parent(text):
    summary = list(re.finditer(
        r'mu4_playback_parent_terminal: wrapper=(\d+) internal=(\d+) body=(\d+) optional=(\d+) '
        r'returned=(\d+) loop=(\d+) dispatch=(\d+) select=(\d+) target=(\d+) fallthrough=(\d+) opcode_mismatch=0 first_session_complete=1 '
        r'overlay_verified=1 illegal=0 t=(\d+\.\d+) selection=first_verified_aa99_session '
        r'identity=Execute_entry observer_writes=0 audio_acceptance=0$', text, re.MULTILINE))
    if len(summary) != 1 or text.count('mu4_playback_parent_terminal:') != 1:
        raise ValueError('missing or invalid first-session parent coverage')
    counts = [int(summary[0][index]) for index in range(1, 11)]
    stop = float(summary[0][11])
    starts = re.findall(r'mu4_playback_audio_window: start=decoder_execute_entry t=(\d+\.\d+) ', text)
    residents = re.findall(r'mu4_playback_execute_entry: address=026d62 opcode=[0-9a-f]{4} t=(\d+\.\d+) ', text)
    if len(starts) != 1 or len(residents) < 2 or not float(residents[0]) < float(starts[0]) < float(residents[1]) <= stop:
        raise ValueError('parent coverage lacks a complete verified first overlay interval')
    resident_times = [float(value) for value in residents]
    if any(right <= left for left, right in zip(resident_times, resident_times[1:])) or any(time > stop for time in resident_times):
        raise ValueError('resident Execute entries are duplicated or out of time order')
    start, finish = float(starts[0]), float(residents[1])
    rows = list(re.finditer(
        r'mu4_playback_parent_context: site=([0-9]) ordinal=([12]) address=([0-9a-f]{6}) opcode=([0-9a-f]{4}) '
        r'sp=([0-9a-f]{4}) ar1=([0-9a-f]{4}) ar6=[0-9a-f]{4} ar7=[0-9a-f]{4} '
        r'a=[0-9a-f]{10} b=[0-9a-f]{10} stack=([0-9a-f,]+|unobserved) frame=([0-9a-f,]+|unobserved) '
        r't=(\d+\.\d+) identity=Execute_entry context_only=1 observer_writes=0$', text, re.MULTILINE))
    if len(rows) != text.count('mu4_playback_parent_context:'):
        raise ValueError('malformed parent context or unsupported identity claim')
    sites = (('02a10c', '4a11'), ('02a139', '6fe1'), ('02a22e', '11e1'),
             ('02a152', '6101'), ('02a138', 'f4e4'), ('02a2c6', '771a'), ('0205d0', '7711'),
             ('0205ac', '6c81'), ('0205b3', '71e7'), ('0205ae', '76f8'))
    observed = [0] * 10
    def check_snapshot(pointer, snapshot):
        valid = 0x80 <= int(pointer, 16) <= 0xfff0
        if (valid and not re.fullmatch(r'[0-9a-f]{4}(?:,[0-9a-f]{4}){15}', snapshot)) or (not valid and snapshot != 'unobserved'):
            raise ValueError('parent context has an incomplete or unsafe RAM snapshot')
        return [int(word, 16) for word in snapshot.split(',')] if valid else None
    for row in rows:
        site = int(row[1])
        observed[site] += 1
        if (int(row[2]) != observed[site] or (row[3], row[4]) != sites[site] or
                not start <= float(row[9]) < finish):
            raise ValueError('parent context lies outside its instruction/session selection')
        for pointer, snapshot in ((row[5], row[7]), (row[6], row[8])):
            check_snapshot(pointer, snapshot)
    if observed != [min(2, count) for count in counts]:
        raise ValueError('parent counters lack their bounded entry contexts')
    selectors = list(re.finditer(
        r'mu4_playback_selector_context: ordinal=([12]) receiver=([0-9a-f]{4}) words=([0-9a-f,]+|unobserved) '
        r'ar1=([0-9a-f]{4}) a=([0-9a-f]{10}) t=(\d+\.\d+) identity=Execute_entry context_only=1 observer_writes=0$', text, re.MULTILINE))
    if len(selectors) != min(2, counts[7]) or len(selectors) != text.count('mu4_playback_selector_context:'):
        raise ValueError('selector counters lack their bounded receiver contexts')
    samples = []
    for index, row in enumerate(selectors, 1):
        if int(row[1]) != index or not start <= float(row[6]) < finish:
            raise ValueError('selector context lies outside the selected first session')
        words = check_snapshot(row[2], row[3])
        samples.append({'receiver': int(row[2], 16), 'header': words[0] if words else None,
                        'field13': words[13] if words else None, 'ar1': int(row[4], 16), 'a': int(row[5], 16)})
    return dict(zip(('wrapper', 'internal', 'body', 'optional', 'returned', 'loop', 'dispatch', 'select', 'target', 'fallthrough'), counts),
                first_session=(start, finish), selector_samples=samples, decoded_audio=False, subsystem_identity='unvalidated')


def verify_playback_switch_history(text, words, session, *, kind='switch'):
    formats = {
        'switch': ('address=020478 opcode=10e4 ar4=804a', 'live_ar4_context'),
        'enqueue': ('address=0202d7 opcode=487e ar2=8044', 'live_ar2_context'),
        'activation': ('address=020419 opcode=f6be paired_ar2=8044', 'paired_enqueue_ar2_context'),
        'callback': ('address=020478 opcode=10e4 ar4=d7b0', 'live_ar4_context'),
        'notify': ('address=0201b3 opcode=f7b8', 'entry_address_context'),
        'restore': ('address=024538 opcode=eefe', 'entry_address_context'),
    }
    if kind not in formats:
        raise ValueError('unknown playback transfer history kind')
    label, (entry, selector) = kind, formats[kind]
    rows = list(re.finditer(
        rf'mu4_playback_{label}_history: ordinal=(\d+) {entry} '
        r'p0=([0-9a-f]{6})/([0-9a-f]{4}) p1=([0-9a-f]{6})/([0-9a-f]{4}) '
        r'p2=([0-9a-f]{6})/([0-9a-f]{4}) p3=([0-9a-f]{6})/([0-9a-f]{4}) '
        rf't=(\d+\.\d{{9}}) identity=Execute_entry selection={selector} '
        r'receiver_ownership=unvalidated observer_writes=0$', text, re.MULTILINE))
    if len(rows) != text.count(f'mu4_playback_{label}_history:') or len(rows) > 8:
        raise ValueError('malformed or excessive playback switch history')
    result, previous = [], session[0]
    for ordinal, row in enumerate(rows, 1):
        time = float(row[10])
        if int(row[1]) != ordinal or not previous <= time < session[1] or (kind == 'activation' and ordinal > 1 and time == previous):
            raise ValueError('playback switch history ordinal/session mismatch')
        history = [(int(row[i], 16), int(row[i + 1], 16)) for i in (2, 4, 6, 8)]
        if any(words.get(address) != opcode for address, opcode in history):
            raise ValueError('playback switch history disagrees with original AA99 source')
        result.append({'time': time, 'history': [f'{a:06x}/{op:04x}' for a, op in history]})
        previous = time
    return result


def compare_playback_bit_context(text, words, session):
    if __package__:
        from .mu4_context_contract import reference_bit_window
    else:
        from mu4_context_contract import reference_bit_window
    rows = list(re.finditer(
        r'mu4_playback_bit_context: boundary=(entry|return) ordinal=(\d+) address=([0-9a-f]{6}) '
        r'a=([0-9a-f]{10}) remaining=([0-9a-f]{4}) reservoir=([0-9a-f]{8}) index=([0-9a-f]{4}) '
        r'base=([0-9a-f]{4}) next=([0-9a-f]{4}|unobserved) caller=([0-9a-f]{6}) group=([0-3]) t=(\d+\.\d{9}) '
        r'identity=Execute_entry context_only=1 observer_writes=0$', text, re.MULTILINE))
    if len(rows) != text.count('mu4_playback_bit_context:') or len(rows) % 2 or len(rows) > 304:
        raise ValueError('malformed or unpaired bit-reader context')
    result, previous, source_counts = [], session[0], [0, 0, 0, 0]
    for index in range(0, len(rows), 2):
        entry, returned = rows[index:index + 2]
        for boundary, row, address, opcode in (('entry', entry, 0x2510c, 0xed00), ('return', returned, 0x25142, 0xf4e4)):
            time = float(row[12])
            if row[1] != boundary or int(row[2]) != index // 2 + 1 or int(row[3], 16) != address or words.get(address) != opcode:
                raise ValueError('bit-reader boundary, ordinal or original source mismatch')
            if not previous <= time < session[1]:
                raise ValueError('bit-reader context outside ordered first session')
            previous = time
        raw_request, remaining = int(entry[4], 16) & 0xffff, int(entry[5], 16)
        caller = int(entry[10], 16)
        if entry[10] != returned[10] or caller not in words:
            raise ValueError('bit-reader caller context is unpaired or absent from original source')
        source = int(entry[11])
        expected_source = 1 if caller == 0x25354 else 2 if caller == 0x25e2d else 0
        if entry[11] != returned[11] or (source != 3 and source != expected_source):
            raise ValueError('bit-reader selection group disagrees with paired caller')
        source_counts[source] += 1
        if source_counts[source] > (128 if source == 3 else 8):
            raise ValueError('bit-reader caller selection exceeds its bounded samples')
        literal = words.get(caller - 1)
        literal_request = literal & 0xff if words.get(caller) == 0xf982 and words.get(caller + 1) == 0x510c and literal is not None and literal & 0xff00 == 0xe800 else None
        row = {'raw_request': raw_request, 'caller': entry[10], 'capture_group': source, 'preceding_literal_candidate': literal_request,
               'literal_candidate_agrees': None if literal_request is None else literal_request == raw_request,
               'entry_time': float(entry[12]), 'return_time': float(returned[12]),
               'scope': 'raw context comparison, not retained operands or CPU/codec acceptance'}
        if not 0 <= raw_request <= 16 or remaining > 32 or (remaining <= 16 and entry[9] == 'unobserved') or (literal_request is not None and literal_request != raw_request):
            row['comparison'] = 'unvalidated_input_context'
        else:
            expected = reference_bit_window(remaining, int(entry[6], 16), int(entry[7], 16),
                                            int(entry[9], 16) if entry[9] != 'unobserved' else 0, raw_request)
            observed = dict(zip(('value', 'remaining', 'reservoir', 'index'),
                                (int(returned[4], 16) & 0xffff, int(returned[5], 16), int(returned[6], 16), int(returned[7], 16))))
            row.update(comparison='matches' if observed == expected and entry[8] == returned[8] else 'differs',
                       expected=expected, observed=observed, base_unchanged=entry[8] == returned[8])
        result.append(row)
    return result


def verify_playback_optional_window(text, bits, session):
    starts = list(re.finditer(r'mu4_playback_optional_window: start=02532e t=(\d+\.\d{9}) identity=Execute_entry observer_writes=0$', text, re.MULTILINE))
    ends = list(re.finditer(r'mu4_playback_optional_terminal: end=(025359|024538) samples=(\d+) capped=([01]) t=(\d+\.\d{9}) '
                            r'identity=Execute_entry observer_writes=0 audio_acceptance=0$', text, re.MULTILINE))
    selected = [row for row in bits if row['capture_group'] == 3]
    if not starts and not ends and not selected and 'mu4_playback_optional_' not in text:
        return None
    if len(starts) != 1 or len(ends) != 1 or text.count('mu4_playback_optional_window:') != 1 or text.count('mu4_playback_optional_terminal:') != 1:
        raise ValueError('missing or duplicated optional-field window')
    start, end = float(starts[0][1]), float(ends[0][4])
    if not session[0] <= start < end < session[1] or starts[0].end() >= ends[0].start():
        raise ValueError('optional-field window outside ordered first session')
    if int(ends[0][2]) != len(selected) or int(ends[0][3]) != int(len(selected) == 128) or not selected:
        raise ValueError('optional-field window sample/cap mismatch')
    if any(not start <= row['entry_time'] <= row['return_time'] <= end for row in selected):
        raise ValueError('optional-field sample outside its window')
    if selected[0]['caller'] != '02532f':
        raise ValueError('optional-field window lacks its initial one-bit call')
    if ends[0][1] == '025359' and len(selected) < 128 and selected[-1]['caller'] != '025354':
        raise ValueError('optional-field window lacks its final one-bit call')
    if ends[0][1] == '025359' and len(selected) < 128 and not any(row['caller'] == '025344' for row in selected):
        raise ValueError('optional-field window lacks its intermediate one-bit call')
    return {'start': start, 'end': end, 'end_address': ends[0][1], 'capped': len(selected) == 128,
            'calls': selected, 'scope': 'one bounded optional-field attempt, not AAC validity or full stream alignment'}


def verify_playback_gates(text, session):
    terminals = list(re.finditer(
        r'mu4_playback_gate_terminal: direction=(read|write) address=(ba4f|ba51) accesses=(\d+) samples=(\d+) '
        r'last=([0-9a-f]{4}|unobserved) selection=common_first_session_window observer_writes=0 audio_acceptance=0$',
        text, re.MULTILINE))
    rows = list(re.finditer(
        r'mu4_playback_gate_access: direction=(read|write) address=(ba4f|ba51) value=([0-9a-f]{4}) '
        r'ordinal=(\d+) access=(\d+) t=(\d+\.\d{9}) context_pc=([0-9a-f]{6}) ownership=unvalidated observer_writes=0$',
        text, re.MULTILINE))
    if not terminals and not rows and 'mu4_playback_gate_' not in text:
        return None
    expected = {(direction, address) for direction in ('read', 'write') for address in ('ba4f', 'ba51')}
    if len(terminals) != 4 or len(terminals) != text.count('mu4_playback_gate_terminal:') or len(rows) != text.count('mu4_playback_gate_access:'):
        raise ValueError('missing or malformed playback gate observations')
    reports = {(row[1], row[2]): row for row in terminals}
    if set(reports) != expected:
        raise ValueError('duplicate or missing playback gate identity')
    samples = {key: [] for key in expected}
    previous_time = max(session[0], 0.8182)
    for row in rows:
        key, ordinal, access, time = (row[1], row[2]), int(row[4]), int(row[5]), float(row[6])
        seen = samples[key]
        if ordinal != len(seen) + 1 or ordinal > 32 or access <= (seen[-1]['access'] if seen else 0):
            raise ValueError('playback gate ordinal/access mismatch')
        if not previous_time <= time <= min(session[1], 1.7) or row.end() >= reports[key].start():
            raise ValueError('playback gate time or terminal ordering mismatch')
        seen.append({'value': row[3], 'access': access, 'time': time, 'context_pc': row[7]})
        previous_time = time
    result = {}
    for key, report in reports.items():
        total, count, last = int(report[3]), int(report[4]), report[5]
        seen = samples[key]
        if count != len(seen) or not min(total, 8) <= count <= min(total, 32):
            raise ValueError('playback gate samples disagree with access totals')
        if total == 0:
            if last != 'unobserved':
                raise ValueError('unobserved gate value asserted')
        elif not seen or seen[-1]['access'] > total or last == 'unobserved' or (count < 32 and seen[-1]['value'] != last):
            raise ValueError('playback gate terminal value/access mismatch')
        result['_'.join(key)] = {'accesses': total, 'samples': seen, 'last': last,
                                'sample_cap_reached': count == 32}
    return result


def verify_playback_method(text, words, session):
    terminals = list(re.finditer(
        r'mu4_playback_method_terminal: entry=(\d+) early=(\d+) body=(\d+) returned=(\d+) result=(\d+) publish=(\d+) '
        r'opcode_mismatch=0 selection=first_verified_aa99_session identity=Execute_entry observer_writes=0 audio_acceptance=0$',
        text, re.MULTILINE))
    rows = list(re.finditer(
        r'mu4_playback_method_entry: site=([0-5]) address=([0-9a-f]{6}) opcode=([0-9a-f]{4}) ordinal=(\d+) '
        r't=(\d+\.\d{9}) identity=Execute_entry observer_writes=0$', text, re.MULTILINE))
    if not terminals and not rows and 'mu4_playback_method_' not in text:
        return None
    if len(terminals) != 1 or text.count('mu4_playback_method_terminal:') != 1 or len(rows) != text.count('mu4_playback_method_entry:'):
        raise ValueError('missing, malformed or duplicate method coverage')
    sites = ((0x23e64, 0x4a11), (0x23e84, 0xf020), (0x23e99, 0x62f8),
             (0x23ef7, 0xf4e4), (0x293da, 0x10f8), (0x293ed, 0xf982))
    counts, previous = [0] * 6, session[0]
    for row in rows:
        site, address, opcode, ordinal, time = int(row[1]), int(row[2], 16), int(row[3], 16), int(row[4]), float(row[5])
        counts[site] += 1
        if (address, opcode) != sites[site] or words.get(address) != opcode:
            raise ValueError('method coverage disagrees with original source or selected site')
        if ordinal != counts[site] or row.end() >= terminals[0].start() or not previous <= time < session[1]:
            raise ValueError('method coverage ordinal, session or terminal ordering mismatch')
        previous = time
    totals = list(map(int, terminals[0].groups()))
    if counts != [min(count, 8) for count in totals]:
        raise ValueError('method coverage samples disagree with terminal totals')
    return dict(zip(('entry', 'early', 'body', 'returned', 'result', 'publish'), totals))


def verify_playback_path(text, words, callbacks, *, kind='callback'):
    formats = {
        'callback': ('mu4_playback_path', 'before_first_d7b0_callback'),
        'restore': ('mu4_playback_restore_path', 'before_first_restore_wrapper'),
    }
    if kind not in formats:
        raise ValueError('unknown playback path kind')
    prefix, selection = formats[kind]
    terminals = list(re.finditer(
        rf'{prefix}_terminal: entries=(\d+) t=(\d+\.\d{{9}}) '
        rf'selection={selection} identity=Execute_entry observer_writes=0 audio_acceptance=0$',
        text, re.MULTILINE))
    rows = list(re.finditer(
        rf'{prefix}_entry: index=(\d+) address=([0-9a-f]{{6}}) opcode=([0-9a-f]{{4}}) '
        r'identity=Execute_entry observer_writes=0$', text, re.MULTILINE))
    if not terminals and not rows and prefix + '_' not in text:
        return []
    if len(terminals) != 1 or text.count(prefix + '_terminal:') != 1 or not callbacks:
        raise ValueError('missing or duplicated callback path terminal')
    if len(rows) != text.count(prefix + '_entry:') or not 4 <= len(rows) <= 128 or int(terminals[0][1]) != len(rows):
        raise ValueError('incomplete or excessive bounded callback path')
    if float(terminals[0][2]) != callbacks[0]['time']:
        raise ValueError('callback path terminal is not paired with first callback')
    result = []
    for index, row in enumerate(rows):
        address, opcode = int(row[2], 16), int(row[3], 16)
        if int(row[1]) != index or row.end() >= terminals[0].start() or words.get(address) != opcode:
            raise ValueError('callback path index, ordering or original source mismatch')
        result.append(f'{address:06x}/{opcode:04x}')
    if result[-4:] != callbacks[0]['history']:
        raise ValueError('callback path tail differs from immediate executed history')
    return result


def verify_playback_notify_context(text, history):
    rows = list(re.finditer(
        r'mu4_playback_notify_context: ordinal=(\d+) ar1=([0-9a-f]{4}) ar2=([0-9a-f]{4}) '
        r'a=([0-9a-f]{10}) b=([0-9a-f]{10}) t=(\d+\.\d{9}) context_only=1 observer_writes=0$',
        text, re.MULTILINE))
    if len(rows) != len(history) or len(rows) != text.count('mu4_playback_notify_context:'):
        raise ValueError('missing or malformed notify register context')
    result = []
    for ordinal, (row, entry) in enumerate(zip(rows, history), 1):
        if int(row[1]) != ordinal or float(row[6]) != entry['time']:
            raise ValueError('notify register context is not paired with its Execute entry')
        result.append(dict(zip(('ar1', 'ar2', 'a', 'b'), row.group(2, 3, 4, 5))))
    return result


def verify_playback_callback_reads(text, words, session):
    rows = list(re.finditer(
        r'mu4_playback_callback_descriptor_read: address=(d7b[0-5]) value=([0-9a-f]{4}) '
        r'ordinal=(\d+) t=(\d+\.\d{9}) context_pc=[0-9a-f]{6} '
        r'reader_identity=unvalidated observer_writes=0$', text, re.MULTILINE))
    if len(rows) != text.count('mu4_playback_callback_descriptor_read:'):
        raise ValueError('malformed callback descriptor bus observation')
    counts, previous = {}, session[0]
    for row in rows:
        address, value, ordinal, time = int(row[1], 16), int(row[2], 16), int(row[3]), float(row[4])
        counts[address] = counts.get(address, 0) + 1
        if ordinal != counts[address] or ordinal > 8 or not max(previous, 0.8182) <= time <= min(1.7, session[1]):
            raise ValueError('callback descriptor bus ordinal/session mismatch')
        if words.get(0x2707B + address - 0xD7B0) != value:
            raise ValueError('callback descriptor bus value differs from original initialization')
        previous = time
    return {f'{address:04x}': count for address, count in sorted(counts.items())}


def verify_playback_range(text, words):
    parent = verify_playback_parent(text)
    first, last = 0x200a0, 0x20cd1
    if any(address not in words for address in range(first, last + 1)):
        raise ValueError('AA99 source does not cover the complete selected word range')
    summary = list(re.finditer(
        r'mu4_playback_range_terminal: first=0200a0 last=020cd1 unique=(\d+) entries=(\d+) opcode_changes=0 '
        r'selection=first_verified_aa99_session identity=Execute_entry observer_writes=0 audio_acceptance=0$', text, re.MULTILINE))
    if len(summary) != 1 or text.count('mu4_playback_range_terminal:') != 1:
        raise ValueError('missing or invalid range coverage summary')
    rows = list(re.finditer(
        r'mu4_playback_range_entry: address=([0-9a-f]{6}) opcode=([0-9a-f]{4}) entries=(\d+) '
        r'selection=first_verified_aa99_session identity=Execute_entry observer_writes=0$', text, re.MULTILINE))
    if len(rows) != int(summary[0][1]) or len(rows) != text.count('mu4_playback_range_entry:'):
        raise ValueError('range coverage is missing or duplicating entries')
    visits = {}
    for row in rows:
        address, opcode, count = int(row[1], 16), int(row[2], 16), int(row[3])
        if not first <= address <= last or address in visits or count <= 0 or words[address] != opcode:
            raise ValueError('range entry disagrees with its address, identity or original AA99 word')
        visits[address] = {'opcode': f'{opcode:04x}', 'entries': count}
    if not visits or sum(row['entries'] for row in visits.values()) != int(summary[0][2]):
        raise ValueError('range entry totals disagree with the complete census')
    enqueue_history = verify_playback_switch_history(text, words, parent['first_session'], kind='enqueue')
    activation_history = verify_playback_switch_history(text, words, parent['first_session'], kind='activation')
    notify_history = verify_playback_switch_history(text, words, parent['first_session'], kind='notify')
    callback_history = verify_playback_switch_history(text, words, parent['first_session'], kind='callback')
    restore_history = verify_playback_switch_history(text, words, parent['first_session'], kind='restore')
    bit_context = compare_playback_bit_context(text, words, parent['first_session'])
    if activation_history and (len(activation_history) != len(enqueue_history) or any(
            a['time'] >= e['time'] for a, e in zip(activation_history, enqueue_history))):
        raise ValueError('activation history does not precede its paired enqueue entries')
    return {'parent': parent, 'source_range_words': last - first + 1,
            'switch_context_history': verify_playback_switch_history(text, words, parent['first_session']),
            'enqueue_context_history': enqueue_history,
            'activation_context_history': activation_history,
            'callback_context_history': callback_history,
            'callback_producer_path': verify_playback_path(text, words, callback_history),
            'restore_context_history': restore_history,
            'restore_producer_path': verify_playback_path(text, words, restore_history, kind='restore'),
            'method_coverage': verify_playback_method(text, words, parent['first_session']),
            'method_gate_accesses': verify_playback_gates(text, parent['first_session']),
            'bit_context_comparisons': bit_context,
            'optional_field_window': verify_playback_optional_window(text, bit_context, parent['first_session']),
            'callback_descriptor_read_samples': verify_playback_callback_reads(text, words, parent['first_session']),
            'notify_context_history': notify_history,
            'notify_raw_register_context': verify_playback_notify_context(text, notify_history),
            'visited_words': len(visits), 'execute_entries': int(summary[0][2]),
            'visits': {f'{address:06x}': row for address, row in sorted(visits.items())},
            'scope': 'complete Execute-entry counts in selected first-session AA99 word range only; '
                     'not instruction retirement, data-read ownership, subsystem semantics or decoded audio'}


def verify_recorder_input(text):
    """Check the explicit bench source and actual RX DMA reads, not encoded audio."""
    matches = list(re.finditer(
        r'mu4_recorder_ac_input: generated=(\d+),(\d+),(\d+),(\d+) '
        r'received=(\d+),(\d+),(\d+),(\d+) other=(\d+) other_owner_reads=(\d+) '
        r'shape=square periods=64,32 amplitudes=8192,4096 '
        r'ownership=actual_channel2_source_read analog_adc=0 decoded_audio=0$', text, re.MULTILINE))
    if len(matches) != 1 or text.count('mu4_recorder_ac_input:') != 1:
        raise ValueError('missing or duplicated recorder converted-input observation')
    values = [int(value) for value in matches[0].groups()]
    generated, received = values[:4], values[4:8]
    frames = generated[0] + generated[1]
    if frames < 64 or generated[2] + generated[3] != frames:
        raise ValueError('recorder converted source lacks complete stereo periods')
    for channel, period in enumerate((64, 32)):
        cycles, remainder = divmod(frames, period)
        positive = cycles * (period // 2) + min(remainder, period // 2)
        if generated[channel * 2:channel * 2 + 2] != [positive, frames - positive]:
            raise ValueError('recorder converted-source statistics violate the square fixture')
    if values[8] or not all(received) or sum(received) > sum(generated):
        raise ValueError('actual RX DMA does not establish both polarities of both converted inputs')
    return {'generated_frames': frames, 'dma_symbol_reads': received,
            'other_owner_reads': values[9], 'encoder_consumption': 'unvalidated',
            'decoded_audio': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--runtime', action='store_true', help='check explicitly hybrid service frontier')
    parser.add_argument('--resident-memory', action='store_true',
                        help='check MU4 original AA22 program/data RAM only, not handset bootstrap or boot')
    parser.add_argument('--playback-observation', action='store_true',
                        help='validate Execute/audio observation scope only, not playback acceptance')
    parser.add_argument('--playback-lifecycle', action='store_true',
                        help='validate a passive short-run lifecycle/control observation, not audio')
    parser.add_argument('--playback-dma', action='store_true',
                        help='validate actual channel-3 data reads, not TX pairing or decoded media')
    parser.add_argument('--playback-banks', action='store_true',
                        help='validate scoped data-write contexts, not writer ownership or decoded media')
    parser.add_argument('--recorder-input', action='store_true',
                        help='validate varying converted-input generation and RX DMA, not encoding')
    parser.add_argument('--playback-parent', action='store_true',
                        help='validate first-session Execute contexts, not subsystem semantics or audio')
    parser.add_argument('--playback-range', action='store_true',
                        help='validate first-session low-code counts against original AA99 words')
    parser.add_argument('--source-a00', type=Path, help='original R060 InitData required for --playback-range')
    parser.add_argument('--serial-readiness', action='store_true',
                        help='also require observed MBUSTIM queue drainage')
    parser.add_argument('--input-lifecycle', action='store_true',
                        help='also require MU4 task initialization and receiver entry, not keys')
    parser.add_argument('--input-timer', action='store_true',
                        help='also require MU4 timer arm and delivery; observe at least 16 seconds')
    parser.add_argument('--dsp-service-route', action='store_true',
                        help='require task-4 forwarding then task-8 HLE discovery ingress, not MU4')
    args = parser.parse_args()
    bench_modes = (args.resident_memory, args.playback_observation, args.playback_lifecycle, args.playback_dma, args.playback_banks, args.recorder_input, args.playback_parent, args.playback_range)
    if args.playback_range != bool(args.source_a00):
        parser.error('--source-a00 must accompany --playback-range only')
    if any(bench_modes) and (
            sum(bench_modes) > 1 or any((
                args.runtime, args.serial_readiness, args.input_lifecycle,
                args.input_timer, args.dsp_service_route))):
        parser.error('MU4 observation modes cannot be combined with other bootstrap checks')
    try:
        text = args.log.read_text(errors='replace')
        if args.playback_range:
            if __package__:
                from .noki5510_a00_inventory import uploaded_word_image
                from .mu4_clock_contract import SOURCE_SHA256
            else:
                from noki5510_a00_inventory import uploaded_word_image
                from mu4_clock_contract import SOURCE_SHA256
            source = args.source_a00.read_bytes()
            if hashlib.sha256(source).hexdigest() != SOURCE_SHA256:
                raise ValueError('range coverage requires the original R060 source')
            words = uploaded_word_image(bytes.fromhex(source.decode('ascii')), 'aa99')
            print(json.dumps(verify_playback_range(text, words), indent=2, sort_keys=True))
            return
        if args.playback_parent:
            result = verify_playback_parent(text)
            print(f'MU4 playback parent observation complete: {result}; subsystem semantics and audio remain unvalidated')
            return
        if args.recorder_input:
            result = verify_recorder_input(text)
            print(f'MU4 recorder converted-input observation complete: {result}; '
                  'encoder consumption and decoded audio remain unvalidated')
            return
        if args.playback_banks:
            result = verify_playback_banks(text)
            print(f'MU4 playback bank observation complete: {result}; '
                  'writer ownership, all-alias coverage and media validity remain unvalidated')
            return
        if args.playback_dma:
            result = verify_playback_dma(text)
            print(f'MU4 playback DMA observation complete: {result}; '
                  'TX pairing and decoded media remain unvalidated')
            return
        if args.playback_lifecycle:
            result = verify_playback_lifecycle(text)
            print(f'MU4 playback lifecycle observation complete: {result}; '
                  'worker completion cause and decoded audio remain unvalidated')
            return
        if args.playback_observation:
            result = verify_playback_observation(text)
            print(f'MU4 playback observation complete: {result}; '
                  'stream alignment and decoded audio remain unvalidated')
            return
        if args.resident_memory:
            verify_resident_memory(text)
            print('MU4 resident memory PASS: 48261 program + 3656 data words; '
                  '4 register writes and physical mapping unverified; not full boot')
            return
        verify(text, args.runtime)
        if args.serial_readiness:
            verify_serial_readiness(text)
        if args.input_lifecycle:
            verify_input_lifecycle(text)
        if args.input_timer:
            verify_input_timer(text)
        if args.dsp_service_route:
            verify_dsp_service_route(text)
    except (OSError, ValueError) as error:
        parser.exit(1, f'5510 bootstrap FAIL: {error}\n')
    print('5510 own uploads PASS; ' + ('hybrid discovery/self-test consumed, not idle boot'
                                     if args.runtime else 'missing mask 2c75 remains blocked'))


if __name__ == '__main__':
    main()

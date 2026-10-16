"""Check own-upload execution, not graphical boot or fitted mask identity."""
import argparse
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
            'din_nonzero': din_nonzero, 'tx_last_nonzero': audio['tx_last'],
            'din_last_nonzero': audio['din_last'], 'pc': playback[2]}


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
    parser.add_argument('--serial-readiness', action='store_true',
                        help='also require observed MBUSTIM queue drainage')
    parser.add_argument('--input-lifecycle', action='store_true',
                        help='also require MU4 task initialization and receiver entry, not keys')
    parser.add_argument('--input-timer', action='store_true',
                        help='also require MU4 timer arm and delivery; observe at least 16 seconds')
    parser.add_argument('--dsp-service-route', action='store_true',
                        help='require task-4 forwarding then task-8 HLE discovery ingress, not MU4')
    args = parser.parse_args()
    bench_modes = (args.resident_memory, args.playback_observation, args.playback_lifecycle)
    if any(bench_modes) and (
            sum(bench_modes) > 1 or any((
                args.runtime, args.serial_readiness, args.input_lifecycle,
                args.input_timer, args.dsp_service_route))):
        parser.error('MU4 observation modes cannot be combined with other bootstrap checks')
    try:
        text = args.log.read_text(errors='replace')
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

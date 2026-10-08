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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--runtime', action='store_true', help='check explicitly hybrid service frontier')
    parser.add_argument('--serial-readiness', action='store_true',
                        help='also require observed MBUSTIM queue drainage')
    parser.add_argument('--input-lifecycle', action='store_true',
                        help='also require MU4 task initialization and receiver entry, not keys')
    parser.add_argument('--input-timer', action='store_true',
                        help='also require MU4 timer arm and delivery; observe at least 16 seconds')
    parser.add_argument('--dsp-service-route', action='store_true',
                        help='require task-4 forwarding then task-8 HLE discovery ingress, not MU4')
    args = parser.parse_args()
    try:
        text = args.log.read_text(errors='replace')
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

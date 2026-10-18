"""Run completed-word and explicit serial-pin tests through the native backend."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

MARKER = 'C54x native backend BSP: PASS cases=3 save_replay=3 reset_cases=3 handset_audio_claim=0'
OBSERVATION = 'C54x native backend BSP observation: PASS receive_cases=2 debugger_reads=8 overrun_restore=2 normal_acks=4 handset_audio_claim=0'
RF_OBSERVATION = 'C54x native backend RF observation: PASS debugger_reads=8 ordinary_reads=3 source_restore=1 reset=1 physical_rf_claim=0'
RECEIVE_PINS = 'C54x native backend BSP receive pins: PASS words=3 partial_restore=1 reset_abort=1 vectors=3 handset_audio_claim=0'
TRANSMIT_PINS = 'C54x native backend BSP transmit pins: PASS words=5 partial_restore=1 queued_word=1 underrun_no_irq=1 shift_status=1 reset_abort=1 reset_irq=1 vectors=4 handset_audio_claim=0'
CLOCK_INPUTS = 'C54x native backend BSP clock inputs: PASS levels=4 save_replay=4 reset_inputs=1 handset_clock_claim=0'
CONTINUOUS_RX = 'C54x native backend BSP continuous RX: PASS words=5 partial_restore=1 single_sync_continuation=1 overrun_restart=1 reset_abort=1 source=4 handset_clock_claim=0'
CONTINUOUS_TX = 'C54x native backend BSP continuous TX: PASS completed_words=6 load_irqs=5 partial_restore=1 queued_boundary=1 underrun_halt=1 external_restart=1 reset_abort=1 source=5 handset_clock_claim=0'
FRAME_IGNORE = 'C54x native backend BSP frame ignore: PASS directions=2 words=4 partial_restore=1 ignored_sync=1 reset_abort=1 sources=4,5 handset_clock_claim=0'
JOINED_PINS = 'C54x native backend BSP joined pins: PASS cases=128 formats=4 polarities=4 patterns=4 phases=2 partial_restore=128 sources=4,5 handset_clock_claim=0'
CLOCKED_STORE = 'C54x native backend BSP clocked store: PASS opcode=7721 pending_without_clocks=1 echo=0aaa untimed=0 sources=4,5 handset_clock_claim=0'
STAGED_CLOCKED_STORE = 'C54x staged backend BSP clocked store: PASS opcode=7721 pending_without_clocks=1 echo=0aaa untimed=0 sources=4,5 physical_phase_claim=0'
COMPLETION = 'Fatal error: C54x backend BSP tests complete'
REQUESTS = 'C54x native backend compare requests: PASS cases=16 channels=2 pending_restore=16 armed_restore=16 overlap_restore=1 payload_preserved=1 masked_pending=1 ack_ports=1,2 reset=16 mapping_claim=0'
REQUEST_COMPLETION = 'Fatal error: C54x backend compare request tests complete'


def check_result(text, returncode, *, verbose_teardown=False, host_requests=False):
    lines = text.splitlines()
    if verbose_teardown and lines[-2:] == ['Enter sdlwindow_exit', 'Leave sdlwindow_exit']:
        lines = lines[:-2]
    if host_requests:
        if (returncode != 3 or lines.count(REQUESTS) != 1 or lines.count(REQUEST_COMPLETION) != 1
                or not lines or lines[-1] != REQUEST_COMPLETION):
            raise ValueError('missing unique slot-request storage/restoration completion')
        if any(line.startswith('Fatal error:') and line != REQUEST_COMPLETION for line in lines):
            raise ValueError('unexpected slot-request fatal error')
        return
    if returncode != 3 or lines.count(MARKER) != 1:
        raise ValueError('missing unique successful backend BSP completion')
    if lines.count(OBSERVATION) != 1:
        raise ValueError('missing unique nondestructive BDRR/overrun result')
    if lines.count(RF_OBSERVATION) != 1:
        raise ValueError('missing unique nondestructive RF input/restoration result')
    if lines.count(RECEIVE_PINS) != 1:
        raise ValueError('missing unique native receive-pin/partial-restore result')
    if lines.count(TRANSMIT_PINS) != 1:
        raise ValueError('missing unique native transmit-pin/partial-restore result')
    if lines.count(CLOCK_INPUTS) != 1:
        raise ValueError('missing unique native clock-input/restoration result')
    if lines.count(CONTINUOUS_RX) != 1:
        raise ValueError('missing unique native continuous-RX/restoration result')
    if lines.count(CONTINUOUS_TX) != 1:
        raise ValueError('missing unique native continuous-TX/restoration result')
    if lines.count(FRAME_IGNORE) != 1:
        raise ValueError('missing unique native frame-ignore/restoration result')
    if lines.count(JOINED_PINS) != 1:
        raise ValueError('missing unique native joined-pin/restoration result')
    if lines.count(CLOCKED_STORE) != 1:
        raise ValueError('missing unique native clocked-store result')
    if lines.count(STAGED_CLOCKED_STORE) != 1:
        raise ValueError('missing unique staged clocked-store result')
    if not lines or lines[-1] != COMPLETION:
        raise ValueError('missing terminal backend BSP completion')
    if any(line.startswith('Fatal error:') and line != COMPLETION for line in lines):
        raise ValueError('unexpected backend BSP fatal error')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mame', type=Path, required=True)
    parser.add_argument('--output-root', type=Path, default=Path('tmp'))
    parser.add_argument('--coverage', action='store_true', help='retain native opcode/decoder records with verbose teardown')
    parser.add_argument('--host-requests', action='store_true', help='check slot pending storage, masking, acceptance and save/load, not physical source mapping')
    args = parser.parse_args()
    args.output_root.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix='c54x-backend-bsp-', dir=args.output_root.resolve()))
    command = [str(args.mame.resolve()), 'tms54bsp', '-video', 'none',
               '-sound', 'none', '-nothrottle', '-noreadconfig', '-log',
               '-seconds_to_run', '1']
    if args.coverage:
        command.append('-verbose')
    if args.host_requests:
        command.extend(['-bios', 'requests'])
    print(f'Native backend BSP evidence: {run}', flush=True)
    with (run / 'console.log').open('w') as output:
        result = subprocess.run(command, cwd=run, stdout=output, stderr=subprocess.STDOUT)
    text = (run / 'console.log').read_text()
    print(text, end='')
    try:
        check_result(text, result.returncode, verbose_teardown=args.coverage, host_requests=args.host_requests)
    except ValueError as error:
        print(f'Native backend BSP: FAIL: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

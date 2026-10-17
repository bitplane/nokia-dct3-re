"""Native key-tone and boot-loopback experiments with explicit laboratory clocks."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from tools.c54x_rom4_pcm_clock_check import check, check_wire, check_restore, check_clocked_boot, demand_transitions
from tools.c54x_opcode_coverage import summarize, require_extension_capture, require_extension_decoding, decoded_boundaries
from tools.c54x_rom4_codec_contract import check as check_codec, check_mcu_speech_field
from tools.c54x_rom4_rf_delivery_check import check_observer_health
from tools.run_c54x_rom4_rf_delivery import input_provenance


def paired_screen(first, reference):
    from PIL import Image
    with Image.open(first) as first_image, Image.open(reference) as reference_image:
        actual, expected = first_image.convert('RGB'), reference_image.convert('RGB')
    if actual.size != (84, 48) or expected.size != actual.size or all(low == high for low, high in actual.getextrema()):
        raise ValueError('missing nonblank 84x48 native handset screen')
    if actual.tobytes() != expected.tobytes():
        raise ValueError('clocked boot screen differs from its paired serial control')
    return dict(width=84, height=48, paired_pixels_equal=True,
                rgb_sha256=hashlib.sha256(actual.tobytes()).hexdigest())


def instruction_report(text):
    observed = summarize(text)
    require_extension_capture(observed)
    require_extension_decoding(observed)
    if set(observed['decoded']) != set(observed['first_pc']) or decoded_boundaries(observed) != set(observed['first_pc']):
        raise ValueError('missing or unsupported native instruction decoder records')
    return dict(opcodes=observed['opcodes'], groups=observed['high_byte_groups'],
                sha256=observed['set_sha256'], independent_arithmetic_assertion_claim=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mame', type=Path, required=True)
    parser.add_argument('--rompath', type=Path, required=True)
    parser.add_argument('--output-root', type=Path, default=Path('tmp'))
    parser.add_argument('--restore', action='store_true', help='also compare an active native tone suffix after save/load')
    parser.add_argument('--wire', action='store_true', help='also test explicit 1 MHz edges with assumed one-clock sync and RX/TX fan-out')
    parser.add_argument('--coverage', action='store_true', help='retain native opcode/extension observations using MAME verbose teardown')
    parser.add_argument('--clocked-loopback', action='store_true', help='also test pin-clocked boot loopback with the untimed word path disabled')
    args = parser.parse_args()
    repo = Path(__file__).resolve().parent.parent
    args.output_root.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='native-pcm-clock-', dir=args.output_root.resolve()))
    observer = repo / 'tools/c54x_rom4_tone_observe.lua'
    generator = repo / 'tools/make_5110_eeprom_profile.py'
    inputs = dict(binary=args.mame, observer=observer, generator=generator,
                  codec_observer=repo / 'tools/c54x_rom4_codec_observe.lua',
                  input_fixture=repo / 'mame_nokia_dct3_input_exerciser.lua')
    restore_observer = repo / 'tools/c54x_rom4_pcm_restore.lua'
    if args.restore:
        inputs['restore_observer'] = restore_observer
    for name in ('5110f530.fls', 'nse-1.bin', 'nse1_rom4_dsp_program.bin', 'nse1_rom4_dsp_data.bin'):
        inputs[name] = args.rompath / 'noki5110' / name
    check_codec(inputs['nse1_rom4_dsp_program.bin'].read_bytes())
    check_mcu_speech_field(inputs['5110f530.fls'].read_bytes())
    (root / 'inputs.json').write_text(json.dumps(input_provenance(inputs), indent=2) + '\n')
    print(f'Native external-frame evidence: {root}', flush=True)
    results = []
    cases = [('control', False, 'noki5110'), ('clock', True, 'nse1pcm')]
    if args.wire:
        cases.append(('wire', True, 'nse1bit'))
    if args.clocked_loopback:
        cases.append(('pin_boot', True, 'nse1pin'))
    if args.restore:
        cases.append(('restore', True, 'nse1pcm'))
        if args.wire:
            cases.append(('wire_restore', True, 'nse1bit'))
        if args.clocked_loopback:
            cases.append(('pin_restore', True, 'nse1pin'))
    for label, clock, machine in cases:
        restoring = label.endswith('restore')
        case = root / label
        nvram = case / 'nvram' / machine
        nvram.mkdir(parents=True)
        # Same NSE-1 inputs on both sides; no sibling provisioning.
        subprocess.run([sys.executable, str(generator), '--eeprom', str(inputs['nse-1.bin']),
                        '--flash', str(inputs['5110f530.fls']), '--output', str(nvram / 'eeprom')], check=True)
        env = {key: value for key, value in os.environ.items() if not key.startswith('NOKIA_DCT3_')}
        env.update(NOKIA_DCT3_POST_READY_KEYS='' if restoring else '2', NOKIA_DCT3_POST_READY_KEY_DELAY_MS='8000',
                   NOKIA_DCT3_POST_READY_KEY_DURATION_MS='220')
        command = [str(args.mame.resolve()), machine, '-rompath', str(args.rompath.resolve()),
                   '-nvram_directory', str(case / 'nvram'), '-noreadconfig', '-video', 'none',
                   '-sound', 'none', '-log', '-skip_gameinfo', '-nothrottle', '-seconds_to_run', '12',
                   '-autoboot_script', str(restore_observer if restoring else observer)]
        if args.coverage:
            command.append('-verbose')
        (case / 'invocation.json').write_text(json.dumps(dict(command=command, cwd=str(case),
            fixture_env={key: value for key, value in env.items() if key.startswith('NOKIA_DCT3_')},
            generated_storage=input_provenance({'eeprom': nvram / 'eeprom'})), indent=2) + '\n')
        with (case / 'console.log').open('w') as output:
            completed = subprocess.run(command, cwd=case, env=env, stdout=output, stderr=subprocess.STDOUT)
        try:
            if completed.returncode:
                raise ValueError(f'MAME exit status {completed.returncode}')
            check_observer_health((case / 'console.log').read_text())
            text = (case / 'error.log').read_text()
            if restoring:
                result = check_restore(text, wire=machine in ('nse1bit', 'nse1pin'))
            else:
                result = check_wire(text) if machine in ('nse1bit', 'nse1pin') else check(text, clock=clock)
            if machine == 'nse1pin':
                result['clocked_boot'] = check_clocked_boot(text)
                reference_machine = 'nse1bit' if args.wire else 'nse1pcm'
                reference_case = ('wire_restore' if args.wire else 'restore') if restoring else ('wire' if args.wire else 'clock')
                result['screen'] = paired_screen(case / 'snap' / machine / '0000.png',
                    root / reference_case / 'snap' / reference_machine / '0000.png')
            result['codec_demand'] = demand_transitions(text)
            if args.coverage:
                result['instruction_observations'] = instruction_report(text)
            result['status'] = 'pass'
        except (OSError, ValueError) as error:
            result = dict(clock=clock, status='failed', error=str(error))
        results.append(result)
        result['case'] = label
        (root / 'summary.json').write_text(json.dumps(dict(schema=1, cases=results,
            handset_clock_claim=False, native_call_speech_claim=False), indent=2) + '\n')
        print(f"Native serial {label}: {result['status']}", flush=True)
    return 0 if all(result['status'] == 'pass' for result in results) else 1


if __name__ == '__main__':
    raise SystemExit(main())

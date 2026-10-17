"""Paired native key-tone experiment with explicit external completed frames."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from tools.c54x_rom4_pcm_clock_check import check, check_restore, demand_transitions
from tools.c54x_rom4_codec_contract import check as check_codec, check_mcu_speech_field
from tools.c54x_rom4_rf_delivery_check import check_observer_health
from tools.run_c54x_rom4_rf_delivery import input_provenance


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mame', type=Path, required=True)
    parser.add_argument('--rompath', type=Path, required=True)
    parser.add_argument('--output-root', type=Path, default=Path('tmp'))
    parser.add_argument('--restore', action='store_true', help='also compare an active native tone suffix after save/load')
    args = parser.parse_args()
    repo = Path(__file__).resolve().parent.parent
    args.output_root.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='native-pcm-clock-', dir=args.output_root.resolve()))
    observer = repo / 'tools/c54x_rom4_tone_observe.lua'
    generator = repo / 'tools/make_5110_eeprom_profile.py'
    inputs = dict(binary=args.mame, observer=observer, generator=generator,
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
    if args.restore:
        cases.append(('restore', True, 'nse1pcm'))
    for label, clock, machine in cases:
        case = root / label
        nvram = case / 'nvram' / machine
        nvram.mkdir(parents=True)
        # Same NSE-1 inputs on both sides; no sibling provisioning.
        subprocess.run([sys.executable, str(generator), '--eeprom', str(inputs['nse-1.bin']),
                        '--flash', str(inputs['5110f530.fls']), '--output', str(nvram / 'eeprom')], check=True)
        env = {key: value for key, value in os.environ.items() if not key.startswith('NOKIA_DCT3_')}
        env.update(NOKIA_DCT3_POST_READY_KEYS='' if label == 'restore' else '2', NOKIA_DCT3_POST_READY_KEY_DELAY_MS='8000',
                   NOKIA_DCT3_POST_READY_KEY_DURATION_MS='220')
        command = [str(args.mame.resolve()), machine, '-rompath', str(args.rompath.resolve()),
                   '-nvram_directory', str(case / 'nvram'), '-noreadconfig', '-video', 'none',
                   '-sound', 'none', '-log', '-skip_gameinfo', '-nothrottle', '-seconds_to_run', '12',
                   '-autoboot_script', str(restore_observer if label == 'restore' else observer)]
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
            result = check_restore(text) if label == 'restore' else check(text, clock=clock)
            result['codec_demand'] = demand_transitions(text)
            result['status'] = 'pass'
        except (OSError, ValueError) as error:
            result = dict(clock=clock, status='failed', error=str(error))
        results.append(result)
        result['case'] = label
        (root / 'summary.json').write_text(json.dumps(dict(schema=1, cases=results,
            handset_clock_claim=False, native_call_speech_claim=False), indent=2) + '\n')
        print(f"Native completed-frame {label}: {result['status']}", flush=True)
    return 0 if all(result['status'] == 'pass' for result in results) else 1


if __name__ == '__main__':
    raise SystemExit(main())

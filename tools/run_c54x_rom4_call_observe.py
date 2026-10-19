#!/usr/bin/env python3
"""Capture physical native call attempts, not connected-call acceptance."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

try:
    from tools.run_c54x_rom4_rf_delivery import input_provenance, MACHINES
    from tools.c54x_rom4_call_observe_check import inspect
except ModuleNotFoundError:
    from run_c54x_rom4_rf_delivery import input_provenance, MACHINES
    from c54x_rom4_call_observe_check import inspect


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--machine', choices=tuple(MACHINES), default='nse1astd')
    parser.add_argument('--keys', default='1,1,2,navi', help='empty string for no-input control')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    observer = root / 'tools/c54x_rom4_call_observe.lua'
    run = Path(tempfile.mkdtemp(prefix='native-call-', dir=root / 'tmp'))
    nv = run / 'nvram' / args.machine
    nv.mkdir(parents=True)
    (run / 'snap').mkdir()
    generator = root / 'tools/make_5110_eeprom_profile.py'
    subprocess.run([sys.executable, str(generator), '--eeprom',
                    str(root / 'roms/noki5110/nse-1.bin'), '--flash',
                    str(root / 'roms/noki5110/5110f530.fls'), '--output',
                    str(nv / 'eeprom')], check=True)
    paths = dict(binary=root / 'mame/mame', observer=observer, generator=generator,
                 input_harness=root / 'mame_nokia_dct3_input_exerciser.lua',
                 initial_eeprom=nv / 'eeprom')
    for name in ('nse1_rom4_dsp_program.bin', 'nse1_rom4_dsp_data.bin',
                 '5110f530.fls', 'nse-1.bin', 'nse1_rom4_boot.bin'):
        paths['rom:' + name] = root / 'mame/roms/noki5110' / name
    (run / 'inputs.json').write_text(json.dumps(dict(
        machine=args.machine, keys=args.keys, fresh_storage=True,
        connected_call_claim=False, native_speech_claim=False,
        inputs=input_provenance(paths)), indent=2) + '\n')
    # Do not inherit fixture injections or host-backend configuration.
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(('NOKIA_DCT3_', 'NOKI3210_'))}
    env.update(NOKIA_DCT3_POST_READY_KEYS=args.keys,
               NOKIA_DCT3_POST_READY_KEY_DELAY_MS='8000',
               NOKIA_DCT3_POST_READY_KEY_DURATION_MS='220',
               NOKIA_DCT3_POST_READY_KEY_GAP_MS='220',
               NOKIA_DCT3_SNAPSHOT_DIR=str(run / 'snap'))
    command = [str(paths['binary']), args.machine, '-rompath', str(root / 'mame/roms'),
               '-nvram_directory', str(run / 'nvram'), '-video', 'none', '-sound',
               'none', '-log', '-debug', '-debugger', 'none', '-nothrottle',
               '-seconds_to_run', '18', '-autoboot_script', str(observer)]
    (run / 'invocation.json').write_text(json.dumps(command, indent=2) + '\n')
    print(run, flush=True)
    with (run / 'console.log').open('w') as stream:
        result = subprocess.run(command, cwd=run, env=env, stdout=stream,
                                stderr=subprocess.STDOUT)
    try:
        if result.returncode:
            raise ValueError(f'MAME exited {result.returncode}')
        report = inspect((run / 'error.log').read_text(),
                         (run / 'console.log').read_text(), args.machine)
    except (ValueError, OSError) as error:
        (run / 'summary.json').write_text(json.dumps(dict(
            observation_complete=False, error=str(error)), indent=2) + '\n')
        print(f'Invalid observation: {error}', file=sys.stderr)
        return 1
    (run / 'summary.json').write_text(json.dumps(report, indent=2) + '\n')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

"""Run native descriptor-driven compare evidence without RF or state injection."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from tools.c54x_rom4_codec_contract import check as check_program
from tools.c54x_rom4_transfer_check import check
from tools.run_c54x_rom4_crc import file_identity


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mame', type=Path, required=True)
    parser.add_argument('--rompath', type=Path, required=True)
    parser.add_argument('--output-root', type=Path, default=Path('tmp'))
    parser.add_argument('--seconds', type=int, choices=(8, 30), default=8)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    args.output_root.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix='native-transfer-', dir=args.output_root.resolve()))
    print(f'Native transfer evidence: {run}', flush=True)
    program = args.rompath / 'noki5110/nse1_rom4_dsp_program.bin'
    check_program(program.read_bytes())
    seed = run / 'nvram/noki5110/eeprom'
    seed.parent.mkdir(parents=True)
    generator = repo / 'tools/make_5110_eeprom_profile.py'
    subprocess.run([sys.executable, str(generator), '--eeprom', str(repo / 'roms/noki5110/nse-1.bin'),
                    '--flash', str(repo / 'roms/noki5110/5110f530.fls'), '--output', str(seed)], check=True)
    observer = repo / 'tools/c54x_rom4_transfer_observe.lua'
    command = [str(args.mame.resolve()), 'noki5110', '-rompath', str(args.rompath.resolve()),
               '-nvram_directory', str(run / 'nvram'), '-noreadconfig', '-video', 'none',
               '-sound', 'none', '-log', '-skip_gameinfo', '-nothrottle', '-seconds_to_run', str(args.seconds),
               '-autoboot_script', str(observer)]
    inputs = [args.mame, observer, generator, Path(__file__),
              repo / 'tools/c54x_rom4_transfer_check.py', repo / 'tools/c54x_rom4_codec_contract.py', seed]
    inputs += [args.rompath / 'noki5110' / name for name in
               ('5110f530.fls', 'nse-1.bin', 'nse1_rom4_dsp_program.bin', 'nse1_rom4_dsp_data.bin')]
    evidence = dict(command=command, inputs=[file_identity(path) for path in inputs],
                    scope='native timed descriptor consumption; physical timing and RF acquisition unproved')
    (run / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    environment = {key: value for key, value in os.environ.items() if not key.startswith('NOKIA_DCT3_')}
    with (run / 'console.log').open('w') as output:
        result = subprocess.run(command, cwd=run, env=environment, stdout=output, stderr=subprocess.STDOUT)
    console = (run / 'console.log').read_text()
    if result.returncode or any(token in console for token in ('Lua error', 'runtime error', 'Fatal error:')):
        raise ValueError('native transfer run failed; see retained console')
    report = check((run / 'error.log').read_text(), minimum_seconds=args.seconds)
    (run / 'comparison.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f"Native descriptor transfers: PASS bursts={len(report['bursts'])} queued_pairs={report['queued_pairs']} physical_timing_claim=0 acquisition_claim=0")
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print(f'Native descriptor transfers: FAIL: {error}', file=sys.stderr)
        raise SystemExit(1)

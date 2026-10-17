"""Capture native 5110 keypad-tone Off/three-level physical-input controls."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

try:
    from .c54x_rom4_tone_levels_check import check_level, check_frame, check_host_writers, check_private_shadows
    from .c54x_rom4_codec_contract import check, check_mcu_speech_field, decode_host_dispatch_mask, census_mcu_host_interface
except ImportError:
    from c54x_rom4_tone_levels_check import check_level, check_frame, check_host_writers, check_private_shadows
    from c54x_rom4_codec_contract import check, check_mcu_speech_field, decode_host_dispatch_mask, census_mcu_host_interface


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mame', type=Path, required=True)
    parser.add_argument('--rompath', type=Path, required=True)
    parser.add_argument('--output-root', type=Path, default=Path('tmp'))
    args = parser.parse_args()
    repo = Path(__file__).resolve().parent.parent
    program = (args.rompath / 'noki5110/nse1_rom4_dsp_program.bin').read_bytes()
    check(program)
    mcu_image = (args.rompath / 'noki5110/5110f530.fls').read_bytes()
    check_mcu_speech_field(mcu_image)
    print(f'Reviewed ISR predicate union: {decode_host_dispatch_mask(program):04x}; hardware acknowledgement unresolved', flush=True)
    args.output_root.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='rom4-tone-levels-', dir=args.output_root.resolve()))
    print(f'Native tone-level evidence: {root}', flush=True)
    host_report = census_mcu_host_interface(mcu_image)
    (root / 'host_mmio_census.json').write_text(json.dumps(host_report, indent=2) + '\n')
    if not {0x271764, 0x272048, 0x2721d2} <= {item['pc'] for item in host_report['accesses']}:
        raise ValueError('host MMIO census lost a reviewed direct writer')
    for option, name in enumerate(('off', 'low', 'medium', 'high'), 1):
        run = root / name
        nvram = run / 'nvram' / 'noki5110'
        nvram.mkdir(parents=True)
        subprocess.run([sys.executable, str(repo / 'tools/make_5110_eeprom_profile.py'),
                        '--eeprom', str(repo / 'roms/noki5110/nse-1.bin'),
                        '--flash', str(repo / 'roms/noki5110/5110f530.fls'),
                        '--output', str(nvram / 'eeprom')], check=True)
        env = {key: value for key, value in os.environ.items()
               if not key.startswith('NOKIA_DCT3_')}
        env.update(NOKIA_DCT3_POST_READY_KEYS=f'menu,9,5,{option},clear,clear,clear,2',
                   NOKIA_DCT3_POST_READY_KEY_DELAY_MS='8000',
                   NOKIA_DCT3_POST_READY_KEY_DURATION_MS='220',
                   NOKIA_DCT3_POST_READY_KEY_GAP_MS='200',
                   NOKIA_DCT3_POST_READY_CAPTURE_DELAY_MS='1500',
                   NOKIA_DCT3_SNAPSHOT_DIR=str(run / 'snap'))
        (run / 'snap').mkdir()
        with (run / 'console.log').open('w') as output:
            result = subprocess.run([
                str(args.mame.resolve()), 'noki5110', '-rompath', str(args.rompath.resolve()),
                '-nvram_directory', str(run / 'nvram'), '-noreadconfig',
                '-video', 'none', '-sound', 'none', '-log', '-skip_gameinfo',
                '-nothrottle', '-seconds_to_run', '15', '-autoboot_script',
                str(repo / 'tools/c54x_rom4_tone_observe.lua')],
                env=env, cwd=run, stdout=output, stderr=subprocess.STDOUT)
        if result.returncode:
            raise ValueError(f'{name}: MAME exited {result.returncode}; see {run}')
        text = (run / 'error.log').read_text()
        check_level(text, option)
        check_host_writers(text, host_report)
        shadow_report = check_private_shadows(text)
        (run / 'private_shadows.json').write_text(json.dumps(shadow_report, indent=2) + '\n')
        check_frame(run / 'snap/noki5110/0000.png')
        print(f'Native tone setting {name}: PASS (control only; no native audio claim)', flush=True)
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print(f'Native tone levels: FAIL: {error}', file=sys.stderr)
        raise SystemExit(1)

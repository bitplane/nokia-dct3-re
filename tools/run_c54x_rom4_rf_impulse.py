#!/usr/bin/env python3
"""Run isolated native RF-port impulse cases; no acquisition claim."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile


COEFFICIENTS = (0x0387, 0x11a3, 0x2ad6, 0x4000,
                0x4000, 0x2ad6, 0x11a3, 0x0387)
INPUT = re.compile(r"rom4_rf_impulse_input: index=(\d+) pc=([0-9a-f]{4}) value=([0-9a-f]{4})")
OUTPUT = re.compile(r"rom4_rf_impulse_output: index=(\d+) pc=([0-9a-f]{4}) address=([0-9a-f]{4}) value=([0-9a-f]{4})")


def mixed_words():
    return [(((index * 7919 + 1237) % 4096) - 2048) & 0xffff for index in range(32)]


def expected_words(inputs):
    if len(inputs) != 32 or any(not 0 <= word <= 0xffff for word in inputs):
        raise ValueError('requires 32 unsigned 16-bit input words')
    products = [0] * 48
    for index, word in enumerate(inputs):
        signed = word if word < 0x8000 else word - 0x10000
        first = 2 * (index // 4) + 1 - index % 2
        phase = 1 - (index % 4) // 2
        for tap in range(4):
            products[first + tap * 2] += signed * COEFFICIENTS[phase + tap * 2]
    # Round the summed accumulator, not each isolated impulse contribution.
    return [((value + 0x8000) >> 16) & 0xffff for value in products]


def expected(index, amplitude):
    if not 0 <= index < 32:
        raise ValueError('impulse index must be in 0..31')
    inputs = [0] * 32
    inputs[index] = amplitude
    return expected_words(inputs)


def check(text, index, amplitude, mixed=False):
    inputs = INPUT.findall(text)
    outputs = OUTPUT.findall(text)
    if len(inputs) != 32 or len(outputs) != 48:
        raise ValueError("missing complete input/output capture")
    wanted_inputs = mixed_words() if mixed else [amplitude if number == index else 0 for number in range(32)]
    for number, (position, pc, value) in enumerate(inputs):
        if int(position) != number or int(pc, 16) != (0x330c, 0x3312, 0x3318, 0x331e)[number % 4]:
            raise ValueError(f"input alignment changed at {number}")
        if int(value, 16) != wanted_inputs[number]:
            raise ValueError(f"input value changed at {number}")
    for number, ((position, pc, address, value), wanted) in enumerate(zip(outputs, expected_words(wanted_inputs))):
        if int(position) != number or int(pc, 16) != 0x3322 + number % 2 or int(address, 16) != 0x1a48 + number % 16:
            raise ValueError(f"output alignment changed at {number}")
        if int(value, 16) != wanted:
            raise ValueError(f"output {number}: actual={value} expected={wanted:04x}")
    marker = ('rom4_rf_mixed_summary: samples=32' if mixed else
              f"rom4_rf_impulse_summary: impulse={index} amplitude={amplitude:04x}")
    if text.count(marker) != 1:
        raise ValueError("missing or duplicate case summary")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mame", type=Path, required=True)
    parser.add_argument("--rompath", type=Path, required=True)
    parser.add_argument('--mixed', action='store_true',
                        help='check overlapping bounded nonzero words instead of isolated impulses')
    parser.add_argument("--output-root", type=Path,
                        default=Path(__file__).resolve().parents[1] / 'tmp')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    args.output_root.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix="run_rom4_rf_impulse_", dir=args.output_root))
    print(f"RF impulse evidence: {run}", flush=True)
    input_paths = {'binary': args.mame, 'observer': root / 'tools/c54x_rom4_rf_impulse.lua',
                   'reference': Path(__file__).resolve(),
                   'generator': root / 'tools/make_5110_eeprom_profile.py',
                   'provisioning_flash': root / 'roms/noki5110/5110f530.fls',
                   'provisioning_eeprom': root / 'roms/noki5110/nse-1.bin'}
    for name in ('nse1_rom4_dsp_program.bin', 'nse1_rom4_dsp_data.bin',
                 '5110f530.fls', 'nse-1.bin', 'nse1_rom4_boot.bin'):
        input_paths['rom:' + name] = args.rompath / 'noki5110' / name
    identities = {}
    for name, path in input_paths.items():
        path = path.resolve()
        identity = {'path': str(path), 'present': path.is_file()}
        if identity['present']:
            with path.open('rb') as source:
                identity['sha256'] = hashlib.file_digest(source, 'sha256').hexdigest()
            identity['bytes'] = path.stat().st_size
        identities[name] = identity
    (run / 'inputs.json').write_text(json.dumps({
        'schema': 1, 'inputs': identities, 'mixed': args.mixed,
        'physical_iq_claim': False, 'acquisition_claim': False}, indent=2) + '\n')
    cases = 0
    for amplitude in ((0,) if args.mixed else (0x4000, 0xc000)):
        for index in (range(1) if args.mixed else range(32)):
            case = run / f"{amplitude:04x}_{index:02d}"
            nvram = case / "nvram" / "noki5110"
            nvram.mkdir(parents=True)
            subprocess.run([str(root / ".venv/bin/python"), str(root / "tools/make_5110_eeprom_profile.py"),
                            "--eeprom", str(root / "roms/noki5110/nse-1.bin"),
                            "--flash", str(root / "roms/noki5110/5110f530.fls"),
                            "--output", str(nvram / "eeprom")], check=True, stdout=subprocess.DEVNULL)
            env = dict(os.environ, ROM4_RF_IMPULSE_INDEX=str(index), ROM4_RF_IMPULSE_VALUE=str(amplitude),
                       ROM4_RF_IMPULSE_TIME="1.6402")
            if args.mixed:
                env['ROM4_RF_MIXED_WORDS'] = '1'
            else:
                env.pop('ROM4_RF_MIXED_WORDS', None)
            command = [str(args.mame.resolve()), "noki5110", "-rompath", str(args.rompath.resolve()),
                       "-nvram_directory", str(case / "nvram"), "-video", "none", "-sound", "none",
                       "-log", "-skip_gameinfo", "-nothrottle", "-seconds_to_run", "8",
                       "-autoboot_script", str(root / "tools/c54x_rom4_rf_impulse.lua")]
            (case / 'invocation.json').write_text(json.dumps({
                'command': command, 'cwd': str(case.resolve()),
                'stimulus': 'mixed' if args.mixed else 'impulse',
                'input_words': mixed_words() if args.mixed else
                    [amplitude if number == index else 0 for number in range(32)],
                'generated_eeprom_sha256': hashlib.sha256((nvram / 'eeprom').read_bytes()).hexdigest(),
            }, indent=2) + '\n')
            with (case / "console.log").open("w") as console:
                subprocess.run(command, cwd=case, env=env, stdout=console, stderr=subprocess.STDOUT, check=True)
            check((case / "error.log").read_text(), index, amplitude, mixed=args.mixed)
            cases += 1
            print('RF mixed convolution case: PASS words=32 outputs=48' if args.mixed else
                  f"RF impulse case: PASS index={index} amplitude={amplitude:04x}", flush=True)
    print(f"ROM4 RF {'mixed convolution' if args.mixed else 'impulse'}: PASS cases={cases} "
          'physical_iq_claim=0 acquisition_claim=0')


if __name__ == "__main__":
    main()

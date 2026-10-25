"""Run fresh own NSE-2 inputs to the graphical failure boundary; no DSP replies."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nse2_package_check import inspect
from tools.nse2_boot_check import NV_BLOCKS, verify, verify_record, verify_upload
from tools.process_runner import run_emulator


def checksum_fixture(*, record_checksum=False):
    """Diagnostic integrity only: no identity, calibration or lock provisioning."""
    image = bytearray(b'\xff' * 8192)
    for start, length, stored, _ in NV_BLOCKS:
        value = (~(sum(image[start:start + length]) & 0xffff)) & 0xffffffff
        image[stored:stored + 4] = value.to_bytes(4, 'big')
    if record_checksum:
        value = (sum(image[0x240:0x31e]) - sum(image[0x274:0x276])) & 0xffff
        image[0x31e:0x320] = value.to_bytes(2, 'big')
    return bytes(image)


def prepare(root, run, *, checksum_only=False, record_checksum=False):
    if run.exists():
        raise ValueError('NSE-2 boundary requires a new run directory')
    flash, report = inspect((root / 'roms/archive-dct3-packages/nse2_553.zip').read_bytes())
    comparison, other = inspect((root / 'roms/archive-dct3-packages/nse2_553.exe').read_bytes())
    if flash != comparison:
        raise ValueError('NSE-2 qualified wrappers differ')
    for directory in ('roms/noki5110i', 'nvram', 'cfg'):
        (run / directory).mkdir(parents=True, exist_ok=True)
    (run / 'roms/noki5110i/5110i_5.53a.fls').write_bytes(flash)
    report.update(comparison_archive_sha256=other['archive_sha256'],
                  initial_eeprom='absent dump; erased device region',
                  dsp_backend='silent; no mask or completion supplied',
                  boot_rom='shared minimal flash-entry HLE',
                  flash_part='capacity-compatible TE28F160 trial; fitted identity unknown')
    if checksum_only or record_checksum:
        image = checksum_fixture(record_checksum=record_checksum)
        path = run / 'nvram/noki5110i/eeprom'
        path.parent.mkdir()
        path.write_bytes(image)
        report.update(initial_eeprom='diagnostic erased payload with five own-ROM checksum words',
                      eeprom_sha256=hashlib.sha256(image).hexdigest(),
                      authentic_provisioning=False)
        if record_checksum:
            report['initial_eeprom'] = 'diagnostic erased payload with five block checksums and own 031e record checksum'
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--mame', type=Path)
    experiment = parser.add_mutually_exclusive_group()
    experiment.add_argument('--checksum-only', action='store_true',
                        help='separate NV integrity experiment; not a usable-boot gate')
    experiment.add_argument('--record-checksum-only', action='store_true',
                            help='also repair own 031e record checksum; no payload provisioning')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    mame = (args.mame or root / 'mame/mame').resolve()
    try:
        if not mame.is_file():
            raise ValueError('missing MAME binary')
        integrity = args.checksum_only or args.record_checksum_only
        report = prepare(root, run, checksum_only=integrity,
                         record_checksum=args.record_checksum_only)
        command = [str(mame), 'noki5110i', '-rompath', str(run / 'roms'),
                   '-nvram_directory', str(run / 'nvram'), '-cfg_directory', str(run / 'cfg'),
                   '-video', 'none', '-sound', 'none', '-joystickprovider', 'none',
                   '-midiprovider', 'none', '-nothrottle', '-noreadconfig', '-log', '-verbose',
                   '-seconds_to_run', '8', '-autoboot_delay', '0',
                   '-autoboot_script', str(root / 'tools/nse2_boot_observe.lua')]
        with mame.open('rb') as stream:
            report['mame_sha256'] = hashlib.file_digest(stream, 'sha256').hexdigest()
        report['command'] = command
        (run / 'inputs.json').write_text(json.dumps(report, indent=2) + '\n')
        with (run / 'console.log').open('w') as console:
            returncode = run_emulator(command, run, console)
        if returncode != 0:
            raise ValueError(f'NSE-2 MAME exited {returncode}')
        console = (run / 'console.log').read_text(errors='replace')
        if 'LUA ERROR' in console or 'Error in' in console:
            raise ValueError('NSE-2 observer failed')
        with Image.open(run / 'snap/nse2_7.png') as frame:
            text = (run / 'error.log').read_text(errors='replace')
            result = verify(text, frame.convert('L').tobytes(), frame.size,
                            checksum_only=integrity)
            verifier = report['loader']['entries'][25]
            source = int(verifier['descriptor'], 16) - 0x200000 + 12
            flash = (run / 'roms/noki5110i/5110i_5.53a.fls').read_bytes()
            result['initial_upload'] = verify_upload(text, flash[source:source + 400])
            if args.record_checksum_only:
                verify_record(text)
                result['record_checksum_selftest_passed'] = True
            if integrity:
                result.update(nv_integrity_selftest_passed=True, authentic_provisioning=False,
                              frame_sha256=hashlib.sha256(frame.convert('L').tobytes()).hexdigest())
        (run / 'acceptance.json').write_text(json.dumps(result, indent=2) + '\n')
        print('NSE-2 NV integrity experiment PASS; usable boot remains unproved' if integrity
              else 'NSE-2 graphical failure boundary PASS; usable boot, keys and DSP remain unproved')
        print('Evidence:', run)
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSE-2 boundary FAIL: {error}\n')


if __name__ == '__main__':
    main()

"""Observe own NSE-2 boot with explicit acquired NSE-1 ROM4 compatibility inputs."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.run_nse2_boundary import prepare
from tools.nse2_native_check import verify_native, verify_menu_probe, verify_nominal_voltage


MASK_INPUTS = (
    ('nse1_rom4_dsp_program.bin', 0x1fffe, 'a05a1e96a8c36ec5a47e1ea059d15afa54ca5739'),
    ('nse1_rom4_dsp_data.bin', 0x20000, '024c7f970f4ef754d3e90471de48a167515f930d'),
)


def prepare_native(root, run, *, record_checksum=False, nominal_vbat=False):
    if run.exists():
        raise ValueError('NSE-2 native experiment requires a new run directory')
    masks = {}
    for name, size, expected in MASK_INPUTS:
        data = (root / 'roms/noki5110' / name).read_bytes()
        if len(data) != size or hashlib.sha1(data).hexdigest() != expected:
            raise ValueError('acquired NSE-1 mask differs: ' + name)
        masks[name] = data
    report = prepare(root, run, record_checksum=record_checksum)
    machine = 'nse2r4v' if nominal_vbat else 'nse2r4t'
    (run / 'roms/noki5110i').rename(run / 'roms' / machine)
    if (run / 'nvram/noki5110i').exists():
        (run / 'nvram/noki5110i').rename(run / 'nvram' / machine)
    for name, data in masks.items():
        (run / 'roms' / machine / name).write_bytes(data)
    report.update(machine=machine, dsp_backend='native C54x; acquired NSE-1 ROM4 compatibility input',
                  fitted_mask_proved=False, authentic_provisioning=False,
                  dsp_clock='52 MHz NSE-1 compatibility assumption; unmeasured for NSE-2',
                  mask_inputs={name: dict(bytes=len(data), sha1=hashlib.sha1(data).hexdigest())
                               for name, data in masks.items()})
    report['adc2_input'] = dict(raw=554 if nominal_vbat else 1023,
                               basis='NSE-1 3.6 V / typical CCONT 6.5 mV per bit' if nominal_vbat else 'research open-pin default',
                               measured_nse2_calibration=False)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--mame', type=Path)
    parser.add_argument('--record-checksum-only', action='store_true')
    parser.add_argument('--nominal-vbat', action='store_true', help='separate 3.6 V battery-family hypothesis')
    parser.add_argument('--physical-menu', action='store_true', help='press only the physical Menu contacts')
    parser.add_argument('--seconds', type=int, default=8, choices=range(8, 121), metavar='8..120')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    mame = (args.mame or root / 'mame/mame').resolve()
    try:
        report = prepare_native(root, run, record_checksum=args.record_checksum_only,
                                nominal_vbat=args.nominal_vbat)
        machine = report['machine']
        command = [str(mame), machine, '-rompath', str(run / 'roms'),
                   '-nvram_directory', str(run / 'nvram'), '-cfg_directory', str(run / 'cfg'),
                   '-video', 'none', '-sound', 'none', '-joystickprovider', 'none',
                   '-midiprovider', 'none', '-nothrottle', '-noreadconfig', '-log', '-verbose',
                   '-seconds_to_run', str(args.seconds), '-autoboot_delay', '0',
                   '-autoboot_script', str(root / 'tools' / (
                       'nse2_menu_fixture.lua' if args.physical_menu else 'nse2_boot_observe.lua'))]
        with mame.open('rb') as stream:
            report['mame_sha256'] = hashlib.file_digest(stream, 'sha256').hexdigest()
        report['command'] = command
        (run / 'inputs.json').write_text(json.dumps(report, indent=2) + '\n')
        with (run / 'console.log').open('w') as stream:
            result = subprocess.run(command, cwd=run, stdout=stream, stderr=subprocess.STDOUT)
        (run / 'exit_status.json').write_text(json.dumps({'returncode': result.returncode}) + '\n')
        if result.returncode:
            raise ValueError(f'native MAME exit {result.returncode}')
        console = (run / 'console.log').read_text(errors='replace')
        if 'LUA ERROR' in console or 'Error in' in console:
            raise ValueError('native observer failed')
        source = int(report['loader']['entries'][25]['descriptor'], 16) - 0x200000 + 12
        flash = (run / 'roms' / machine / '5110i_5.53A.fls').read_bytes()
        with Image.open(run / 'snap/nse2_7.png') as frame:
            result = verify_native((run / 'error.log').read_text(errors='replace'),
                                   flash[source:source + 400], frame.convert('L').tobytes(), frame.size,
                                   record_checksum=args.record_checksum_only, minimum_time=args.seconds)
            if args.nominal_vbat:
                result.update(verify_nominal_voltage((run / 'error.log').read_text(errors='replace'),
                                                    frame.convert('L').tobytes(), physical_menu=args.physical_menu))
        if args.physical_menu and not args.nominal_vbat:
            result['menu_probe'] = verify_menu_probe((run / 'error.log').read_text(errors='replace'))
        (run / 'acceptance.json').write_text(json.dumps(result, indent=2) + '\n')
        if args.nominal_vbat:
            scope = 'decoded physical Menu' if args.physical_menu else 'graphical Insert SIM card'
            print(f'NSE-2 nominal-voltage {scope} PASS; fitted board and phone services unproved')
        else:
            print('NSE-2 native verifier and readiness PASS; usable boot remains unproved')
        print(run)
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSE-2 native experiment FAIL: {error}\n')


if __name__ == '__main__':
    main()

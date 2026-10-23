"""Observe own NSE-2 boot with explicit acquired NSE-1 ROM4 compatibility inputs."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET

from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.run_nse2_boundary import prepare
from tools.nse2_native_check import verify_native, verify_menu_probe, verify_nominal_voltage
from tools.nse2_sim_check import verify_sim_reads, verify_security_attempt, verify_post_auth_menu
from tools.nse2_lifecycle_check import (
    diagnose_processing, diagnose_schedule, diagnose_transport,
    diagnose_scan_progress, diagnose_host_packets,
)
from tools.nse2_native_result_check import inspect_capture, inspect_control_trace, inspect_request_lifecycle


MASK_INPUTS = (
    ('nse1_rom4_dsp_program.bin', 0x1fffe, 'a05a1e96a8c36ec5a47e1ea059d15afa54ca5739'),
    ('nse1_rom4_dsp_data.bin', 0x20000, '024c7f970f4ef754d3e90471de48a167515f930d'),
)


def prepare_native(root, run, *, record_checksum=False, nominal_vbat=False, insert_sim=False, phone_retry_zero=False):
    if run.exists():
        raise ValueError('NSE-2 native experiment requires a new run directory')
    if insert_sim and not nominal_vbat:
        raise ValueError('NSE-2 inserted card requires the nominal-voltage research socket')
    if phone_retry_zero and not (record_checksum and insert_sim):
        raise ValueError('NSE-2 retry fixture requires integrity EEPROM and inserted SIM')
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
    report['inserted_synthetic_sim'] = insert_sim
    if phone_retry_zero:
        path = run / 'nvram' / machine / 'eeprom'
        image = bytearray(path.read_bytes())
        image[0x4a4] = 0
        path.write_bytes(image)
        report.update(phone_retry_fixture={'address': '04a4', 'initial': 0, 'authentic_dump': False},
                      initial_eeprom='diagnostic own checksums plus explicit zero-attempt byte 04a4',
                      eeprom_sha256=hashlib.sha256(image).hexdigest())
    if insert_sim:
        config = ET.Element('mameconfig', version='10')
        system = ET.SubElement(config, 'system', name=machine)
        inputs = ET.SubElement(system, 'input')
        ET.SubElement(inputs, 'port', tag=':SIM_REMOVED', type='OTHER', mask='1', defvalue='1', value='0')
        ET.ElementTree(config).write(run / 'cfg' / f'{machine}.cfg', encoding='utf-8', xml_declaration=True)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--mame', type=Path)
    parser.add_argument('--record-checksum-only', action='store_true')
    parser.add_argument('--nominal-vbat', action='store_true', help='separate 3.6 V battery-family hypothesis')
    parser.add_argument('--insert-sim', action='store_true', help='insert the available synthetic card through its physical input')
    parser.add_argument('--physical-menu', action='store_true', help='press only the physical Menu contacts')
    parser.add_argument('--security-code', action='store_true', help='one physical 12345 + OK experiment')
    parser.add_argument('--phone-retry-zero', action='store_true', help='explicit own-record zero-attempt NV fixture')
    parser.add_argument('--post-auth-menu', action='store_true', help='physical Menu probe after accepted phone code')
    parser.add_argument('--native-processing', action='store_true', help='attach existing passive ROM4 processing observers')
    parser.add_argument('--seconds', type=int, default=8, choices=range(8, 121), metavar='8..120')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    mame = (args.mame or root / 'mame/mame').resolve()
    try:
        if args.insert_sim and args.seconds < 12:
            raise ValueError('NSE-2 complete SIM read bootstrap requires at least 12 seconds')
        if args.security_code and (not args.insert_sim or args.seconds < 18 or args.physical_menu):
            raise ValueError('security experiment requires inserted SIM, at least 18 seconds, and no Menu probe')
        if args.phone_retry_zero and not args.security_code:
            raise ValueError('retry fixture requires the physical security-code experiment')
        if args.post_auth_menu and not (args.phone_retry_zero and args.seconds >= 24):
            raise ValueError('post-auth Menu probe requires zero-attempt fixture and at least 24 seconds')
        if args.native_processing and not (args.post_auth_menu and args.seconds >= 30):
            raise ValueError('native processing census requires post-authentication Menu and at least 30 seconds')
        report = prepare_native(root, run, record_checksum=args.record_checksum_only,
                                nominal_vbat=args.nominal_vbat, insert_sim=args.insert_sim,
                                phone_retry_zero=args.phone_retry_zero)
        machine = report['machine']
        command = [str(mame), machine, '-rompath', str(run / 'roms'),
                   '-nvram_directory', str(run / 'nvram'), '-cfg_directory', str(run / 'cfg'),
                   '-video', 'none', '-sound', 'none', '-joystickprovider', 'none',
                   '-midiprovider', 'none', '-nothrottle', '-noreadconfig', '-log', '-verbose',
                   '-seconds_to_run', str(args.seconds), '-autoboot_delay', '0',
                   '-autoboot_script', str(root / 'tools' / ('nse2_processing_fixture.lua' if args.native_processing else
                       'nse2_post_auth_fixture.lua' if args.post_auth_menu else
                       'nse2_security_fixture.lua' if args.security_code else
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
                                                    frame.convert('L').tobytes(), physical_menu=args.physical_menu,
                                                    empty_socket_frame=not args.insert_sim))
        if args.physical_menu and not args.nominal_vbat:
            result['menu_probe'] = verify_menu_probe((run / 'error.log').read_text(errors='replace'))
        if args.insert_sim:
            text = (run / 'error.log').read_text(errors='replace')
            with Image.open(run / 'snap/nse2_7.png') as frame:
                result.update(verify_sim_reads(text, frame.convert('L').tobytes()))
            if args.security_code:
                with Image.open(run / 'snap/nse2_security_result.png') as frame:
                    result.update(verify_security_attempt(text, frame.convert('L').tobytes(), retry_zero=args.phone_retry_zero))
            if args.post_auth_menu:
                with Image.open(run / 'snap/nse2_post_auth_21.png') as menu, \
                        Image.open(run / 'snap/nse2_post_auth_23.png') as submenu:
                    result.update(verify_post_auth_menu(text, menu.convert('L').tobytes(), submenu.convert('L').tobytes()))
        if args.native_processing:
            result['native_processing_boundary'] = dict(diagnose_schedule(text), **diagnose_transport(text))
            result['native_processing_boundary']['conditional_request'] = inspect_control_trace(text)
            result['native_processing_boundary']['request_lifecycle'] = inspect_request_lifecycle(text)
            result['native_processing_boundary']['processing'] = diagnose_processing(text)
            result['native_processing_boundary']['measurement'] = diagnose_scan_progress(text)
            result['native_processing_boundary']['host_packets'] = diagnose_host_packets(text)
            result['native_processing_boundary']['result_paths'] = inspect_capture(
                (run / 'nse2_native_program.bin').read_bytes(),
                (run / 'roms' / machine / 'nse1_rom4_dsp_program.bin').read_bytes(),
                (run / 'roms' / machine / 'nse1_rom4_dsp_data.bin').read_bytes())
        (run / 'acceptance.json').write_text(json.dumps(result, indent=2) + '\n')
        if args.post_auth_menu:
            print('NSE-2 physical Phone book menu/submenu PASS; contact persistence and network unproved')
        elif args.insert_sim:
            print('NSE-2 SIM read bootstrap PASS; ' +
                  ('phone code accepted, application UI unproved' if args.phone_retry_zero else
                   'phone-code authentication remains unproved'))
        elif args.nominal_vbat:
            scope = 'decoded physical Menu' if args.physical_menu else 'graphical Insert SIM card'
            print(f'NSE-2 nominal-voltage {scope} PASS; fitted board and phone services unproved')
        else:
            print('NSE-2 native verifier and readiness PASS; usable boot remains unproved')
        print(run)
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSE-2 native experiment FAIL: {error}\n')


if __name__ == '__main__':
    main()

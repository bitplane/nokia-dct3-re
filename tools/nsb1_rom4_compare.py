"""Run own NSB-1 against explicitly unproved NSE-1 resident-ROM compatibility inputs."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import xml.etree.ElementTree as ET

INPUTS = (
    ('noki5190/5190f671a.fls', '27b8f0cb8640e5b423960b206e9aee065f32d726'),
    ('noki5110/nse1_rom4_dsp_program.bin', 'a05a1e96a8c36ec5a47e1ea059d15afa54ca5739'),
    ('noki5110/nse1_rom4_dsp_data.bin', '024c7f970f4ef754d3e90471de48a167515f930d'),
)


EMPTY_SOCKET_RGB_SHA256 = 'a879c658a532b6526d3343287b42848f8a16bf10b7215086c1e215db45ee67cc'
PHONEBOOK_LIST_RGB_SHA256 = '1c9dac19aa382370ba315a4e99f2ea65c844f4ec099a263624b6593b0525a5bd'


def check_frame(path, expected):
    from PIL import Image
    with Image.open(path) as source:
        frame = source.convert('RGB')
        if frame.size != (84, 48) or hashlib.sha256(frame.tobytes()).hexdigest() != expected:
            raise ValueError(f'NSB-1 frame mismatch: {path.name}')


def check_menu(trace, snapshots, frame_indices=(2, 3)):
    from PIL import Image
    contacts = re.findall(r'nsb1_menu_contact: index=(\d+) down=([01]) t=[0-9.]+ physical_only=1', trace)
    if contacts != [('1', '1'), ('1', '0'), ('2', '1'), ('2', '0')]:
        raise ValueError('missing physical Menu/Select sequence')
    # The list is static; unlike the parent menu it has no animated icon.
    for index in frame_indices:
        with Image.open(snapshots / f'nsb1_menu_{index}.png') as source:
            frame = source.convert('RGB')
            if frame.size != (84, 48) or hashlib.sha256(frame.tobytes()).hexdigest() != PHONEBOOK_LIST_RGB_SHA256:
                raise ValueError('physical phonebook list pixels changed')


def check_native_execution(trace, minimum_time=5):
    samples = re.findall(r'nsb1_native_execution: t=([0-9.]+) pc=([0-9a-f]{4}) illegal=([01]) idle=[01] tx=[0-9a-f]{4}/[0-9a-f]{4} rx=[0-9a-f]{4}/[0-9a-f]{4} observer_writes=0', trace)
    if not samples or not any(float(time) >= minimum_time for time, _, _ in samples):
        raise ValueError('missing native DSP execution observations')
    if any(illegal != '0' for _, _, illegal in samples):
        raise ValueError('native DSP stopped on an illegal instruction; bootstrap alone is insufficient')


def check_receive_activity(trace, minimum_time):
    """Require the observed PIN-disabled menu/control inactivity schedule, not CPU legality."""
    if minimum_time >= 34:
        activity = re.findall(r'nsb1_activity_check: count=([0-9a-f]{4}) pc=002aff04 t=[0-9.]+ observer_writes=0', trace)
        required = int(minimum_time // 31)
        if len(activity) < required or any(int(value, 16) == 0 for value in activity):
            raise ValueError('missing sustained native receive activity at inactivity checks')


def check_security_gate(trace, initialized_retry):
    contacts = re.findall(r'nsb1_phonecode_contact: index=(\d+) down=([01]) t=[0-9.]+ physical_only=1', trace)
    if contacts != [(str(index), down) for index in range(1, 7) for down in ('1', '0')]:
        raise ValueError('missing physical security-code contact sequence')
    gates = re.findall(r'nsb1_security_gate: retry=([0-9a-f]{2}) match=([0-9a-f]{8}) t=[0-9.]+ observer_writes=0', trace)
    if gates != [('00' if initialized_retry else 'ff', '00000001')]:
        raise ValueError('own security-code comparison/retry gate changed')


def check_late_presentation(trace, snapshots):
    from PIL import Image
    samples = re.findall(r'nsb1_late_ui: t=([0-9.]+) phase=03 reports=0f column_mask=20 observer_writes=0', trace)
    if not any(10 <= float(value) < 10.1 for value in samples):
        raise ValueError('missing late empty-socket readiness observation')
    with Image.open(snapshots / 'nsb1_native_10s.png') as source:
        frame = source.convert('RGB')
        if frame.size != (84, 48) or hashlib.sha256(frame.tobytes()).hexdigest() != EMPTY_SOCKET_RGB_SHA256:
            raise ValueError('late empty-socket Insert SIM card pixels changed')


def checksum_control(source, initialized_retry=False):
    """Own decoded integrity fields only; never claim erased contents are calibrated."""
    if len(source) != 8192:
        raise ValueError('checksum control requires own 8 KiB external storage')
    result = bytearray(source)
    security_sum = sum(source[:0x11c]) & 0xffff
    calibration_sum = (sum(source[0x120:0x1fe]) - source[0x154] - source[0x155]) & 0xffff
    struct.pack_into('>I', result, 0x11c, security_sum)
    struct.pack_into('>H', result, 0x1fe, calibration_sum)
    if initialized_retry:
        # Own 0709 record: 2ab974 rejects all comparisons at counts >= 5.
        # External synthetic storage only; do not alter identity or encoded code.
        result[0x4ac] = 0
    return bytes(result)


def check_checksum_control(trace):
    flags = re.findall(r'nsb1_presentation: t=([0-9.]+) selftest_flags=([0-9a-f]{2}) observer_writes=0', trace)
    if len(flags) != 5 or [value for _, value in flags] != ['00', 'c0', 'c0', '40', '40']:
        raise ValueError('checksum control did not retain the observed self-test bit 40')
    if re.search(r'nsb1_selftest_flags: value=[0-9a-f]{2} pc=0023a018 ', trace):
        raise ValueError('checksum-failure branch still executed in checksum control')
    if not re.search(r'nsb1_startup_mode: value=02 pc=002a438e t=[0-9.]+ observer_writes=0', trace):
        raise ValueError('missing own ADC-conditioned initializer mode write')
    for marker in ('direction=W address=00020038 data=ff pc=0029cdfc',
                   'direction=W address=0002003a data=03 pc=0029ce26',
                   'direction=W address=0002003d data=18 pc=0029ce3c'):
        if marker not in trace:
            raise ValueError('missing own SIMI initialization after integrity acceptance')
    simi = re.findall(r'nsb1_simi_summary: t=([0-9.]+) reads=(\d+) writes=(\d+) card_inserted=0 observer_writes=0', trace)
    if len(simi) != 5 or [(reads, writes) for _, reads, writes in simi] != [
            ('0', '0'), ('0', '0'), ('0', '18'), ('3', '21'), ('9', '45')]:
        raise ValueError('unexpected empty-socket SIMI lifecycle')
    readiness = re.findall(r'nsb1_input_readiness: t=([0-9.]+) startup_reason=([0-9a-f]{2}) phase=([0-9a-f]{2}) reports=([0-9a-f]{2}) column_mask=([0-9a-f]{2}) observer_writes=0', trace)
    # Nominal-input diagnostic boundary, not accepted presentation or fitted hardware.
    if (len(readiness) != 5 or not 5 <= float(readiness[-1][0]) < 5.1
            or readiness[-1][1:] != ('0a', '03', '0f', '20')):
        raise ValueError('checksum-control input-readiness boundary changed; inspect before re-banking')


def check_trace(trace, program3, program4, returncode):
    if returncode or trace.count('Soft reset') != 1 or '[LUA ERROR]' in trace or 'nsb1_reset_reason:' in trace:
        raise ValueError('native comparison reset, crashed or failed observation')
    programs = re.findall(r'nsb1_release_program: release=(\d+) words=186 data=([0-9a-f]+) observer_writes=0', trace)
    if len(program3) != 372 or len(program4) != 372 or programs != [('1', program3.hex()), ('2', program4.hex())]:
        raise ValueError('ROM3/ROM4 retries did not execute the two own uploads')
    releases = re.findall(r'nsb1_dsp_release: command=01 arguments=([0-9a-f]+) pc=([0-9a-f]+)', trace)
    if releases != [('0100030000008000000100010200', '0028f9c8')] * 2 + [
            ('fd00ff8002440500007800000000', '0028fc50')]:
        raise ValueError('unexpected verifier/loader release lifecycle')
    reads = re.findall(r'nsb1_dsp_result_read: address=00010004 data=([0-9a-f]{8}) mask=([0-9a-f]{8}) pc=([0-9a-f]{8})', trace)
    expected = [('00040000', 'ffff0000', '0028f9dc'),
                ('00040000', 'ffff0000', '0028fa08'),
                ('00000003', '0000ffff', '0028fa0c'),
                ('00040000', 'ffff0000', '0028f9dc'),
                ('00040000', 'ffff0000', '0028fa08'),
                ('00000004', '0000ffff', '0028fa0c')]
    if reads[:6] != expected:
        raise ValueError('missing resident-version mismatch then organic ROM4 acceptance')
    completions = re.findall(r'nsb1_dsp_result_read: address=00010000 data=([0-9a-f]{8}) mask=0000ffff pc=0028fa96', trace)
    if completions != ['0000ffff', '00000004']:
        raise ValueError('missing native final-result transition from pending to version 4')
    states = re.findall(r'nsb1_dsp_state: t=([0-9.]+) .*?init_success=01 saved_version=0004 handoffs=63 releases=3 observer_writes=0', trace)
    if len(states) != 4 or any(abs(float(actual) - wanted) > 0.02 for actual, wanted in zip(states, (0.5, 1, 2, 5))):
        raise ValueError('missing completed 63-handoff native continuation observations')
    for marker in ('rom4_port_write: port=02 data=ffff pc=0d8f',
                   'rom4_port_write: port=36 data=0080 pc=8d98'):
        if marker not in trace:
            raise ValueError('missing native loader/resident execution evidence')


def check_lcd(trace, snapshots):
    from PIL import Image

    commands = re.findall(r'nsb1_lcd_command: byte=([0-9a-f]{2}) pc=', trace)
    expected = '24 40 80 41 80 42 80 43 80 44 80 45 80 20 21 05 14 ce 20 0c'.split()
    if commands[:20] != expected:
        raise ValueError('missing own six-bank LCD initialization')
    summaries = re.findall(r'nsb1_lcd_summary: t=([0-9.]+) commands=(\d+) data_bytes=(\d+) nonzero=(\d+) observer_writes=0', trace)
    counts = [(int(commands), int(data), int(nonzero)) for _, commands, data, nonzero in summaries]
    if counts != [(0, 0, 0), (32, 1008, 504), (32, 1008, 504), (68, 2016, 504), (68, 2016, 504)]:
        raise ValueError('LCD traffic does not reproduce clear/fill/clear self-test')
    for name, value in (('nsb1_native_1s.png', 0), ('nsb1_native_5s.png', 255)):
        with Image.open(snapshots / name) as source:
            image = source.convert('RGB')
            if image.size != (84, 48) or image.getextrema() != ((value, value),) * 3:
                raise ValueError(f'LCD self-test pixels mismatch: {name}')


def inputs_for_ppm(ppm):
    if ppm == 'A':
        return INPUTS
    if ppm == 'B':
        return (('noki5190/5190f671b.fls', '49987333223932f13dd4fb8e71a9645497894f73'), *INPUTS[1:])
    raise ValueError('unsupported NSB-1 PPM selection')


def storage_for_ppm(ppm):
    inputs_for_ppm(ppm)
    return 'nsb1r4t' if ppm == 'A' else 'nsb1r4t_1'


def prepare(root, run, checksum_from=None, insert_sim=False, initialized_retry=False, ppm='A', english_dictionary=False):
    inputs = inputs_for_ppm(ppm)
    storage_name = storage_for_ppm(ppm)
    if english_dictionary and (ppm != 'B' or not initialized_retry):
        raise ValueError('English dictionary fixture requires PPM B and initialized retry control')
    if initialized_retry and (checksum_from is None or not insert_sim):
        raise ValueError('initialized retry requires an inserted-card checksum-control experiment')
    if run.exists():
        raise ValueError('use a fresh comparison directory; no prior EEPROM is admitted')
    qualified = []
    for name, expected in inputs:
        data = (root / name).read_bytes()
        if hashlib.sha1(data).hexdigest() != expected:
            raise ValueError(f'unqualified comparison input: {name}')
        qualified.append((name, data, expected))
    fixture = None
    if checksum_from:
        previous = json.loads((checksum_from / 'input_manifest.json').read_text())
        if (previous.get('inputs') != [{'source': name, 'sha1': expected} for name, expected in inputs]
                or previous.get('external_eeprom') != 'erased 8 KiB; no donor storage'):
            raise ValueError('checksum source must be a qualified own erased-storage comparison')
        if hashlib.sha1((checksum_from / 'nvram' / storage_name / 'flash').read_bytes()).hexdigest() != inputs[0][1]:
            raise ValueError('checksum source executable differs')
        original = (checksum_from / 'nvram' / storage_name / 'eeprom').read_bytes()
        fixture = checksum_control(original, initialized_retry)
        if english_dictionary:
            # Own 0702+0x17 -> 10fced; qualified PPM B declares LDB language 1.
            fixture = bytearray(fixture)
            fixture[0x4c7] = 1
            fixture = bytes(fixture)
    target = run / 'inputs/nsb1r4t'
    target.mkdir(parents=True)
    for name, data, expected in qualified:
        (target / Path(name).name).write_bytes(data)
    manifest = {'inputs': [{'source': name, 'sha1': expected} for name, _, expected in qualified],
                'ppm': ppm, 'bios': '671' + ppm.lower(), 'storage_name': storage_name,
                'external_eeprom': 'erased 8 KiB; no donor storage',
                'resident_mask': 'NSE-1 compatibility candidate; NSB-1 fitted identity unproved',
                'dsp_clock_hz': 52000000, 'physical_dsp_clock_proved': False,
                'adc2_raw': 554, 'adc2_basis': '3.6 V / CCONT typical 6.5 mV per bit; nominal hypothesis',
                'physical_nsb1_adc_routing_proved': False,
                'synthetic_sim_inserted': insert_sim,
                'synthetic_retry_initialized': initialized_retry,
                'synthetic_dictionary_language': 1 if english_dictionary else None,
                'boot_claim': False}
    if fixture is not None:
        storage = run / 'nvram' / storage_name
        storage.mkdir(parents=True)
        (storage / 'eeprom').write_bytes(fixture)
        manifest['external_eeprom'] = ('diagnostic checksum/retry control; calibration/identity not validated'
                                      if initialized_retry else
                                      'diagnostic checksum-only control; calibration/identity not validated')
        manifest['checksum_source_run'] = str(checksum_from.resolve())
        manifest['checksum_source_sha1'] = hashlib.sha1(original).hexdigest()
        manifest['checksum_fixture_sha1'] = hashlib.sha1(fixture).hexdigest()
        manifest['changed_offsets'] = [hex(i) for i, (a, b) in enumerate(zip(original, fixture)) if a != b]
    (run / 'input_manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    if insert_sim:
        config = ET.Element('mameconfig', version='10')
        system = ET.SubElement(config, 'system', name='nsb1r4t')
        inputs = ET.SubElement(system, 'input')
        ET.SubElement(inputs, 'port', tag=':SIM_REMOVED', type='OTHER', mask='1', defvalue='1', value='0')
        (run / 'cfg').mkdir()
        ET.ElementTree(config).write(run / 'cfg/nsb1r4t.cfg', encoding='utf-8', xml_declaration=True)
    return run / 'inputs'


def run_mame(binary, rompath, run, script, seconds, verbose=False, bios=None):
    if bios not in (None, '671a', '671b'):
        raise ValueError('unsupported NSB-1 BIOS')
    with (run / 'mame_output.log').open('w') as output:
        result = subprocess.run([
            str(binary), 'nsb1r4t', '-rompath', str(rompath), '-noreadconfig',
            *(['-bios', bios] if bios else []),
            '-video', 'none', '-sound', 'none', '-nothrottle', '-log',
            *(['-verbose'] if verbose else []),
            '-seconds_to_run', str(seconds), '-autoboot_delay', '0',
            '-autoboot_script', str(script)],
            cwd=run, stdout=output, stderr=subprocess.STDOUT, timeout=180)
    (run / 'exit_status.json').write_text(json.dumps({'returncode': result.returncode}) + '\n')
    return result.returncode


def prepare_cold_run(run, source, validate_card, inputs, ppm='A'):
    """Copy only qualified own persisted devices; the workflow validates card contents."""
    if run.exists():
        raise ValueError('cold reload requires a fresh directory')
    storage_name = storage_for_ppm(ppm)
    manifest = json.loads((source / 'input_manifest.json').read_text())
    if (manifest.get('inputs') != [{'source': name, 'sha1': digest} for name, digest in inputs]
            or manifest.get('ppm', 'A') != ppm
            or not manifest.get('synthetic_sim_inserted') or not manifest.get('synthetic_retry_initialized')
            or manifest.get('external_eeprom') != 'diagnostic checksum/retry control; calibration/identity not validated'):
        raise ValueError('cold reload requires qualified own synthetic save provenance')
    for name, digest in inputs:
        if hashlib.sha1((source / 'inputs/nsb1r4t' / Path(name).name).read_bytes()).hexdigest() != digest:
            raise ValueError('cold reload executable/resident inputs changed')
    storage = source / 'nvram' / storage_name
    files = {name: (storage / name).read_bytes() for name in ('flash', 'eeprom', 'sim_card')}
    if hashlib.sha1(files['flash']).hexdigest() != inputs[0][1] or len(files['eeprom']) != 8192:
        raise ValueError('cold reload executable or EEPROM geometry changed')
    validate_card(files['sim_card'])
    target = run / 'nvram' / storage_name
    target.mkdir(parents=True)
    shutil.copytree(source / 'inputs', run / 'inputs')
    shutil.copytree(source / 'cfg', run / 'cfg')
    for name, data in files.items():
        (target / name).write_bytes(data)
    manifest['cold_source_run'] = str(source.resolve())
    manifest['cold_storage_sha256'] = {name: hashlib.sha256(data).hexdigest() for name, data in files.items()}
    (run / 'input_manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return run / 'inputs'


def check_cold_link(run, source, inputs=INPUTS, ppm='A'):
    """Requalify executable inputs and preceding saved-device hashes."""
    manifest = json.loads((run / 'input_manifest.json').read_text())
    if (manifest.get('inputs') != [{'source': name, 'sha1': digest} for name, digest in inputs]
            or manifest.get('ppm', 'A') != ppm
            or not manifest.get('synthetic_sim_inserted')
            or not manifest.get('synthetic_retry_initialized')
            or manifest.get('external_eeprom') != 'diagnostic checksum/retry control; calibration/identity not validated'):
        raise ValueError('own cold-input provenance differs')
    for name, digest in inputs:
        if hashlib.sha1((run / 'inputs/nsb1r4t' / Path(name).name).read_bytes()).hexdigest() != digest:
            raise ValueError('own executable/resident input changed')
    storage = source / 'nvram' / storage_for_ppm(ppm)
    hashes = {name: hashlib.sha256((storage / name).read_bytes()).hexdigest()
              for name in ('flash', 'eeprom', 'sim_card')}
    if manifest.get('cold_storage_sha256') != hashes:
        raise ValueError('cold input hashes differ from preceding saved devices')


def check_card_run(run, returncode, seconds, ppm='A'):
    image = (run / 'inputs/nsb1r4t' / Path(inputs_for_ppm(ppm)[0][0]).name).read_bytes()
    storage = run / 'nvram' / storage_for_ppm(ppm)
    trace = (run / 'error.log').read_text()
    check_trace(trace, image[0xb9350:0xb9350 + 372], image[0xbe570:0xbe570 + 372], returncode)
    check_native_execution(trace, seconds - 1)
    if (storage / 'flash').read_bytes() != image:
        raise ValueError('card phase changed executable flash')
    if len((storage / 'eeprom').read_bytes()) != 8192:
        raise ValueError('card phase changed EEPROM geometry')
    return trace, (storage / 'sim_card').read_bytes()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('rom_root', type=Path)
    parser.add_argument('run_dir', type=Path)
    parser.add_argument('--seconds', type=int, default=6, choices=range(1, 121))
    parser.add_argument('--verbose', action='store_true', help='include the larger native opcode inventory')
    parser.add_argument('--insert-sim', action='store_true', help='physical card-input experiment; no card-service acceptance claim')
    parser.add_argument('--initialized-retry', action='store_true',
                        help='diagnostic external EEPROM only: initialize own 0709 retry byte to zero')
    parser.add_argument('--phonecode-fixture', action='store_true',
                        help='physical 12345/Menu entry plus own comparison/retry observation (27 seconds minimum)')
    parser.add_argument('--menu-fixture', action='store_true',
                        help='physical code entry and Phone book list acceptance (35 seconds minimum)')
    parser.add_argument('--checksum-control-from', type=Path,
                        help='diagnostic only: recompute six checksum bytes from a qualified own erased-storage run')
    args = parser.parse_args()
    if args.menu_fixture:
        if not args.initialized_retry or args.seconds < 35:
            parser.error('menu fixture requires initialized retry and at least 35 seconds')
        args.phonecode_fixture = True
    if args.phonecode_fixture and (not args.insert_sim or not args.checksum_control_from or args.seconds < 27):
        parser.error('phonecode fixture requires inserted card, checksum source and at least 27 seconds')
    try:
        binary, root, run = args.binary.resolve(), args.rom_root.resolve(), args.run_dir.resolve()
        rompath = prepare(root, run, args.checksum_control_from, args.insert_sim, args.initialized_retry)
        script = Path(__file__).with_name(
            'nsb1_menu_fixture.lua' if args.menu_fixture else
            'nsb1_security_fixture.lua' if args.phonecode_fixture else 'nsb1_boot_observe.lua').resolve()
        returncode = run_mame(binary, rompath, run, script, args.seconds, args.verbose)
        if args.seconds >= 6:
            image = (rompath / 'nsb1r4t/5190f671a.fls').read_bytes()
            if args.menu_fixture:
                check_native_execution((run / 'error.log').read_text(), minimum_time=args.seconds - 1)
                check_receive_activity((run / 'error.log').read_text(), args.seconds - 1)
            check_trace((run / 'error.log').read_text(), image[0xb9350:0xb9350 + 372],
                        image[0xbe570:0xbe570 + 372], returncode)
            if args.checksum_control_from is None:
                check_lcd((run / 'error.log').read_text(), run / 'snap')
            elif not args.insert_sim:
                check_checksum_control((run / 'error.log').read_text())
                if args.seconds >= 12:
                    check_late_presentation((run / 'error.log').read_text(), run / 'snap')
            if (run / 'nvram/nsb1r4t/flash').read_bytes() != image:
                raise ValueError('own executable flash changed')
            if len((run / 'nvram/nsb1r4t/eeprom').read_bytes()) != 8192:
                raise ValueError('external EEPROM capacity changed')
            if args.phonecode_fixture:
                check_security_gate((run / 'error.log').read_text(), args.initialized_retry)
            if args.menu_fixture:
                check_menu((run / 'error.log').read_text(), run / 'snap')
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'NSB-1 comparison failed: {error}\n')
    status = 'native bootstrap PASS' if args.seconds >= 6 else f'run recorded: returncode={returncode}'
    qualifier = ''
    if args.checksum_control_from:
        qualifier = ('checksum diagnostic; graphical empty-socket frame accepted; no card/service acceptance; '
                     if args.seconds >= 12 else 'checksum diagnostic; late presentation not tested; ')
    if args.insert_sim:
        qualifier = 'inserted-card experiment; card/presentation acceptance not established; '
    if args.menu_fixture:
        qualifier = 'sustained native execution and physical Phone book list accepted; no registration/service acceptance; '
    if re.search(r'nsb1_native_execution: .* illegal=1 ', (run / 'error.log').read_text()):
        qualifier += 'DSP execution fault observed; no sustained-native acceptance; '
    print(f'NSB-1 compatibility {status}; {qualifier}fitted mask/phone boot not promoted; evidence={run}')


if __name__ == '__main__':
    main()

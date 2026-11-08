"""Bounded native 6210 compatibility checks, not fitted-mask qualification."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
# Reviewed Messages envelope animation phase under finite ADC stimulation.
# Keep the ordinary native Menu oracle unchanged.
ADC_SCH_MENU_SHA256 = '81b207e77820f80f4844daeeebdbd260c1f2590888de1d0274ac67f7d036b586'
ADC_P09_MENU_SHA256 = '748bea6093d040edebd9141c371983ad858fe1744d6f28403bb6605b79cd8638'
# Full-SI acquisition changes the envelope animation phase, not menu text.
ADC_BROADCAST_MENU_SHA256 = '8c7650fdb0514ec34c85b89795e529de062e6f141268a507bafc7eb77370df65'


def expected_menu_digest(adc_source, capture_start):
    from tools.run_noki6210_acceptance import MENU_SHA256
    if adc_source and adc_source != 'silence' and capture_start == 'p09':
        return ADC_BROADCAST_MENU_SHA256 if adc_source == 'fcch-sch-broadcast' else ADC_P09_MENU_SHA256
    return ADC_SCH_MENU_SHA256 if adc_source == 'fcch-sch' else MENU_SHA256
if __package__ in (None, ''):
    sys.path.insert(0, str(ROOT))


def check_input_start(text, marker, expected):
    times = re.findall(re.escape(marker) + r' t=([\d.]+)', text)
    if len(times) != 1 or abs(float(times[0]) - expected) > 0.02:
        raise ValueError('native 6210 physical input did not start at the requested horizon')


def check_execution(text, loader, program, flash, *, reference=False, minimum_seconds=0):
    """Reject incomplete initialization, restarts and unacquired low ROM reads."""
    clock = 45_500_000 if reference else 13_000_000
    required = {
        f'npe3_native_composition: hle=0 staged=0 clock_policy={clock} fitted_mask_claim=0 observer_writes=0': 1,
        'npe3_native_control_contract: status=10 release=04 execution=01 readback=05 observer_writes=0': 1,
        'npe3_native_verifier: result0=0000 result1=0006': 1,
        'npe3_native_entry: kind=reset': 2,
        'npe3_native_entry: kind=loader': 2,
        'npe3_native_entry: kind=resident': 1,
        'npe3_native_selftest: command=0d faults=00 flag=c4': 1,
        'npe3_native_coverage: missing_program_reads=0 observer_writes=0': 1,
    }
    if reference:
        required['npe3_native_reference_clock: source=8210_documented_rate fitted_clock_claim=0 experiment=1 observer_writes=0'] = 1
    for marker, count in required.items():
        if text.count(marker) != count:
            raise ValueError(f'native 6210 marker count changed: {marker}')
    terminals = re.findall(r'npe3_native_terminal: t=([\d.]+) arm=[\da-f]+ dsp=[\da-f]+ illegal=(\d+) idle=[01] pmst=[\da-f]+ observer_writes=0', text)
    if (len(terminals) != 1 or terminals[0][1] != '0' or 'Lua error' in text
            or re.search(r'staged.*unsupported|unsupported.*staged', text)):
        raise ValueError('native 6210 terminal execution is incomplete or illegal')
    if float(terminals[0][0]) < minimum_seconds:
        raise ValueError('native 6210 ended before the required horizon')
    clocks = re.findall(r'native_dsp_terminal_timing: released=1 clock=(\d+)', text)
    if clocks != [str(clock)]:
        raise ValueError('native 6210 terminal clock or reset-release state changed')
    if (loader != flash[0x24b7c:0x24b7c + 1258]
            or len(program) != 0x20000
            or program[0x590 * 2:0x736 * 2] != flash[0x1eac4:0x1eac4 + 844]):
        raise ValueError('native uploads do not match own 6210 MCU bytes')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds', type=int, default=30)
    parser.add_argument('--scenario', choices=('menu', 'phonebook'), default='menu')
    parser.add_argument('--input-at', type=int, default=20,
                        help='physical input start in emulated seconds (20..100)')
    parser.add_argument('--reference-clock', action='store_true',
                        help='select the separate 45.5MHz reference-DSP experiment, not a fitted 6210 clock')
    parser.add_argument('--reset-policy', action='store_true',
                        help='add passive reset-policy and native ring observations')
    parser.add_argument('--radio', action='store_true',
                        help='capture complete passive DSP I/O writes and bounded read summaries')
    parser.add_argument('--adc-source', choices=('silence', 'fcch-sch', 'fcch-sch-bcch', 'fcch-sch-broadcast'),
                        help='explicit raw ADC laboratory source; requires --reference-clock')
    parser.add_argument('--capture-start', choices=('immediate', 'p09'), default='immediate',
                        help='explicit laboratory capture-edge hypothesis, not fitted hardware')
    args = parser.parse_args()
    if args.capture_start != 'immediate' and not args.adc_source:
        parser.error('capture-edge candidates require an explicit ADC laboratory source')
    if args.adc_source in ('fcch-sch-bcch','fcch-sch-broadcast') and args.capture_start != 'p09':
        parser.error('the raw BCCH candidate requires explicit P09 laboratory capture')
    if args.adc_source:
        if not args.reference_clock:
            parser.error('ADC experiments require the explicitly labelled reference clock')
        args.radio = True
    if not 24 <= args.seconds <= 120:
        parser.error('Menu check duration must be 24..120 seconds')
    if not 20 <= args.input_at <= 100:
        parser.error('physical input start must be 20..100 seconds')
    flash = (ROOT / 'roms/noki6210/6210_556c.fls').read_bytes()
    pmm = (ROOT / 'roms/noki6210/6210 virgin eeprom 005fa000.fls').read_bytes()
    if (hashlib.sha1(flash).hexdigest() != '3d9ea319503e78ec69b60d72cda23e461e118ea9'
            or hashlib.sha1(pmm).hexdigest() != 'b3a527ede1be87bd715fb3741a81eef5bd422efa'):
        raise ValueError('NPE-3 scout requires pinned own MCU/PMM inputs')
    run = Path(tempfile.mkdtemp(prefix='npe3-native-', dir=ROOT / 'tmp'))
    print(f'Native6210 evidence: {run}', flush=True)
    model = 'npe3ref' if args.reference_clock else 'npe3r6'
    if args.adc_source:
        model = 'npe3ad0' if args.adc_source == 'silence' else 'npe3afs'
        if args.capture_start == 'p09':
            model = 'npe3zp9' if args.adc_source == 'silence' else 'npe3cp9'
            if args.adc_source == 'fcch-sch-bcch': model = 'npe3bp9'
            if args.adc_source == 'fcch-sch-broadcast': model = 'npe3sp9'
    storage = run / 'nvram' / model
    storage.mkdir(parents=True)
    (storage / 'flash').write_bytes(flash.ljust(0x3fa000, b'\xff') + pmm)
    roms = run / 'roms'
    roms.mkdir()
    (roms / model).symlink_to(ROOT / 'roms/nsm3r6', target_is_directory=True)
    observer = ('rom6_adc_lab.lua' if args.adc_source else 'rom6_native_radio.lua' if args.radio else
                'rom6_reset_policy_observe.lua' if args.reset_policy else 'npe3_native_observe.lua')
    command=[str(ROOT/'mame/mame'),model,'-rompath',str(roms)+';'+str(ROOT/'roms'),
        '-nvram_directory',str(run/'nvram'),'-cfg_directory','cfg','-snapshot_directory','snap',
        '-noreadconfig','-joystickprovider','none','-midiprovider','none','-keyboardprovider','none',
        '-mouseprovider','none','-lightgunprovider','none','-video','none','-sound','none','-nothrottle',
        '-debug','-debugger','none','-log','-autoboot_delay','0',
        '-autoboot_script',str(ROOT/'tools'/observer),'-seconds_to_run',str(args.seconds)]
    (run/'command.json').write_text(json.dumps(command,indent=2)+'\n')
    def execute(phase, scenario, seconds, persist=False):
        directory = run / phase
        directory.mkdir()
        cmd = command.copy()
        cmd[-1] = str(seconds)
        if not persist:
            cmd.append('-nonvram_save')
        if scenario != 'menu':
            cmd.append('-verbose')
        env = {k: v for k, v in os.environ.items() if not k.startswith('NOKIA_DCT3_')}
        env['NPE3_NATIVE_SCENARIO'] = scenario
        env['NPE3_NATIVE_INPUT_AT'] = str(args.input_at)
        with (directory / 'console.log').open('w') as output:
            subprocess.run(cmd, cwd=directory, env=env, stdout=output,
                           stderr=subprocess.STDOUT, check=True, timeout=600)
        text = (directory / 'error.log').read_text(errors='replace')
        check_execution(text, (directory/'npe3_loader2.be16').read_bytes(),
                        (directory/'npe3_terminal_program.be16').read_bytes(), flash,
                        reference=args.reference_clock, minimum_seconds=seconds)
        for line in text.splitlines():
            if 'npe3_native_' in line:
                print(line)
        return directory, text
    first, text = execute('primary', args.scenario,
                          args.input_at + 10 if args.scenario == 'phonebook' else max(args.seconds, args.input_at + 4),
                          args.scenario == 'phonebook')
    if args.scenario == 'phonebook':
        from PIL import Image
        from tools.run_noki6210_acceptance import check_phonebook
        card = storage / 'sim_card'
        before = card.read_bytes()
        cold, read_text = execute('cold', 'read', args.input_at + 7)
        check_input_start(text, '6210_phonebook_physical: action=names', args.input_at)
        check_input_start(read_text, '6210_phonebook_read_physical: action=names', args.input_at)
        with Image.open(cold / 'snap/6210_phonebook_read_contact.png') as frame:
            check_phonebook(text, read_text, before, frame)
        if before != card.read_bytes():
            raise ValueError('cold read changed retained SIM bytes')
        print('Native6210 phonebook: PASS bounded own uploads, physical save, cold readback')
    else:
        from PIL import Image
        from tools.run_noki6210_acceptance import check_frame
        if ('npe3_native_physical: key=Menu' not in text
                or 'npe3_native_key: value=19' not in text):
            raise ValueError('native 6210 physical Menu was not decoded')
        check_input_start(text, 'npe3_native_physical: key=Menu', args.input_at)
        with Image.open(first/'snap/npe3_after_menu.png') as frame:
            digest = expected_menu_digest(args.adc_source, args.capture_start)
            check_frame(frame, digest, 'native 6210 Messages menu')
        print('Native6210 Menu: PASS bounded native boot and physical input')
    if args.radio:
        from tools.rom6_radio_census import census, find_output_source
        with (first / 'rom6_radio_writes.tsv').open('rb') as stream:
            report = census(text, stream)
        overlay = (first / 'rom6_radio_first_output.be16').read_bytes()
        report['own_output_writer'] = find_output_source(flash, overlay)
        report['fitted_clock_claim'] = False
        report['rf_acquisition_claim'] = False
        (run / 'radio-census.json').write_text(json.dumps(report, indent=2) + '\n')
        print(f"Native6210 radio census: PASS writes={report['write_records']} own_writer={report['own_output_writer']['mcu_offset']} acquisition_claim=0")
    if args.adc_source:
        from tools.rom6_adc_lab_check import check as check_adc
        from tools.rom6_adc_filter_check import check as check_filter, check_mixer
        from tools.rom6_adc_measure_check import check_rx_publications
        broadcast=args.adc_source=='fcch-sch-broadcast'
        bcch=args.adc_source in ('fcch-sch-bcch','fcch-sch-broadcast')
        report = check_adc(text, args.adc_source != 'silence', burst=True, sch=True, bcch=bcch,broadcast=broadcast)
        processing_program = (first / 'rom6_adc_processing_program.be16').read_bytes()
        report['native_filter'] = check_filter(text,
            processing_program, flash,
            (ROOT / 'roms/nsm3r6/nsm3_rom6_program.be16').read_bytes(), product='6210')
        report['native_mixer'] = check_mixer(text)
        report['native_rx'] = check_rx_publications(text)
        if args.capture_start == 'p09':
            from tools.npe3_sch_receive_check import check_compare_edges
            report['candidate_compare_edges'] = check_compare_edges(text)
            if args.adc_source == 'silence':
                if any(packet['type'] == 0x80 and packet['payload'].startswith('400000')
                       for packet in report['native_rx']['packets']):
                    raise ValueError('zero-source control unexpectedly published successful SCH')
                report['zero_source_no_successful_sch'] = True
        if args.adc_source != 'silence':
            from tools.rom6_adc_lab_check import check_active_burst_window
            from tools.npe3_sch_provenance_check import check as check_sch_provenance, check_capture_compare
            from tools.rom6_adc_detect_check import (check_finite_search_acceptance,
                check_finite_peak_accumulation, check_peak_clock)
            report['active_source_window'] = check_active_burst_window(text, sch=True, bcch=bcch,broadcast=broadcast)
            report['sch_sample_provenance'] = check_sch_provenance(text,
                receive_words=992 if args.capture_start == 'p09' else 960)
            if args.capture_start == 'immediate':
                report['capture_compare_hypotheses'] = check_capture_compare(text, report['sch_sample_provenance'])
            else:
                from tools.npe3_sch_receive_check import check as check_sch_result
                report['sch_result'] = check_sch_result(text, report['sch_sample_provenance'],
                    (first / 'npe3_terminal_program.be16').read_bytes(),
                    (first / 'npe3_terminal_data.be16').read_bytes(),
                    (ROOT / 'roms/nsm3r6/nsm3_rom6_program.be16').read_bytes(),
                    (ROOT / 'roms/nsm3r6/nsm3_rom6_data.be16').read_bytes(),
                    allow_reacquisition=broadcast)
            # The later receiver overlay replaces the search upload. Bind its
            # read-only trace to the snapshot taken at the first ADC read.
            report['finite_search'] = check_finite_search_acceptance(text, processing_program, flash, product='6210')
            report['finite_residual'] = check_finite_peak_accumulation(text, processing_program, flash, product='6210')
            report['peak_source_clock'] = check_peak_clock(text,
                index_bias=0 if args.capture_start == 'p09' else 1)
            if report['native_filter']['nonzero_output_words'] == 0:
                raise ValueError('coded raw source has not reached the qualified FIR output prefix')
        (run / 'adc-source.json').write_text(json.dumps(report, indent=2) + '\n')
        if not report['transport_fault_free']:
            raise ValueError('raw ADC transport fault; no decoder/acquisition qualification')
        if bcch:
            from tools.npe3_bcch_receive_check import check as check_bcch,check_broadcast
            report['bcch_result']=(check_broadcast if broadcast else check_bcch)(text,(first/'rom6_radio_writes.tsv').read_text())
            if broadcast:
                from tools.npe3_rach_transmit_check import check as check_access
                report['native_access']=check_access(text,(first/'rom6_radio_writes.tsv').read_text())
                print(f"Native6210 access serialization: PASS attempts={report['native_access']['attempts']}; physical_release=0 registration=0")
            (run/'adc-source.json').write_text(json.dumps(report,indent=2)+'\n')
            print('Native6210 BCCH: PASS '+('complete SI set and MCU acquisition return' if broadcast else 'three raw SI3 blocks')+'; camping=0 registration=0')
        print('Native6210 ADC source: PASS independently checked raw prefix; acquisition_claim=0')
        if 'sch_result' in report:
            result = report['sch_result']
            print(f"Native6210 SCH: PASS FN={result['decoded_frame']} BSIC={result['decoded_bsic']} laboratory_capture=1 physical_claim=0 camping=0")


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        raise SystemExit(f'Native6210 scout failed: {error}')

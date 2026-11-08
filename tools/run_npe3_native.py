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
    parser.add_argument('--adc-source', choices=('silence', 'fcch-sch'),
                        help='explicit raw ADC laboratory source; requires --reference-clock')
    args = parser.parse_args()
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
        from tools.run_noki6210_acceptance import check_frame, MENU_SHA256
        if ('npe3_native_physical: key=Menu' not in text
                or 'npe3_native_key: value=19' not in text):
            raise ValueError('native 6210 physical Menu was not decoded')
        check_input_start(text, 'npe3_native_physical: key=Menu', args.input_at)
        with Image.open(first/'snap/npe3_after_menu.png') as frame:
            digest = ADC_SCH_MENU_SHA256 if args.adc_source == 'fcch-sch' else MENU_SHA256
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
        report = check_adc(text, args.adc_source != 'silence', burst=True, sch=True)
        report['native_filter'] = check_filter(text,
            (first / 'rom6_adc_processing_program.be16').read_bytes(), flash,
            (ROOT / 'roms/nsm3r6/nsm3_rom6_program.be16').read_bytes(), product='6210')
        report['native_mixer'] = check_mixer(text)
        report['native_rx'] = check_rx_publications(text)
        if args.adc_source == 'fcch-sch':
            from tools.rom6_adc_lab_check import check_active_burst_window
            report['active_source_window'] = check_active_burst_window(text, sch=True)
            if report['native_filter']['nonzero_output_words'] == 0:
                raise ValueError('coded raw source has not reached the qualified FIR output prefix')
        (run / 'adc-source.json').write_text(json.dumps(report, indent=2) + '\n')
        if not report['transport_fault_free']:
            raise ValueError('raw ADC transport fault; no decoder/acquisition qualification')
        print('Native6210 ADC source: PASS independently checked raw prefix; acquisition_claim=0')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        raise SystemExit(f'Native6210 scout failed: {error}')

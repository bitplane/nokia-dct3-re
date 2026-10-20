"""Bounded own NSK-1 / NSE-1 ROM4 compatibility observation, not fitted-mask proof."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsk1_static_check import inspect
from tools.nsk1_record_exchange_check import check as check_records
from tools.nsk1_record_profile import build_from_capture,check_observation,repair_security_checksum,initialize_phone_retry
from tools.nsk1_input_check import check as check_input


INPUTS = (
    ('noki5130/5130f561a.fls', 'a2a99ec6c52104eebcbae281cf8e845bdc8ea1d9'),
    ('noki5130/5130f561a-2915.fls', '64068df6b7950b47f34ac141d0ef9c51dc666cca'),
    ('noki5110/nse1_rom4_dsp_program.bin', 'a05a1e96a8c36ec5a47e1ea059d15afa54ca5739'),
    ('noki5110/nse1_rom4_dsp_data.bin', '024c7f970f4ef754d3e90471de48a167515f930d'),
)

EMPTY_SOCKET_RGB_SHA256 = 'a879c658a532b6526d3343287b42848f8a16bf10b7215086c1e215db45ee67cc'
SECURITY_PROMPT_RGB_SHA256 = 'f82d19b12314200132f0d19ab17a2d86b202d65ba7bc817d1a5c6cbb90a59dfc'


def check_nominal_readiness(summary):
    """Common report and physical-key contract for the nominal-input trials."""
    observed = summary.get('physical_input_observation',{})
    contacts = observed.get('contacts',[])
    reports = {row['value'] for row in observed.get('task1_scalars',[])}
    windows = observed.get('power_windows',[])
    if (summary.get('adc2_source') != 554 or summary.get('native_illegal_observed') is not False
            or not {0x14,0x15} <= reports or 0 not in observed.get('report14_countdowns',[])
            or len(contacts) != 6 or any(row['checklist'] != 15 or row['irq_mask'] & 1 for row in contacts)
            or observed.get('forwarded_keys') != [0x19,0x17,0x1a]
            or not windows or any(row['sample'] != 554*1500//232 for row in windows)
            or contacts[-1]['simi_reads'] == 0):
        raise ValueError('nominal-input report/checklist/physical-key boundary changed')


def check_nominal_startup(summary, snapshots):
    """Own bounded graphical/input acceptance, not SIM or radio acceptance."""
    from PIL import Image
    check_nominal_readiness(summary)
    for seconds in (10,20):
        with Image.open(snapshots/f'nsk1_native_{seconds}s.png') as source:
            frame = source.convert('RGB')
            if frame.size != (84,48) or hashlib.sha256(frame.tobytes()).hexdigest() != EMPTY_SOCKET_RGB_SHA256:
                raise ValueError('own NSK-1 Insert SIM card pixels changed')
    return {'empty_socket_rgb_sha256':EMPTY_SOCKET_RGB_SHA256,'physical_keys':[0x19,0x17,0x1a],
            'scope':'native compatibility graphical boot and physical key forwarding; not fitted mask, SIM acceptance or radio'}


def check_card_startup(summary, trace, snapshots):
    """Require the real DCS directory/file conversation, not merely a prompt."""
    from PIL import Image
    check_nominal_readiness(summary)
    headers = re.findall(r'sim_device: header cla=([0-9a-f]{2}) ins=([0-9a-f]{2}) p1=([0-9a-f]{2}) p2=([0-9a-f]{2}) p3=([0-9a-f]{2}) selected=([0-9a-f]{4})',trace)
    if len(headers) != trace.count('sim_device: header '):
        raise ValueError('unrecognized card header trace')
    reads = {(ins,fid,length) for cla,ins,p1,p2,length,fid in headers if cla == 'a0' and p1 == p2 == '00'}
    required_reads = {('b0','2fe2','0a'),('b0','6fae','01'),('b0','6f38','08'),('b0','6f07','09')}
    adn = [int(p1,16) for cla,ins,p1,p2,length,fid in headers
           if (cla,ins,p2,length,fid) == ('a0','b2','04','20','6f3a')]
    if (not required_reads <= reads or adn != list(range(1,51))
            or 'requested=7f21 selected=7f21 result=ok' not in trace
            or 'SIM status ins=b2' in trace):
        raise ValueError('missing DCS application reads or complete empty-ADN scan')
    for seconds in (10,20):
        with Image.open(snapshots/f'nsk1_native_{seconds}s.png') as source:
            frame = source.convert('RGB')
            if frame.size != (84,48) or hashlib.sha256(frame.tobytes()).hexdigest() != SECURITY_PROMPT_RGB_SHA256:
                raise ValueError('own NSK-1 security-prompt pixels changed')
    return {'apdu_headers':[list(row) for row in headers], 'adn_records_read':len(adn),
            'security_prompt_rgb_sha256':SECURITY_PROMPT_RGB_SHA256,
            'scope':'native compatibility SIM initialization and security-prompt presentation; not phone-code acceptance, menu, radio or fitted-mask proof'}


def prepare(root, run, record_profile_from=None, physical_keys=False, security_checksum=False, nominal_adc2=False, insert_sim=False, initialized_retry=False):
    if run.exists():
        raise ValueError('NSK-1 observation requires a fresh run directory')
    if security_checksum and record_profile_from is None:
        raise ValueError('checksum-only comparison requires the qualified record fixture')
    if nominal_adc2 and not security_checksum:
        raise ValueError('nominal ADC2 comparison requires checksum control')
    if insert_sim and not nominal_adc2:
        raise ValueError('inserted-card observation requires nominal-input control')
    if initialized_retry and not insert_sim:
        raise ValueError('phone retry comparison requires inserted-card control')
    sources = [(root / name, digest) for name, digest in INPUTS]
    for source, digest in sources:
        if hashlib.sha1(source.read_bytes()).hexdigest() != digest:
            raise ValueError(f'NSK-1 compatibility input hash differs: {source.name}')
    for index, variant in enumerate(('standard', '2915')):
        inspect(sources[index][0].read_bytes(), variant)
    initial_eeprom,profile_metadata = bytes([255])*2048,None
    if record_profile_from is not None:
        initial_eeprom,profile_metadata = build_from_capture(record_profile_from,INPUTS)
    checksum_metadata = None
    if security_checksum:
        before = initial_eeprom
        initial_eeprom = repair_security_checksum(before)
        checksum_metadata = {'classification':'diagnostic checksum-only external input, not factory data',
                             'stored_before':before[0x11e:0x120].hex(),
                             'stored_after':initial_eeprom[0x11e:0x120].hex(),
                             'written_range':'011e..011f',
                             'eeprom_sha256':hashlib.sha256(initial_eeprom).hexdigest()}
    retry_metadata = None
    if initialized_retry:
        before = initial_eeprom
        initial_eeprom = initialize_phone_retry(before,sources[0][0].read_bytes())
        retry_metadata = {'classification':'diagnostic own retry initialization, not a credential change',
                          'written_range':'02a4','before':before[0x2a4], 'after':initial_eeprom[0x2a4],
                          'eeprom_sha256':hashlib.sha256(initial_eeprom).hexdigest()}
    target = run / 'inputs/nsk1r4t'
    target.mkdir(parents=True)
    for source, _ in sources:
        shutil.copyfile(source, target / source.name)
    if profile_metadata is not None:
        storage = run/'nvram/nsk1r4t'
        storage.mkdir(parents=True)
        (storage/'eeprom').write_bytes(initial_eeprom)
    if nominal_adc2:
        config = ET.Element('mameconfig',version='10')
        system = ET.SubElement(config,'system',name='nsk1r4t')
        inputs = ET.SubElement(system,'input')
        ET.SubElement(inputs,'port',tag=':RESEARCH_ADC2',type='CONFIG',mask='1023',defvalue='1023',value='554')
        if insert_sim:
            ET.SubElement(inputs,'port',tag=':SIM_REMOVED',type='OTHER',mask='1',defvalue='1',value='0')
        (run/'cfg').mkdir()
        ET.ElementTree(config).write(run/'cfg/nsk1r4t.cfg',encoding='utf-8',xml_declaration=True)
    (run / 'input_manifest.json').write_text(json.dumps({
        'inputs': [{'source': name, 'sha1': digest} for name, digest in INPUTS],
        'external_eeprom': ('diagnostic published lock-record example; identity/calibration erased'
                           if profile_metadata else 'own erased 2048 bytes; no donor provisioning'),
        'record_profile':profile_metadata,
        'security_checksum':checksum_metadata,
        'adc2_source':554 if nominal_adc2 else 1023,
        'adc2_scope':'nominal CCONT transfer hypothesis, not measured NSK-1 calibration' if nominal_adc2 else 'generic saturated research control',
        'inserted_synthetic_sim':insert_sim,
        'phone_retry':retry_metadata,
        'resident_mask_and_52mhz_clock': 'NSE-1 compatibility inputs, unproved on NSK-1',
        'peer_completions': False, 'physical_keys': physical_keys,
    }, indent=2)+'\n')
    return initial_eeprom


def summarize(trace, image, variant):
    samples = re.findall(r'nsk1_boot: t=([0-9.]+) pc=([0-9a-f]+).*illegal=([01])', trace)
    if '[LUA ERROR]' in trace or len(samples) != 5 or float(samples[-1][0]) < 5:
        raise ValueError('missing complete passive NSK-1 observation')
    releases = re.findall(r'nsk1_release: index=(\d+) command=01 pc=([0-9a-f]+) args=([0-9a-f]+) program=([0-9a-f]+)', trace)
    descriptor = 0x29c2e4 if variant == 'standard' else 0x29d044
    first_payload = image[descriptor-0x200000+12:descriptor-0x200000+12+372].hex()
    first_pc = '00273504' if variant == 'standard' else '002734f4'
    if not releases or releases[0] != ('1', first_pc, '0100030000008000000100010200', first_payload):
        raise ValueError('first DSP release does not contain own slot-25 payload/arguments')
    for index, _, arguments, _ in releases[1:]:
        slot0 = 0x29728c if variant == 'standard' else 0x29727c
        expected = image[slot0-0x200000:slot0-0x200000+10].hex()
        if arguments[:20] != expected:
            raise ValueError(f'release {index} is not own descriptor-0 transfer')
    presentation = re.findall(r'nsk1_presentation: t=([0-9.]+) commands=(\d+) data=(\d+) nonzero=(\d+) startup_reason=([0-9a-f]{2}) idle_flag=([0-9a-f]{2}) observer_writes=0', trace)
    if len(presentation) != 5 or [row[0] for row in presentation] != [row[0] for row in samples]:
        raise ValueError('missing synchronized LCD/startup observations')
    last = presentation[-1]
    decisions = re.findall(r'nsk1_record_decision: pointer=([0-9a-f]{8}) state=([0-9a-f]{8}) valid=([0-9a-f]{8}) observer_writes=0', trace)
    accepted = re.findall(r'nsk1_record_accept: context=([0-9a-f]{8}) valid=00000001 copied=([0-9a-f]{48}) observer_writes=0',trace)
    late = re.findall(r'nsk1_late: t=([0-9.]+) pc=([0-9a-f]{8}) dsp_pc=([0-9a-f]{4}) illegal=([01]) startup_reason=([0-9a-f]{2})',trace)
    return {'variant': variant, 'samples': len(samples),
            'last_mcu_pc': samples[-1][1],
            'native_illegal_observed': any(row[2] != '0' for row in samples) or any(row[3] != '0' for row in late),
            'releases': len(releases), 'own_slot25_payload_observed': True,
            'descriptor0_release_observed': len(releases) > 1,
            'lcd_commands': int(last[1]), 'lcd_data_bytes': int(last[2]),
            'lcd_nonzero_bytes': int(last[3]),
            'startup_reasons': [row[4] for row in presentation],
            'record_decisions': [{'pointer': pointer, 'state': int(state,16), 'valid': int(valid,16)}
                                 for pointer,state,valid in decisions],
            'record_accepts':[{'context':context,'copied':copied} for context,copied in accepted],
            'late_samples':[{'time':float(time),'mcu_pc':pc,'dsp_pc':dsp_pc,'startup_reason':reason}
                            for time,pc,dsp_pc,_,reason in late],
            'graphical_or_fitted_hardware_proved': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('roms', type=Path)
    parser.add_argument('run', type=Path)
    parser.add_argument('--variant', choices=('standard', '2915'), default='standard')
    parser.add_argument('--debug-startup', action='store_true', help='bounded read-only own startup entry traces')
    parser.add_argument('--record-profile-from',type=Path,help='diagnostic external record example keyed only from a qualified own erased capture')
    parser.add_argument('--seconds',type=int,choices=range(6,62),default=6)
    parser.add_argument('--physical-keys',action='store_true',help='physical Menu/Up/C probe on independently mapped contacts')
    parser.add_argument('--security-checksum',action='store_true',help='repair only 011e/011f in the qualified external record fixture')
    parser.add_argument('--nominal-adc2',action='store_true',help='explicit 554-count CCONT nominal-input hypothesis, no guest sample writes')
    parser.add_argument('--insert-sim',action='store_true',help='cold physical insertion of the explicit synthetic card; no service acceptance claim')
    args = parser.parse_args()
    run = args.run.resolve()
    if args.record_profile_from and args.variant != 'standard':
        parser.error('record-profile experiment is not established for the 2915 family')
    if args.record_profile_from and not args.debug_startup:
        parser.error('record-profile experiment requires --debug-startup for own accepted-copy evidence')
    if args.physical_keys and (not args.debug_startup or args.seconds < 21):
        parser.error('physical keys require --debug-startup and at least 21 seconds')
    if args.security_checksum and (not args.record_profile_from or not args.physical_keys):
        parser.error('checksum comparison requires --record-profile-from and --physical-keys')
    if args.nominal_adc2 and not args.security_checksum:
        parser.error('nominal ADC2 comparison requires --security-checksum')
    if args.insert_sim and not args.nominal_adc2:
        parser.error('inserted-card observation requires --nominal-adc2')
    try:
        initial_eeprom = prepare(args.roms.resolve(), run,args.record_profile_from,args.physical_keys,args.security_checksum,args.nominal_adc2,args.insert_sim)
        script = Path(__file__).with_name('nsk1_input_fixture.lua' if args.physical_keys else 'nsk1_boot_observe.lua').resolve()
        with (run / 'mame_output.log').open('w') as output:
            result = subprocess.run([
                str(args.binary.resolve()), 'nsk1r4t', '-bios',
                '561a' if args.variant == 'standard' else '561a2915',
                '-rompath', str(run / 'inputs'), '-noreadconfig', '-video', 'none',
                '-sound', 'none', '-nothrottle', '-log', '-seconds_to_run', str(args.seconds),
                *(['-verbose'] if args.insert_sim else []),
                *(['-debug', '-debugger', 'none'] if args.debug_startup else []),
                '-autoboot_delay', '0', '-autoboot_script', str(script),
            ], cwd=run, stdout=output, stderr=subprocess.STDOUT, timeout=180)
        (run / 'exit_status.json').write_text(json.dumps({'returncode': result.returncode})+'\n')
        if result.returncode:
            raise ValueError(f'MAME exited {result.returncode}; inspect mame_output.log')
        trace = (run / 'error.log').read_text()
        storage = run / 'nvram' / ('nsk1r4t' if args.variant == 'standard' else 'nsk1r4t_1')
        image_index = 0 if args.variant == 'standard' else 1
        image = (run/'inputs/nsk1r4t'/Path(INPUTS[image_index][0]).name).read_bytes()
        if hashlib.sha1((storage/'flash').read_bytes()).hexdigest() != INPUTS[image_index][1]:
            raise ValueError('own MCU/PPM changed during observation')
        if len((storage/'eeprom').read_bytes()) != 2048:
            raise ValueError('own EEPROM storage capacity changed')
        summary = summarize(trace,image,args.variant)
        expected_adc = 554 if args.nominal_adc2 else 1023
        configured_adc = re.findall(r'ccont_adc2_config: raw=([0-9a-f]{3}) external_input=1',trace)
        if not configured_adc or any(int(value,16) != expected_adc for value in configured_adc):
            raise ValueError('requested physical ADC2 configuration was not applied')
        summary['adc2_source'] = expected_adc
        required_late = [time for time in (10,20,30,40,50,59) if time <= args.seconds-1]
        allowed_late = [time for time in (10,20,30,40,50,59) if time <= args.seconds]
        if not len(required_late) <= len(summary['late_samples']) <= len(allowed_late) or any(
                not expected <= sample['time'] < expected+0.1
                for expected,sample in zip(allowed_late,summary['late_samples'])):
            raise ValueError('missing or mistimed late native observation')
        summary['unchanged_flash'] = True
        if args.security_checksum:
            # The checksum makes the firmware's record-update lifecycle live;
            # its new 35/34 notification is distinct from a 35/32 read reply.
            notifications = [line for line in trace.splitlines()
                             if 'nsk1_record_reply:' in line and 'payload=35340000' in line]
            read_trace = '\n'.join(line for line in trace.splitlines() if line not in notifications)
            summary['record_arithmetic'] = check_records(read_trace,minimum=2)
            observation = check_input(trace)
            if (not notifications or not observation['security_checksums']
                    or any(row['computed'] != row['stored'] for row in observation['security_checksums'])
                    or not any(row['predicate'] == 1 for row in observation['application_gates'])
                    or not (set(range(10,19)) | {20,21}) <= {row['value'] for row in observation['ready_inputs']}
                    or not any(row['value'] == 0x15 for row in observation['task1_scalars'])):
                raise ValueError('checksum experiment did not reproduce checksum/activation/report-15 evidence')
            summary['checksum_experiment'] = {
                'application_activation_observed':True,'report_15_observed':True,
                'record_update_notifications':notifications,
                'record_update_semantics':'observed, not yet qualified',
                'final_eeprom_sha256':hashlib.sha256((storage/'eeprom').read_bytes()).hexdigest(),
                'graphical_or_interactive_acceptance':False}
        else:
            summary['record_arithmetic'] = (check_records(trace,minimum=1 if args.record_profile_from else 2,initial_eeprom=initial_eeprom)
                                        if args.variant == 'standard' else {'comparison_performed':False,'reason':'2915 record lifecycle not qualified'})
        if args.record_profile_from and not args.security_checksum:
            check_observation(summary,initial_eeprom,(storage/'eeprom').read_bytes())
            summary['synthetic_record_conformance'] = True
        if args.physical_keys:
            summary['physical_input_observation'] = check_input(trace)
        if args.nominal_adc2 and not args.insert_sim:
            summary['nominal_input_presentation'] = check_nominal_startup(summary,run/'snap')
        if args.insert_sim:
            summary['card_present_observation'] = check_card_startup(summary,trace,run/'snap')
        (run / 'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
        print(json.dumps(summary, indent=2))
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'NSK-1 observation failed: {error}\n')


if __name__ == '__main__':
    main()

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
from tools.process_runner import run_emulator
from tools.nse2_native_check import verify_native, verify_menu_probe, verify_nominal_voltage
from tools.nse2_sim_check import verify_sim_reads, verify_security_attempt, verify_post_auth_menu
from tools.nse2_lifecycle_check import (
    diagnose_processing, diagnose_schedule, diagnose_transport,
    diagnose_scan_progress, diagnose_host_packets,
)
from tools.nse2_native_result_check import (inspect_capture, inspect_control_trace,
                                           inspect_request_lifecycle, inspect_scan_pacing,
                                           inspect_completion_counters, inspect_publisher_runtime,inspect_scan_request_selection,inspect_scan_bitmap_source,inspect_scan_bitmap_writers,inspect_receive_activity)
from tools.nse2_sampling_check import inspect_sampling_controls,inspect_peripheral_routes,inspect_peripheral_consumers,inspect_word_port_activity,inspect_word_control_contract


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
    parser.add_argument('--phonebook', action='store_true', help='physical Add name and real SIM write experiment')
    parser.add_argument('--phonebook-reload', type=Path, help='card NVRAM produced by a separate accepted contact save')
    parser.add_argument('--phonebook-search', action='store_true', help='physical Search A on the fresh erased card (negative control)')
    parser.add_argument('--sms-store', action='store_true', help='physical offline SMS draft experiment')
    parser.add_argument('--sms-reload', type=Path, help='card NVRAM produced by a separate accepted draft save')
    parser.add_argument('--sms-action', choices=('erase','cancel','empty'), help='physical confirmation or erased-card Outbox check')
    parser.add_argument('--pin-enabled', action='store_true', help='separate PIN-enabled synthetic-card boot observation')
    parser.add_argument('--pin-action', choices=('verify','wrong','blocked','puk','reauth'), help='physical PIN/PUK attempt or cold prompt control')
    parser.add_argument('--pin-card', type=Path, help='persistent PIN-enabled laboratory card from an earlier process')
    parser.add_argument('--puk-attempts',type=int,choices=range(1,11),default=10,help='explicit expected PUK1 retries for blocked/puk input')
    parser.add_argument('--pin2-action',choices=('change','restore','wrong','blocked','puk2','puk2-wrong','permanent'),help='physical security-menu PIN2/PUK2 lifecycle')
    parser.add_argument('--pin2-card',type=Path,help='complete saved card for a cold PIN2 lifecycle step')
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
        if args.phonebook and (not args.post_auth_menu or args.seconds != 30 or args.native_processing):
            raise ValueError('phonebook experiment requires post-auth Menu, thirty seconds and no processing census')
        if args.phonebook_reload and (not args.post_auth_menu or args.seconds != 30 or args.native_processing or args.phonebook):
            raise ValueError('phonebook reload requires separate post-auth Menu run, thirty seconds and no save/census')
        if args.phonebook_search and (not args.post_auth_menu or args.seconds != 30 or args.native_processing or args.phonebook or args.phonebook_reload):
            raise ValueError('erased-card search requires a separate thirty-second post-auth run')
        if args.sms_store and (not args.phone_retry_zero or args.seconds != 30 or args.post_auth_menu
                              or args.native_processing or args.phonebook or args.phonebook_reload or args.phonebook_search):
            raise ValueError('SMS experiment requires authenticated thirty-second run without another application fixture')
        if args.sms_reload and (not args.phone_retry_zero or args.seconds!=30 or args.sms_store or args.post_auth_menu
                               or args.native_processing or args.phonebook or args.phonebook_reload or args.phonebook_search):
            raise ValueError('cold SMS read requires a separate authenticated thirty-second run')
        if args.sms_action and not args.sms_reload:
            raise ValueError('SMS confirmation action requires a separate saved-card reload')
        if args.pin_enabled and (not args.insert_sim or args.seconds!=(28 if args.pin_action=='puk' else 14) or args.physical_menu or args.security_code or args.phone_retry_zero
                                 or args.post_auth_menu or args.native_processing or args.sms_store or args.sms_reload):
            raise ValueError('PIN-enabled observation requires inserted card, fourteen seconds and no phone-code/application fixture')
        if args.pin_action and not args.pin_enabled:
            raise ValueError('SIM PIN input requires the explicit PIN-enabled card')
        if args.puk_attempts!=10 and (args.pin_action not in ('blocked','puk') or not args.pin_card):
            raise ValueError('reduced PUK1 retries require an imported blocked/recovery card')
        if args.pin_card and not args.pin_enabled:
            raise ValueError('persistent PIN card requires the PIN-enabled boot fixture')
        if args.pin_action in ('blocked','puk','reauth') and not args.pin_card:
            raise ValueError('blocked PIN control requires the card saved by actual PIN exhaustion')
        if args.pin2_action and (not args.phone_retry_zero or args.seconds!=30 or args.pin_enabled or args.post_auth_menu
                               or args.sms_store or args.sms_reload or args.phonebook or args.phonebook_reload or args.phonebook_search):
            raise ValueError('PIN2 menu requires a separate phone-authenticated thirty-second run')
        if (args.pin2_card and not args.pin2_action) or (args.pin2_action in ('restore','blocked','puk2','puk2-wrong','permanent') and not args.pin2_card):
            raise ValueError('cold PIN2 restoration requires the saved changed card')
        report = prepare_native(root, run, record_checksum=args.record_checksum_only,
                                nominal_vbat=args.nominal_vbat, insert_sim=args.insert_sim,
                                phone_retry_zero=args.phone_retry_zero)
        machine = report['machine']
        if args.pin2_action:
            from tools.make_sim_card_profile import make_profile
            from tools.nse2_sim_check import verify_pin2_input
            pin2_card=args.pin2_card.read_bytes() if args.pin2_card else make_profile(False)
            verify_pin2_input(pin2_card,restore=args.pin2_action=='restore',blocked=args.pin2_action in ('blocked','puk2','puk2-wrong','permanent'),
                              permanent=args.pin2_action=='permanent')
            card_path=run/'nvram'/machine/'sim_card'
            card_path.parent.mkdir(parents=True,exist_ok=True)
            card_path.write_bytes(pin2_card)
            report['pin2_card']=dict(sha256=hashlib.sha256(pin2_card).hexdigest(),source=str(args.pin2_card.resolve()) if args.pin2_card else 'laboratory default')
        if args.pin_enabled:
            from tools.make_sim_card_profile import make_profile
            from tools.nse2_sim_check import verify_pin_input
            pin_card=args.pin_card.read_bytes() if args.pin_card else make_profile(True)
            verify_pin_input(pin_card,blocked=args.pin_action in ('blocked','puk'),pin=b'2468' if args.pin_action=='reauth' else b'1234',puk_attempts=args.puk_attempts)
            card_path=run/'nvram'/machine/'sim_card'
            card_path.parent.mkdir(parents=True,exist_ok=True)
            card_path.write_bytes(pin_card)
            report['pin_card']=dict(pin_enabled=True,sha256=hashlib.sha256(pin_card).hexdigest(),
                                    source=str(args.pin_card.resolve()) if args.pin_card else 'laboratory profile; not handset EEPROM provisioning')
        if args.phonebook_reload:
            from tools.sim_phonebook_check import validate_phonebook_storage
            saved_card = args.phonebook_reload.read_bytes()
            validate_phonebook_storage(saved_card,b'A')
            card_path = run / 'nvram' / machine / 'sim_card'
            card_path.parent.mkdir(parents=True,exist_ok=True)
            card_path.write_bytes(saved_card)
            report['saved_card'] = dict(source=str(args.phonebook_reload.resolve()),
                                       sha256=hashlib.sha256(saved_card).hexdigest())
        if args.sms_reload:
            from tools.sim_sms_draft import check_card,check_deleted_card
            saved_sms_card=args.sms_reload.read_bytes()
            (check_deleted_card if args.sms_action=='empty' else check_card)(saved_sms_card)
            card_path=run/'nvram'/machine/'sim_card'
            card_path.parent.mkdir(parents=True,exist_ok=True)
            card_path.write_bytes(saved_sms_card)
            report['saved_sms_card']=dict(source=str(args.sms_reload.resolve()),
                                          sha256=hashlib.sha256(saved_sms_card).hexdigest())
        command = [str(mame), machine, '-rompath', str(run / 'roms'),
                   '-nvram_directory', str(run / 'nvram'), '-cfg_directory', str(run / 'cfg'),
                   '-video', 'none', '-sound', 'none', '-joystickprovider', 'none',
                   '-midiprovider', 'none', '-nothrottle', '-noreadconfig', '-log', '-verbose',
                   '-seconds_to_run', str(args.seconds), '-autoboot_delay', '0',
                   '-autoboot_script', str(root / 'tools' / ('nse2_pin2_permanent_fixture.lua' if args.pin2_action=='permanent' else
                       'nse2_puk2_wrong_fixture.lua' if args.pin2_action=='puk2-wrong' else
                       'nse2_puk2_fixture.lua' if args.pin2_action=='puk2' else
                       'nse2_pin2_blocked_fixture.lua' if args.pin2_action=='blocked' else
                       'nse2_pin2_wrong_fixture.lua' if args.pin2_action=='wrong' else
                       'nse2_pin2_restore_fixture.lua' if args.pin2_action=='restore' else
                       'nse2_pin2_fixture.lua' if args.pin2_action else
                       'nse2_puk_fixture.lua' if args.pin_action=='puk' else
                       f'nse2_pin_{args.pin_action}_fixture.lua' if args.pin_action in ('verify','wrong','blocked','reauth') else
                       f'nse2_sms_{args.sms_action}_fixture.lua' if args.sms_action else
                       'nse2_sms_reload_fixture.lua' if args.sms_reload else
                       'nse2_sms_fixture.lua' if args.sms_store else
                       'nse2_phonebook_reload_fixture.lua' if args.phonebook_reload or args.phonebook_search else
                       'nse2_phonebook_fixture.lua' if args.phonebook else
                       'nse2_processing_fixture.lua' if args.native_processing else
                       'nse2_post_auth_fixture.lua' if args.post_auth_menu else
                       'nse2_security_fixture.lua' if args.security_code else
                       'nse2_menu_fixture.lua' if args.physical_menu else 'nse2_boot_observe.lua'))]
        with mame.open('rb') as stream:
            report['mame_sha256'] = hashlib.file_digest(stream, 'sha256').hexdigest()
        report['command'] = command
        (run / 'inputs.json').write_text(json.dumps(report, indent=2) + '\n')
        with (run / 'console.log').open('w') as stream:
            returncode = run_emulator(command, run, stream)
        if returncode:
            raise ValueError(f'native MAME exit {returncode}')
        console = (run / 'console.log').read_text(errors='replace')
        if 'LUA ERROR' in console or 'Error in' in console:
            raise ValueError('native observer failed')
        source = int(report['loader']['entries'][25]['descriptor'], 16) - 0x200000 + 12
        flash = (run / 'roms' / machine / '5110i_5.53a.fls').read_bytes()
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
            if args.pin_enabled and args.pin_action=='puk':
                from tools.nse2_sim_check import verify_puk_recovery
                with Image.open(run/'snap/nse2_puk_probe_1.png') as prompt, Image.open(run/'snap/nse2_puk_probe_5.png') as final:
                    result.update(verify_puk_recovery(text,pin_card,(run/'nvram'/machine/'sim_card').read_bytes(),
                                                     prompt.convert('L').tobytes(),final.convert('L').tobytes(),puk_attempts=args.puk_attempts))
            elif args.pin_enabled:
                from tools.nse2_sim_check import verify_pin_boot
                prompt_name=('nse2_pin_1.png' if args.pin_action in ('verify','wrong','reauth') else
                             'nse2_pin_blocked.png' if args.pin_action=='blocked' else 'nse2_7.png')
                with Image.open(run/'snap'/prompt_name) as prompt:
                    if args.pin_action in ('verify','wrong','reauth'):
                        with Image.open(run/'snap/nse2_pin_2.png') as outcome, \
                             Image.open(run/'snap/nse2_pin_3.png') as continuation:
                            result.update(verify_pin_boot(text,(run/'nvram'/machine/'sim_card').read_bytes(),
                                prompt.convert('L').tobytes(),outcome.convert('L').tobytes(),
                                continuation.convert('L').tobytes(),action='verify' if args.pin_action=='reauth' else args.pin_action,initial_card=pin_card,
                                pin=b'2468' if args.pin_action=='reauth' else b'1234'))
                            if args.pin_action in ('verify','reauth'):
                                result.update(verify_sim_reads(text,continuation.convert('L').tobytes(),
                                                             pin_authenticated=True,bootstrap_horizon=14))
                    else:
                        result.update(verify_pin_boot(text,(run/'nvram'/machine/'sim_card').read_bytes(),
                                                     prompt.convert('L').tobytes(),initial_card=pin_card,action=args.pin_action,puk_attempts=args.puk_attempts))
            else:
                with Image.open(run / 'snap/nse2_7.png') as frame:
                    result.update(verify_sim_reads(text, frame.convert('L').tobytes(),later_chv2=args.pin2_action=='puk2'))
            if args.security_code:
                with Image.open(run / 'snap/nse2_security_result.png') as frame:
                    result.update(verify_security_attempt(text, frame.convert('L').tobytes(), retry_zero=args.phone_retry_zero,
                                                         contact_start=9 if args.pin2_action in ('puk2','puk2-wrong') else 12))
            if args.post_auth_menu:
                with Image.open(run / 'snap/nse2_post_auth_21.png') as menu, \
                        Image.open(run / 'snap/nse2_post_auth_23.png') as submenu:
                    result.update(verify_post_auth_menu(text, menu.convert('L').tobytes(), submenu.convert('L').tobytes()))
        if args.native_processing:
            result['native_processing_boundary'] = dict(diagnose_schedule(text), **diagnose_transport(text))
            result['native_processing_boundary']['sampling_controls'] = inspect_sampling_controls(
                (run/'nse2_native_program.bin').read_bytes(),text)
            result['native_processing_boundary']['peripheral_routes'] = inspect_peripheral_routes(
                (run/'nse2_native_program.bin').read_bytes())
            result['native_processing_boundary']['peripheral_consumers'] = inspect_peripheral_consumers(
                (run/'nse2_native_program.bin').read_bytes())
            result['native_processing_boundary']['word_port_activity'] = inspect_word_port_activity(
                (run/'roms'/machine/'nse1_rom4_dsp_data.bin').read_bytes(),text)
            result['native_processing_boundary']['word_control_contract'] = inspect_word_control_contract(
                (run/'nse2_native_program.bin').read_bytes())
            result['native_processing_boundary']['conditional_request'] = inspect_control_trace(text)
            result['native_processing_boundary']['request_lifecycle'] = inspect_request_lifecycle(text)
            result['native_processing_boundary']['scan_request_selection'] = inspect_scan_request_selection(text)
            result['native_processing_boundary']['scan_bitmap_source'] = inspect_scan_bitmap_source(flash,text)
            result['native_processing_boundary']['scan_bitmap_writers'] = inspect_scan_bitmap_writers(flash,text)
            result['native_processing_boundary']['receive_activity'] = inspect_receive_activity(flash,text)
            result['native_processing_boundary']['scan_pacing'] = inspect_scan_pacing(text)
            result['native_processing_boundary']['completion_counters'] = inspect_completion_counters(text)
            result['native_processing_boundary']['publisher_runtime'] = inspect_publisher_runtime(text)
            result['native_processing_boundary']['processing'] = diagnose_processing(text)
            result['native_processing_boundary']['measurement'] = diagnose_scan_progress(text)
            result['native_processing_boundary']['host_packets'] = diagnose_host_packets(text)
            result['native_processing_boundary']['result_paths'] = inspect_capture(
                (run / 'nse2_native_program.bin').read_bytes(),
                (run / 'roms' / machine / 'nse1_rom4_dsp_program.bin').read_bytes(),
                (run / 'roms' / machine / 'nse1_rom4_dsp_data.bin').read_bytes())
        if args.pin2_action:
            from tools.nse2_sim_check import verify_pin2_change,verify_pin2_blocked,verify_puk2_recovery
            frames=[]
            for index in range(1,6):
                prefix='nse2_puk2' if args.pin2_action in ('puk2','puk2-wrong') else 'nse2_pin2_menu'
                with Image.open(run/f'snap/{prefix}_{index}.png') as frame:
                    frames.append(frame.convert('L').tobytes())
            if args.pin2_action in ('puk2','puk2-wrong'):
                result['pin2_change']=verify_puk2_recovery(text,pin2_card,(run/'nvram'/machine/'sim_card').read_bytes(),frames,
                                                        wrong=args.pin2_action=='puk2-wrong')
            elif args.pin2_action in ('blocked','permanent'):
                result['pin2_change']=verify_pin2_blocked(text,pin2_card,(run/'nvram'/machine/'sim_card').read_bytes(),frames,
                                                       permanent=args.pin2_action=='permanent')
            else:
                result['pin2_change']=verify_pin2_change(text,pin2_card,(run/'nvram'/machine/'sim_card').read_bytes(),frames,
                                                       restore=args.pin2_action=='restore',wrong=args.pin2_action=='wrong')
        if args.phonebook:
            from tools.nse2_sim_check import verify_phonebook_save
            with Image.open(run / 'snap/nse2_phonebook_5.png') as saved:
                result['phonebook'] = verify_phonebook_save(text,
                    (run / 'nvram' / machine / 'sim_card').read_bytes(), saved.convert('L').tobytes())
        if args.phonebook_reload or args.phonebook_search:
            from tools.nse2_sim_check import verify_phonebook_reload
            with Image.open(run / 'snap/nse2_phonebook_reload_2.png') as frame:
                result['phonebook_search'] = verify_phonebook_reload(text,
                    saved_card if args.phonebook_reload else None,
                    (run / 'nvram' / machine / 'sim_card').read_bytes(),frame.convert('L').tobytes())
            result['contact_persistence'] = bool(args.phonebook_reload)
        if args.sms_store:
            from tools.nse2_sim_check import verify_sms_save
            with Image.open(run / 'snap/nse2_sms_8.png') as frame:
                result['sms'] = verify_sms_save(text,(run/'nvram'/machine/'sim_card').read_bytes(),
                                                frame.convert('L').tobytes())
        if args.sms_reload:
            if args.sms_action:
                from tools.nse2_sim_check import verify_sms_action
                index=3 if args.sms_action=='empty' else 5
                with Image.open(run/'snap/nse2_sms_reload_4.png') as prompt, \
                     Image.open(run/f'snap/nse2_sms_reload_{index}.png') as frame:
                    result['sms']=verify_sms_action(text,saved_sms_card,
                        (run/'nvram'/machine/'sim_card').read_bytes(),prompt.convert('L').tobytes(),
                        frame.convert('L').tobytes(),args.sms_action)
            else:
                from tools.nse2_sim_check import verify_sms_reload
                with Image.open(run/'snap/nse2_sms_reload_4.png') as frame:
                    result['sms']=verify_sms_reload(text,saved_sms_card,
                        (run/'nvram'/machine/'sim_card').read_bytes(),frame.convert('L').tobytes())
                result['sms_persistence']=True
        (run / 'acceptance.json').write_text(json.dumps(result, indent=2) + '\n')
        if args.pin_enabled:
            print('NSE-2 physical SIM PIN ' + (args.pin_action or 'wait') +
                  ' PASS; phone-side code remains locked')
        elif args.pin2_action:
            print('NSE-2 physical PIN2 '+args.pin2_action+' PASS; network/stable long boot unproved')
        elif args.sms_store:
            print('NSE-2 physical offline SMS draft save PASS; cold readback/network SMS unproved')
        elif args.sms_reload:
            print('NSE-2 physical SMS ' + (args.sms_action or 'cold Outbox read') +
                  ' PASS; network SMS/stable long boot unproved')
        elif args.phonebook_reload or args.phonebook_search:
            print('NSE-2 physical contact search PASS; ' +
                  ('cold saved-card readback' if args.phonebook_reload else 'erased-card negative control'))
        elif args.phonebook:
            print('NSE-2 physical contact save PASS; cold reload and stable long boot unproved')
        elif args.post_auth_menu:
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

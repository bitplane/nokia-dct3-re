#!/usr/bin/env python3
"""Native lab calls on the products trialled with the acquired 8210 ROM6.

Each machine runs its own MCU and DSP uploads with the 6210's laboratory cell
(direct ADC, untuned). With the synthetic record fixture
(make_rom6_record_profile.py) it must register, ring with the caller's number,
answer with Send, carry lab tones both ways on TCH/F and clear with End.
--scenario sip-incoming or sip-outgoing runs the call against a real PJSUA
party through the SIP bridge instead; --roundtrip-at saves, replays and
reloads the lab call. --scenario sms-incoming or sms-outgoing switches the
cell to its session-driven SDCCH (LAB_BTS 0x4000) and receives and reads
"hello", or composes and sends "A" to 5551234, with the phone's own keys.
--acquired-records is the negative control: the donor's records fail the
DSP's own validation and the handset cannot register.
"""
import argparse
import asyncio
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
if __package__ in (None, ''):
    sys.path.insert(0, str(ROOT))
from tools.make_rom6_record_profile import PRODUCTS, make_profile  # noqa: E402

DIALLED = '1234567'
ROUNDTRIPS = {'ringing': 3.4, 'connected': 8.5, 'clearing': 14.4}  # after the page
MODELS = {  # page time, security code, Send field and reviewed LCD pixels (call; SMS "hello")
    'nsm2': dict(name='8850', page=44, code=True, send='Call / Send',
        sms_incoming_frames={'sms_read_2.png':
                    '6a101291e5215012ec718748cca30958cf55e688c0075678c34bcd30c2c9eea3'},
        sms_outgoing_frames={'sms_sent.png':
                    '1eda22f78cde69cd4f9add1c90732b07254f3742ade7aede18cb818219712a78'},
        frames={
        'idle.png': '08737c08772c99df2b41fc8560587820c2c209f5f74c12893a657bd6f623e555',
        'ringing.png': '5e45fce4598de194cdc1a2a5ceb3feb728bec391040a6ad52b28839f56213389'}),
    'nhm3': dict(name='6250', page=24, code=False, send='Send', softkey='Left Softkey / Menu',
        sms_incoming_frames={'sms_read_1.png':
                    'ab21e640456a297698ff12e89d315fb469eca215975b8ba4cc5a1a9cb2a41be3'},
        sms_outgoing_frames={'sms_sent.png':
                    '6572493289ae45122d886b82085470e74ecd45c5dfe057c83ef14a1dda9b6adb'},
        frames={
        'idle.png': 'f130f1115e9561b5786c95fc6e8a47ff44b57156640b4b88a0510b170c7efa66',
        'ringing.png': '7d5d7b6fc9f9562faf52cfa1a7f3b9bfcaf56b1611d0fba626e976174225535d'}),
    'nsm3': dict(name='8210', page=44, code=True, send='Call / Send',
        sms_incoming_frames={'sms_read_2.png':
                    '6a101291e5215012ec718748cca30958cf55e688c0075678c34bcd30c2c9eea3'},
        sms_outgoing_frames={'sms_sent.png':
                    'fcae4c528cb5e8382886e6516531cd14aae80fcbd91cc9c49112d0bb85dcd269'},
        frames={
        'idle.png': '08737c08772c99df2b41fc8560587820c2c209f5f74c12893a657bd6f623e555',
        'ringing.png': '5e45fce4598de194cdc1a2a5ceb3feb728bec391040a6ad52b28839f56213389'}),
    'nsb6': dict(name='8890', page=44, code=True, dismiss='Names / C', send='Call / Send',
        sms_incoming_frames={'sms_read_2.png':
                    '2e2f140c3d6784a75a5c2488c202ddbd4fcb4980f28e618ce7b3610f150b7dde'},
        sms_outgoing_frames={'sms_sent.png':
                    '2c5c80f7003469d6269bf63e497fd96f1a5c1578c7cad35881e113e5294cb560'},
        frames={
        'idle.png': 'c36fbbdd8756ea5f1964222761b2c92624d46a7182f089157171a0808ed884bc',
        'ringing.png': 'a8fe20ecce53006a7ccd8b52c038d468bd7a9f81bff40f540a7872f9610cdc93'}),
}


def prepare(base, key, provision=True):
    product = PRODUCTS[key]
    flash = (ROOT / 'roms' / product.mcu).read_bytes()
    pmm = (ROOT / 'roms' / product.pmm).read_bytes()
    profile = make_profile(product, flash, pmm, (ROOT / 'roms/nsm3r6/nsm3_rom6_data.be16').read_bytes())
    machine = f'{key}ap5'
    (base / 'nvram' / machine).mkdir(parents=True)
    (base / 'nvram' / machine / 'flash').write_bytes(
        flash.ljust(product.flash, b'\xff') + (profile if provision else pmm))
    (base / 'roms').mkdir()
    (base / 'roms' / machine).symlink_to(ROOT / 'roms/nsm3r6', target_is_directory=True)
    return machine


REGISTRATION = (
    r'npe3_raw_agch: ', r'npe3_bts_uplink: start_frame=\d+ octets=013f490508', r'npe3_raw_ua: ',
    r'npe3_raw_lu_accept: ', r'npe3_bts_uplink: start_frame=\d+ octets=032101', r'npe3_raw_release: ',
    r'npe3_raw_disc_ua: ')
PAGED_CALL = REGISTRATION + (r'npe3_bts_page_access: ', r'npe3_raw_call_setup: ', r'npe3_raw_assignment: ')


def check_registration(text, exchange=PAGED_CALL):
    """Own SABM/LU, its UA and Accept, the RR acknowledgement and release, in order."""
    at = -1
    for pattern in exchange:
        match = re.compile(pattern).search(text, at + 1)
        if not match:
            raise ValueError(f'native exchange is missing or out of order: {pattern}')
        at = match.start()


def schedule(model, scenario):
    """Input events as (time, field or @snapshot, seconds), and the run length."""
    page, events = model['page'], []
    if model['code']:
        keys = ['Keypad 1', 'Keypad 2', 'Keypad 3', 'Keypad 4', 'Keypad 5', 'Menu']
        events += [(12 + 0.5 * n, key, 0.15) for n, key in enumerate(keys)]
    if model.get('dismiss'):
        events += [(28, model['dismiss'], 0.15)]
    if scenario == 'sip-outgoing':
        keys = [f'Keypad {digit}' for digit in DIALLED] + [model['send']]
        events += [(page + n, key, 0.15) for n, key in enumerate(keys)]
        return events + [(page + 24, 'End', 0.15)], page + 30
    if scenario == 'sms-incoming':
        # The lab pages for its queued SMS; the phone reads it with its own keys.
        softkey = model.get('softkey', 'Menu')
        return events + [(page, 'Laboratory BTS page', 0.05), (page + 10, '@sms_received', 0),
                         (page + 10.1, softkey, 0.15), (page + 12, '@sms_read_1', 0),
                         (page + 12.1, softkey, 0.15), (page + 14, '@sms_read_2', 0)], page + 15
    if scenario == 'sms-outgoing':
        # Compose "A" to 5551234 with the phone's own keys and send it.
        digits = [f'Keypad {d}' for d in '5551234']
        if 'softkey' in model:  # 6210-style: Messages, Message list (Write first)
            keys = [model['softkey']] * 3 + ['Keypad 2'] + [model['softkey']] * 2 + digits + [model['softkey']]
        else:  # Navi key: Messages, Inbox, Outbox, Write messages; Options, Send
            keys = (['Menu', 'Menu', 'Scroll Down', 'Scroll Down', 'Menu', 'Keypad 2', 'Menu', 'Menu'] +
                    digits + ['Menu'])
        events += [(page + n, key, 0.15) for n, key in enumerate(keys)]
        done = page + len(keys)
        # The phone's "Message sent" note follows the network's RP-ACK.
        return events + [(done + 2.5, '@sms_sent', 0)], done + 3
    if scenario == 'sip-incoming':
        # The host pages once the bridge has registered; it hangs up too.
        return events + [(page + 4, model['send'], 0.15)], page + 24
    events += [(page - 0.1, '@idle', 0), (page, 'Laboratory BTS page', 0.05), (page + 3.9, '@ringing', 0),
               (page + 4, model['send'], 0.15), (page + 14, 'End', 0.15)]
    return events, page + 20


def check_inputs(text, events):
    keys = re.findall(r'rom6_call_input: key=(.+) t=[\d.]+', text)
    if keys != [name for _, name, _ in events if not name.startswith('@')]:
        raise ValueError('call inputs did not complete')


def check_frames(directory, frames):
    from PIL import Image
    for name, digest in frames.items():
        with Image.open(directory / name) as image:
            actual = hashlib.sha256(image.convert('L').tobytes()).hexdigest()
        if actual != digest:
            raise ValueError(f'{name} differs from its reviewed frame: {actual}')


def check(text, run, model, events, scenario, at):
    from tools.rom4_lab_registration_check import (HANDSET_CLEARING, OUTGOING_PHASES, check_host_lab,
                                                   check_roundtrip, check_traffic_call)
    if 'LUA ERROR' in (run / 'console.log').read_text(errors='replace') or 'LUA ERROR' in text:
        raise ValueError('call script failed')
    result = {}
    if at is not None:
        from tools.run_npe3_call_roundtrip import splice
        text = splice(text)
        result.update(check_roundtrip(text))
        replay = re.search(r'rom4_codec_replay: result=pass reference_events=(\d+)', text)
        result['replayed_dsp_writes'] = int(replay[1])
    check_inputs(text, events)
    if scenario.startswith('sms-'):
        from tools.run_6210_lab_sms import check as check_sms
        check_sms(text, scenario[4:])
        check_frames(run / 'snap', model.get(scenario.replace('-', '_') + '_frames', {}))
        return result
    if scenario.startswith('sip-'):
        from tools.run_rom4_lab_sip_call import check_sip_call
        outgoing = scenario == 'sip-outgoing'
        check_registration(text, REGISTRATION)
        result.update(check_host_lab(text, OUTGOING_PHASES, HANDSET_CLEARING, 'phase=connected') if outgoing
                      else check_host_lab(text))
        result.update(check_sip_call(run, outgoing, digits=DIALLED))
        return result
    check_registration(text)
    result.update(check_traffic_call(text))
    check_frames(run / 'snap', model['frames'])
    return result


def main():
    from tools.run_npe3_native import configure_cell_power
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('model', choices=MODELS)
    parser.add_argument('--scenario', choices=('lab', 'sip-incoming', 'sip-outgoing', 'sms-incoming', 'sms-outgoing'), default='lab')
    parser.add_argument('--roundtrip-at', choices=ROUNDTRIPS,
                        help='save, replay and reload the lab call in this phase')
    parser.add_argument('--mame', type=Path, default=ROOT / 'mame/mame')
    parser.add_argument('--acquired-records', action='store_true', help='negative control: keep the donor records')
    parser.add_argument('--http-port', type=int, default=18112)
    parser.add_argument('--sip-port', type=int, default=25190)
    parser.add_argument('--pjsua', default=os.environ.get('SIP_PJSUA_BIN', ''))
    parser.add_argument('--pjsua2', default=os.environ.get('SIP_PYTHON_PATH', ''))
    parser.add_argument('--keep', action='store_true')
    args = parser.parse_args()
    sip = args.scenario.startswith('sip-')
    sms = args.scenario.startswith('sms-')
    if sip and (args.roundtrip_at or not args.pjsua or not args.pjsua2):
        parser.error('SIP scenarios need --pjsua and --pjsua2, without --roundtrip-at')
    model = MODELS[args.model]
    args.mame = args.mame.resolve()  # MAME runs from inside the run directory
    (ROOT / 'tmp').mkdir(exist_ok=True)
    base = Path(tempfile.mkdtemp(prefix=f'{args.model}-call.', dir=ROOT / 'tmp'))
    try:
        machine = prepare(base, args.model, not args.acquired_records)
        run = base / 'run'
        run.mkdir()
        outgoing = args.scenario == 'sip-outgoing'
        if sms:
            configure_cell_power(run, machine, 'handset33', sacch=True, sms=True)
        else:
            configure_cell_power(run, machine, 'handset33', incoming_call=not outgoing, traffic_assignment=True,
                                 call_ack=not outgoing, clear_call=True, sacch=True, speech=True,
                                 accept_outgoing_service=outgoing, proceed_outgoing_call=outgoing)
        events, seconds = schedule(model, args.scenario)
        command = [str(args.mame), machine, '-rompath', f'{base / "roms"};{ROOT / "roms"}',
                   '-nvram_directory', str(base / 'nvram'), '-cfg_directory', 'cfg',
                   '-snapshot_directory', 'snap', '-noreadconfig', '-video', 'none', '-sound', 'none',
                   '-log', '-autoboot_delay', '0',
                   '-autoboot_script', str(ROOT / 'tools/rom6_lab_call.lua'),
                   '-seconds_to_run', str(seconds), '-nonvram_save']
        # SIP media runs in real time, so MAME is throttled for it.
        command += ['-http', '-http_port', str(args.http_port)] if sip else ['-nothrottle']
        environment = {k: v for k, v in os.environ.items() if not k.startswith(('NOKIA_DCT3_', 'ROM6_LAB_'))}
        environment['ROM6_LAB_EVENTS'] = ';'.join(f'{time}|{name}|{held}' for time, name, held in events)
        at = None
        if args.roundtrip_at:
            at = model['page'] + ROUNDTRIPS[args.roundtrip_at]
            environment.update(ROM6_LAB_ROUNDTRIP_AT=str(at), NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS='200')
        if sip:
            from tools.run_rom4_lab_sip_call import run_sip_call
            try:
                asyncio.run(run_sip_call(command, run, root=ROOT, pjsua=args.pjsua, pjsua2=args.pjsua2,
                                         http_port=args.http_port, sip_port=args.sip_port, outgoing=outgoing,
                                         talk=6, handset_env=environment, handset_timeout=300))
            except (RuntimeError, asyncio.TimeoutError) as error:
                raise ValueError(f'SIP call: {error!r}') from error
        else:
            with (run / 'console.log').open('w') as output:
                subprocess.run(command, cwd=run, env=environment, stdout=output, stderr=subprocess.STDOUT,
                               check=True, timeout=1800)
        text = (run / 'error.log').read_text(errors='replace')
        result = check(text, run, model, events, args.scenario, at)
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'Native {model["name"]} {args.scenario} call failed: {error}; evidence in {base}\n')
    print(' '.join(f'{key}={value}' for key, value in result.items()))
    print(f'Native {model["name"]} {args.scenario} call: PASS' +
          (f' with a state round trip while {args.roundtrip_at}' if at else ''))
    if args.keep:
        print(f'evidence retained: {base}')
    else:
        shutil.rmtree(base)


if __name__ == '__main__':
    main()

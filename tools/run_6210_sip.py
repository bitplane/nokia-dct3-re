#!/usr/bin/env python3
"""Run the native 6210 on the ROM6 lab cell as a live SIP phone.

This is the lean path for real calls, not a gate. MAME runs throttled, with
no research observers and no debugger, and its window shows the handset. A
SIP bridge (dct3_sip_bridge.py) joins MAME's call endpoint to SIP:

  incoming  a SIP party calls sip:dct3@HOST:BRIDGE_PORT; the 6210 rings and
            you answer with Send and hang up with End in the MAME window.
  outgoing  dial any number on the 6210 and press Send; the bridge calls
            --destination, and End hangs up.

The lab cell supports one call direction per session, each exactly as its
gate (verify-6210-lab-sip-call / -outgoing-call) configures it. The SIP party
plays through the handset's earpiece (COBBA EAR) to MAME's sound output, and
the host's default microphone feeds its MIC2 (verify-6210-lab-physical-duplex).
With --headless there is no sound, and the microphone carries the lab tone.

Needs the PJSUA2 binding (make sip-build; SIP_PYTHON_PATH). Check the host
can keep up first with `make bench-6210-call` (1.0x or more).
"""
import argparse
import asyncio
import os
from pathlib import Path
import signal
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.bench_6210_call import MODEL, prepare  # noqa: E402
from tools.run_npe3_native import configure_cell_power  # noqa: E402
from tools.run_rom4_lab_sip_call import wait_for_http  # noqa: E402


def lab_cell(directory, direction, host_microphone):
    """The lab BTS configuration each SIP gate verifies, plus the MIC2 source."""
    if direction == 'incoming':
        configure_cell_power(directory, MODEL, 'handset33', incoming_call=True, traffic_assignment=True,
                             call_ack=True, clear_call=True, sacch=True, speech=True,
                             host_microphone=host_microphone)
    else:
        configure_cell_power(directory, MODEL, 'handset33', traffic_assignment=True, clear_call=True,
                             sacch=True, accept_outgoing_service=True, proceed_outgoing_call=True,
                             speech=True, host_microphone=host_microphone)


def handset_command(mame, base, directory, http_port, headless, script=None):
    command = [str(mame), MODEL, '-rompath', f'{base / "roms"};{ROOT / "roms"}',
               '-nvram_directory', str(base / 'nvram'), '-cfg_directory', str(directory / 'cfg'),
               '-snapshot_directory', str(directory / 'snap'), '-noreadconfig', '-skip_gameinfo',
               '-http', '-http_port', str(http_port), '-nonvram_save']
    if headless:
        command += ['-video', 'none', '-sound', 'none']
    if script:
        command += ['-autoboot_delay', '0', '-autoboot_script', str(Path(script).resolve())]
    return command


async def run(args):
    (ROOT / 'tmp').mkdir(exist_ok=True)
    base = Path(tempfile.mkdtemp(prefix='6210-sip.', dir=ROOT / 'tmp'))
    prepare(base)
    directory = base / 'handset'
    directory.mkdir()
    lab_cell(directory, args.direction, host_microphone=not args.headless)
    environment = {k: v for k, v in os.environ.items() if not k.startswith('NOKIA_DCT3_')}
    bridge_environment = dict(os.environ, PYTHONPATH=os.pathsep.join(
        filter(None, (args.pjsua2, os.environ.get('PYTHONPATH')))))
    destination = args.destination or 'sip:unused@127.0.0.1'
    print(f'6210 SIP: run directory {base}', flush=True)
    processes = []
    try:
        with (base / 'console.log').open('wb') as console:
            handset = await asyncio.create_subprocess_exec(
                *handset_command(args.mame, base, directory, args.http_port, args.headless, args.script),
                cwd=directory, env=environment, stdout=console, stderr=asyncio.subprocess.STDOUT)
            processes.append(handset)
            await wait_for_http(args.http_port, handset)
            bridge = await asyncio.create_subprocess_exec(
                sys.executable, str(ROOT / 'tools/dct3_sip_bridge.py'),
                '--url', f'ws://127.0.0.1:{args.http_port}/nokia/dct3/calls',
                '--destination', destination, '--sip-port', str(args.sip_port),
                '--calls', str(args.calls), env=bridge_environment)
            processes.append(bridge)
            if args.direction == 'incoming':
                print(f'6210 SIP: once the 6210 shows DCT3 LAB, call sip:dct3@<this host>:{args.sip_port}',
                      flush=True)
            else:
                print(f'6210 SIP: once the 6210 shows DCT3 LAB, dial and press Send; the bridge calls '
                      f'{destination}', flush=True)
            done, _ = await asyncio.wait([asyncio.ensure_future(p.wait()) for p in processes],
                                         return_when=asyncio.FIRST_COMPLETED)
    finally:
        for process in processes:
            if process.returncode is None:
                process.send_signal(signal.SIGINT)
        for process in processes:
            try:
                await asyncio.wait_for(process.wait(), 10)
            except asyncio.TimeoutError:
                process.kill()
    print(f'6210 SIP: stopped; logs in {base}', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--direction', choices=('incoming', 'outgoing'), default='incoming')
    parser.add_argument('--destination', help='SIP URI the bridge calls for outgoing calls')
    parser.add_argument('--sip-port', type=int, default=25070, help="the bridge's SIP port")
    parser.add_argument('--http-port', type=int, default=18080, help="MAME's call endpoint port")
    parser.add_argument('--pjsua2', default=os.environ.get('SIP_PYTHON_PATH', ''),
                        help='PYTHONPATH entries for the pjsua2 binding (default: $SIP_PYTHON_PATH)')
    parser.add_argument('--mame', type=Path, default=ROOT / 'mame/mame')
    parser.add_argument('--headless', action='store_true', help='no MAME window or sound')
    parser.add_argument('--script', type=Path, help='a Lua autoboot script, e.g. key input for unattended tests')
    parser.add_argument('--calls', type=int, default=0, help='stop after this many calls; zero runs until interrupted')
    args = parser.parse_args()
    if args.direction == 'outgoing' and not args.destination:
        parser.error('outgoing calls need --destination')
    try:
        asyncio.run(run(args))
    except KeyboardInterrupt:
        pass


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Save, replay and reload the native 6210's lab call while ringing, connected
or clearing, then complete it.

The lab's early incoming call (page 18 s, Send 20 s, End 30 s, lab tones both
ways) runs without research observers. At the requested time the machine
saves, runs 200 ms recording every DSP I/O write, reloads, and must repeat
those writes exactly; then the restored branch finishes the call, which must
pass the same traffic-call, FACCH, SACCH and tone checks as an ordinary run.
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.bench_6210_call import MODEL, prepare  # noqa: E402
from tools.rom4_lab_registration_check import check_roundtrip, check_traffic_call  # noqa: E402
from tools.run_npe3_native import configure_cell_power  # noqa: E402


def splice(text):
    """One timeline: drop the reference branch between the save and the reload."""
    begin = text.find('state_replay: phase=reference event=begin')
    # The first line logged after the load, whichever notifier runs first.
    restored = min((at for at in (text.find('state_roundtrip:'), text.find('rom4_codec_state: phase=restored'))
                    if at >= 0), default=-1)
    if begin < 0 or restored < begin:
        raise ValueError('missing state round-trip markers')
    return text[:text.rfind('\n', 0, begin) + 1] + text[text.rfind('\n', 0, restored) + 1:]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('at', type=float, help='round-trip time: ringing 18.1-19.79, connected 20.15-29.79, '
                                               'clearing after 30.15')
    parser.add_argument('--mame', type=Path, default=ROOT / 'mame/mame')
    parser.add_argument('--keep', action='store_true')
    args = parser.parse_args()
    args.mame = args.mame.resolve()  # MAME runs from inside the run directory
    (ROOT / 'tmp').mkdir(exist_ok=True)
    base = Path(tempfile.mkdtemp(prefix='npe3-roundtrip.', dir=ROOT / 'tmp'))
    try:
        prepare(base)
        run = base / 'run'
        run.mkdir()
        configure_cell_power(run, MODEL, 'handset33', incoming_call=True, traffic_assignment=True,
                             call_ack=True, clear_call=True, sacch=True, speech=True)
        command = [str(args.mame), MODEL, '-rompath', f'{base / "roms"};{ROOT / "roms"}',
                   '-nvram_directory', str(base / 'nvram'), '-cfg_directory', 'cfg',
                   '-snapshot_directory', 'snap', '-noreadconfig', '-video', 'none', '-sound', 'none',
                   '-nothrottle', '-log', '-autoboot_delay', '0',
                   '-autoboot_script', str(ROOT / 'tools/npe3_call_roundtrip.lua'),
                   '-seconds_to_run', '36', '-nonvram_save']
        environment = {k: v for k, v in os.environ.items() if not k.startswith('NOKIA_DCT3_')}
        environment.update(NOKIA_DCT3_STATE_ROUNDTRIP_AT=str(args.at), NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS='200')
        with (run / 'console.log').open('w') as output:
            subprocess.run(command, cwd=run, env=environment, stdout=output, stderr=subprocess.STDOUT,
                           check=True, timeout=1800)
        text = splice((run / 'error.log').read_text(errors='replace'))
        if '[LUA ERROR]' in text or 'npe3_call_input: key=End' not in text:
            raise ValueError('call inputs did not complete')
        result = check_roundtrip(text)
        replay = re.search(r'rom4_codec_replay: result=pass reference_events=(\d+)', text)
        result['replayed_dsp_writes'] = int(replay[1])
        result.update(check_traffic_call(text))
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'6210 call round trip at {args.at} s failed: {error}; evidence in {base}\n')
    print(' '.join(f'{key}={value}' for key, value in result.items()))
    print(f'6210 native lab call with state round trip at {args.at} s: PASS')
    if args.keep:
        print(f'evidence retained: {base}')
    else:
        shutil.rmtree(base)


if __name__ == '__main__':
    main()

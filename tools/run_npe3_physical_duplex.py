#!/usr/bin/env python3
"""Native 6210 physical duplex: host audio into MIC2 and out of EAR on a lab call.

The ROM6 lab's early incoming call (page 18 s, Send 20 s, End 30 s) runs with
the lab BTS sending its 1 kHz downlink tone, but the handset's microphone takes
host input through COBBA MIC2 instead of the lab's 500 Hz table tone. A host
500 Hz sine plays into a private PulseAudio null sink that MAME records from;
MAME's speaker output goes to a second null sink that is recorded to a WAV.

Passing needs both directions through the native DSP and GSM-FR codecs:
- uplink: the lab BTS's GSM-FR decoder hears the host's 500 Hz in the
  uplink (run_npe3_native's traffic-audio check);
- downlink: MAME's speaker mix, recorded in emulated time (-wavwrite), carries
  a sustained 1 kHz run (radio_physical_downlink_check.py), and the host's
  PulseAudio sink received non-silent playback. The live capture's continuity
  depends on the host keeping real time (make bench-6210-call), so it is
  required to be non-silent, not unbroken.

NPE-3 System Module table 8 evidences the MIC2/EAR board routes. Routes are
neutral (connectivity only); the analogue gains are not recovered. PulseAudio's
default sink and source are switched to the private endpoints for the run, as
the other physical-duplex gates do, and restored afterwards.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ['--reference-clock', '--seconds', '36', '--adc-source', 'fcch-sch-broadcast',
           '--capture-start', 'p09', '--tx-release', 'p05', '--raw-agch', '--provision-records',
           '--page-at', '18', '--cell-power-policy', 'handset33', '--incoming-call',
           '--ack-call-control', '--sacch', '--answer-call', '--traffic-assignment', '--speech',
           '--end-call-at', '30', '--host-audio']


def live_playback(path):
    """The host sink's capture: non-silent output reached it."""
    import array
    import wave
    with wave.open(str(path), 'rb') as capture:
        samples = array.array('h', capture.readframes(capture.getnframes()))
    if sys.byteorder != 'little':
        samples.byteswap()
    loud = sum(abs(sample) > 64 for sample in samples)
    if len(samples) < 8000 or loud < 4000:
        raise ValueError('the host PulseAudio sink received no handset playback')
    return f'{loud / 8000:.1f} s above 64 of {len(samples) / 8000:.1f} s captured'


def pactl(*args):
    return subprocess.run(['pactl', *args], check=True, capture_output=True, text=True).stdout.strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--volume', type=float, default=0.25,
                        help='host sine amplitude (0.25 is the lab tone level at neutral gain)')
    args = parser.parse_args()
    for command in ('pactl', 'ffmpeg'):
        if shutil.which(command) is None:
            parser.exit(1, f'6210 physical duplex: missing host-audio command {command}\n')
    run = args.run_directory.resolve()
    run.mkdir(parents=True, exist_ok=False)
    tag = os.getpid()
    uplink, downlink, capture = (f'nokia_npe3_uplink_{tag}', f'nokia_npe3_downlink_{tag}',
                                 f'nokia_npe3_capture_{tag}')
    modules, processes = [], []
    default_sink = default_source = None
    try:
        modules.append(pactl('load-module', 'module-null-sink', f'sink_name={uplink}',
                             'sink_properties=device.description=NokiaNPE3Uplink'))
        modules.append(pactl('load-module', 'module-null-sink', f'sink_name={downlink}',
                             'sink_properties=device.description=NokiaNPE3Downlink'))
        modules.append(pactl('load-module', 'module-remap-source', f'master={uplink}.monitor',
                             f'source_name={capture}',
                             'source_properties=device.description=NokiaNPE3Capture'))
        default_sink, default_source = pactl('get-default-sink'), pactl('get-default-source')
        pactl('set-default-sink', downlink)
        pactl('set-default-source', capture)
        processes.append(subprocess.Popen(
            ['ffmpeg', '-hide_banner', '-loglevel', 'error', '-re', '-f', 'lavfi',
             '-i', 'sine=frequency=500:sample_rate=48000', '-filter:a', f'volume={args.volume}',
             '-device', uplink, '-f', 'pulse', '-'], stdin=subprocess.DEVNULL))
        recorder = subprocess.Popen(
            ['ffmpeg', '-y', '-hide_banner', '-loglevel', 'error', '-f', 'pulse',
             '-i', f'{downlink}.monitor', '-ac', '1', '-ar', '8000', '-c:a', 'pcm_s16le',
             str(run / 'downlink.wav')], stdin=subprocess.PIPE)
        processes.append(recorder)
        with (run / 'pulse_routes.log').open('w') as routes:
            processes.append(subprocess.Popen(
                [sys.executable, str(ROOT / 'tools/pulse_route_mame.py'), '--source', capture,
                 '--sink', downlink], stdout=routes))
        with (run / 'handset.log').open('w') as output:
            handset = subprocess.run([sys.executable, str(ROOT / 'tools/run_npe3_native.py'), *FIXTURE,
                                      '--wavwrite', str(run / 'speaker.wav')],
                                     cwd=ROOT, env=dict(os.environ, PULSE_SOURCE=capture),
                                     stdout=output, stderr=subprocess.STDOUT, timeout=3600)
        routes = (run / 'pulse_routes.log').read_text()
        recorder.communicate(b'q', timeout=30)
        handset_log = (run / 'handset.log').read_text(errors='replace')
        if handset.returncode:
            tail = handset_log.strip().splitlines()[-1:] or ['no output']
            raise ValueError(f'handset run failed: {tail[0]}')
        for kind in ('source-output', 'sink-input'):
            if f'pulse_route: {kind} ' not in routes:
                raise ValueError(f'no MAME {kind} stream on the private endpoints')
        subprocess.run(['ffmpeg', '-y', '-hide_banner', '-loglevel', 'error', '-i', str(run / 'speaker.wav'),
                        '-ac', '1', '-ar', '8000', '-c:a', 'pcm_s16le', str(run / 'speaker-8k.wav')], check=True)
        subprocess.run([sys.executable, str(ROOT / 'tools/radio_physical_downlink_check.py'),
                        str(run / 'speaker-8k.wav'), '--allow-transient-clipping'], check=True)
        live = live_playback(run / 'downlink.wav')
        print(f'6210 physical duplex: live host playback {live}')
        print('6210 physical duplex: PASS host 500 Hz through MIC2 and the native uplink codec; '
              'lab 1 kHz through the native downlink codec to EAR; neutral gain, not analogue fidelity')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'6210 physical duplex failed: {error}\n')
    finally:
        for process in processes:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(10)
                except subprocess.TimeoutExpired:
                    process.kill()
        if default_sink:
            subprocess.run(['pactl', 'set-default-sink', default_sink], check=False)
        if default_source:
            subprocess.run(['pactl', 'set-default-source', default_source], check=False)
        for module in reversed(modules):
            subprocess.run(['pactl', 'unload-module', module], check=False)


if __name__ == '__main__':
    main()

"""Generate a channel-input-only EFR lost-SID fixture with an external reference."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.verify_etsi_efr_reference import CFLAGS, EXPECTED, lost_sid_input


def check_flags(text, decoder):
    import struct
    frames = list(struct.iter_unpack('<247H', decoder))
    rows = re.findall(r'^EFRDTX frame=(\d+) taf=(\d+) bfi=(\d+) sid=(\d+) ctrl=([0-9a-f]{4})$', text, re.M)
    if len(frames) != 518 or len(rows) != 518:
        raise ValueError('expected 518 complete DTX observations')
    controls = []
    for index, (row, frame) in enumerate(zip(rows, frames)):
        number, taf, bfi, sid = map(int, row[:4])
        if (number, taf, bfi, sid) != (index, frame[246], frame[0], frame[245]):
            raise ValueError('DTX observation disagrees with decoder input')
        controls.append(int(row[4], 16))
    if [i for i, value in enumerate(controls) if value & 0x80] != [95, 96]:
        raise ValueError('lost-SID muting interval differs')
    if [(i, value) for i, value in enumerate(controls) if value & 0x400] != [(97, 0x501), (356, 0x401)]:
        raise ValueError('first-speech/previous-muting transition differs')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True, type=Path, help='separately acquired and built ETSI EFR source directory')
    parser.add_argument('--dtx-input', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    source, output = args.source.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    decoder = lost_sid_input(args.dtx_input.read_bytes())
    tools = Path(__file__).resolve().parent
    commands = []
    with (output / 'build.log').open('w') as log:
        command = ['make', '-f', 'makefile.gcc', '-f', str(tools / 'etsi_efr_state_observer.mk'),
                   f'CFLAGS={CFLAGS}', f'DTX_OBSERVER_SOURCE={tools / "etsi_efr_dtx_observer.c"}',
                   f'OBSERVER_SOURCE={tools / "etsi_efr_state_observer.c"}', 'decoder_dtx_observed', 'decoder_observed']
        commands.append(command)
        subprocess.run(command, cwd=source, stdout=log, stderr=subprocess.STDOUT, check=True)
    installation = source / 'spch_dos.dec'
    if hashlib.sha256(installation.read_bytes()).hexdigest() != EXPECTED['dec'][1]:
        raise ValueError('reference installation input differs')
    (output / 'lost_sid.dec').write_bytes(decoder)
    runs = [('installation', 'decoder_dtx_observed', installation),
            ('lost_sid', 'decoder_dtx_observed', output / 'lost_sid.dec'),
            ('replay', 'decoder_dtx_observed', output / 'lost_sid.dec'),
            ('state', 'decoder_observed', output / 'lost_sid.dec'),
            ('state_replay', 'decoder_observed', output / 'lost_sid.dec'),
            ('unobserved', 'decoder', output / 'lost_sid.dec')]
    for name, executable, input_path in runs:
        command = [str(source / executable), str(input_path), str(output / f'{name}.out')]
        commands.append(command)
        with (output / f'{name}.log').open('w') as log:
            subprocess.run(command, cwd=source, stdout=log, stderr=subprocess.STDOUT, check=True)
    if hashlib.sha256((output / 'installation.out').read_bytes()).hexdigest() != EXPECTED['out'][1]:
        raise ValueError('observed reference fails installation PCM')
    pcm = (output / 'lost_sid.out').read_bytes()
    if len(pcm) != 518 * 320 or any((output / f'{name}.out').read_bytes() != pcm for name in ('replay', 'state', 'state_replay', 'unobserved')):
        raise ValueError('reference observer or fresh process changes PCM')
    for name in ('lost_sid', 'replay'):
        check_flags((output / f'{name}.log').read_text(), decoder)
    from tools.c54x_rom4_efr_state_check import parse
    from tools.c54x_rom4_efr_filter_check import reference_records
    from tools.c54x_rom4_efr_excitation_check import reference_records as excitation_records
    from tools.c54x_rom4_efr_predictor_check import reference_records as predictor_records
    states = []
    for name in ('state', 'state_replay'):
        text = (output / f'{name}.log').read_text()
        parse(text, frames=518)
        reference_records(text)
        excitation_controls, excitation_vectors = excitation_records(text)
        predictor_inputs = predictor_records(text)
        states.append([line for line in text.splitlines() if line.startswith(
            ('EFRSTATE ', 'EFRGAIN ', 'EFRPOLICY ', 'EFRFILTER ', 'EFRFILTERCALL ',
             'EFREXCCALL ', 'EFREXCPRED ', 'EFREXC ', 'EFRPREDINPUT ', 'EFRPREDHISTORY '))])
    if states[0] != states[1]:
        raise ValueError('reference semantic observations differ across fresh processes')
    evidence = dict(commands=commands, frames=518, samples=82880,
                    input_sha256=hashlib.sha256(decoder).hexdigest(), pcm_sha256=hashlib.sha256(pcm).hexdigest(),
                    observer_sha256=hashlib.sha256((tools / 'etsi_efr_dtx_observer.c').read_bytes()).hexdigest(),
                    state_observer_sha256=hashlib.sha256((tools / 'etsi_efr_state_observer.c').read_bytes()).hexdigest(),
                    installation_verified=True, fresh_process_replay=True, observer_pcm_unchanged=True,
                    synthesis_filter_calls=2072, synthesis_filter_vectors=6216,
                    excitation_calls=len(excitation_controls), excitation_vectors=len(excitation_vectors),
                    predictor_calls=len(predictor_inputs), predictor_history_words=len(predictor_inputs) * 154,
                    native_dsp_claim=False, handset_audio_claim=False)
    (output / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print('PASS: reference-only lost-SID muting and first-speech fixture; native DSP not tested')


if __name__ == '__main__':
    main()

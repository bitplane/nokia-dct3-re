"""Build original argument-only reference routines and retain reproducible evidence."""
import hashlib
from pathlib import Path
import shlex
import struct
import subprocess

from tools.verify_etsi_efr_reference import CFLAGS, EXPECTED


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def calculate(source, output, vectors, *, mode='filter'):
    if mode not in ('filter', 'predictor', 'mix', 'agc'):
        raise ValueError('unknown reference argument mode')
    tools = Path(__file__).resolve().parent
    output.mkdir(parents=True, exist_ok=False)
    (output / 'arguments.bin').write_bytes(vectors)
    originals = [source / name for name in ('decoder', 'syn_filt.c', 'pred_lt6.c',
        'basicop2.c', 'basic_op.h', 'typedef.h', 'count.c', 'count.h', 'agc.c',
        'inv_sqrt.c', 'inv_sqrt.tab', 'sig_proc.h', 'cnst.h')]
    originals += [tools / 'etsi_efr_argument_replay.c']
    identities = {str(path): digest(path) for path in originals}
    program = output / 'reference-calculator'
    commands = [
        ['gcc', *shlex.split(CFLAGS), *(str(source / name) for name in
          ('syn_filt.c', 'pred_lt6.c', 'basicop2.c', 'count.c', 'agc.c', 'inv_sqrt.c')),
         str(tools / 'etsi_efr_argument_replay.c'), '-o', str(program)],
        [str(source / 'decoder'), str(source / 'spch_dos.dec'), str(output / 'installation.out')]]
    if digest(source / 'spch_dos.dec') != EXPECTED['dec'][1]:
        raise ValueError('reference installation input differs')
    for name in ('first', 'replay'):
        commands.append([str(program), *(['--' + mode] if mode != 'filter' else []),
                         str(output / 'arguments.bin'), str(output / f'{name}.bin')])
    with (output / 'commands.log').open('w') as log:
        for command in commands:
            subprocess.run(command, cwd=source, stdout=log, stderr=subprocess.STDOUT, check=True)
    if digest(output / 'installation.out') != EXPECTED['out'][1]:
        raise ValueError('reference fails installation PCM')
    result = (output / 'first.bin').read_bytes()
    if result != (output / 'replay.bin').read_bytes():
        raise ValueError('argument calculator differs across fresh processes')
    if any(digest(path) != identities[str(path)] for path in originals):
        raise ValueError('reference decoder or sources changed')
    return result, dict(commands=commands, installation_verified=True, fresh_process_replay=True,
                        source_sha256=identities, arguments_sha256=digest(output / 'arguments.bin'),
                        output_sha256=digest(output / 'first.bin'), calculator_sha256=digest(program),
                        harness_sha256=digest(tools / 'etsi_efr_argument_replay.c'))


def compare_vectors(output, expected, *, scope):
    if not expected or any(len(values) != 40 for _, values in expected):
        raise ValueError('expected complete native output vectors')
    if len(output) != len(expected) * 80:
        raise ValueError('incomplete reference output')
    differences = []
    for result, (key, actual) in zip(struct.iter_unpack('<40H', output), expected):
        count = sum(a != b for a, b in zip(result, actual))
        if count:
            differences.append(dict(frame=key[0], subframe=key[1], samples=count))
    return dict(scope=scope, calls=len(expected), samples=len(expected) * 40,
                differences=differences, differing_samples=sum(row['samples'] for row in differences),
                reference_equal=not differences, full_decoder_conformance=False,
                handset_audio_claim=False)

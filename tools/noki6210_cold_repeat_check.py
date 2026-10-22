"""Compare two independently accepted NPE-3 accessory cold boots."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import re
import uuid

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from PIL import Image
from tools.run_noki6210_acceptance import events


def fingerprint(run):
    acceptance = json.loads((run / 'acceptance.json').read_text())
    if (acceptance.get('machine'), acceptance.get('scenario'), acceptance.get('passed')) != (
            'npe3hle', 'accessory', True):
        raise ValueError('requires an accepted fresh npe3hle accessory scenario')
    launch = acceptance.get('launch')
    if not isinstance(launch, dict) or launch.get('run_directory') != str(run.resolve()):
        raise ValueError('missing or copied cold-launch provenance')
    invocation = launch.get('invocation_id')
    if not isinstance(invocation, str) or uuid.UUID(invocation).version != 4:
        raise ValueError('missing unique cold-launch invocation')
    inputs = launch.get('input_sha256')
    if not isinstance(inputs, dict) or set(inputs) != {'mame', 'mcu', 'pmm', 'script'} or \
            any(not isinstance(value, str) or not re.fullmatch('[0-9a-f]{64}', value)
                for value in inputs.values()) or launch.get('initial_storage') != {}:
        raise ValueError('cold launch requires identified inputs and empty initial storage')
    trace = events(run / 'error.log')
    if not trace:
        raise ValueError('empty protocol trace')
    result = {'invocation_id': invocation,
              'launch_inputs': hashlib.sha256(json.dumps(inputs, sort_keys=True).encode()).hexdigest(),
              'protocol': hashlib.sha256(trace.encode()).hexdigest()}
    for name in ('ccont', 'flash', 'sim_card'):
        data = (run / 'nvram/npe3hle' / name).read_bytes()
        if not data:
            raise ValueError('empty persisted device: ' + name)
        result[name] = hashlib.sha256(data).hexdigest()
    for name in ('6210_before_menu.png', '6210_after_menu.png'):
        with Image.open(run / 'snap' / name) as frame:
            if frame.size != (96, 60):
                raise ValueError('unexpected frame geometry: ' + name)
            result[name] = hashlib.sha256(frame.convert('L').tobytes()).hexdigest()
    return result


def verify(first, second):
    if first.resolve() == second.resolve():
        raise ValueError('cold-boot directories must be distinct')
    left, right = fingerprint(first), fingerprint(second)
    if left.pop('invocation_id') == right.pop('invocation_id'):
        raise ValueError('cold boots require independent launch invocations')
    different = [name for name in left if left[name] != right[name]]
    if different:
        raise ValueError('cold-boot artifacts differ: ' + ', '.join(different))
    return left


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('first', type=Path)
    parser.add_argument('second', type=Path)
    args = parser.parse_args()
    try:
        result = verify(args.first, args.second)
    except (OSError, ValueError) as error:
        parser.exit(1, f'6210 cold-repeat FAIL: {error}\n')
    print('6210 cold-repeat PASS: ' + json.dumps(result, sort_keys=True))


if __name__ == '__main__':
    main()

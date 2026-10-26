"""Freeze conformance selection/results before structural harness changes.

This inventories declarations and acceptance text, not executed coverage or
silicon correctness. Native gates remain mandatory for behavior changes.
"""
import argparse
import ast
from collections import Counter
import json
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.c54x_profiles import declarations as bios_declarations, harness_sources

ROOT = Path(__file__).resolve().parents[1]
BASELINE = ROOT / 'fixtures/c54x_conformance_baseline.json'
STRING = r'"(?:\\.|[^"\\])*"'


def fields(marker):
    return dict(re.findall(r'(\w+)=([^\s]+)', marker))


def cpp_inventory(sources):
    declarations, formats = {}, Counter()
    assertions = 0
    for source in sources:
        for machine, bios in bios_declarations(source).items():
            if machine in declarations:
                raise ValueError('duplicate machine declaration: ' + machine)
            declarations[machine] = bios
        for match in re.finditer(r'osd_printf_info\s*\(\s*((?:' + STRING + r'\s*)+)', source):
            value = ''.join(ast.literal_eval(word) for word in re.findall(STRING, match[1]))
            if ': PASS' in value:
                formats[value] += 1
        assertions += len(re.findall(r'\bexpect(?:_opcode)?\s*\(', source))
    return dict(machines=declarations, pass_formats=dict(sorted(formats.items())),
                assertion_sites=assertions)


def inventory(root=ROOT):
    from tools import run_c54x_core as runner
    sources = [path.read_text() for path in harness_sources(root) if path.suffix == '.cpp']
    cpp = cpp_inventory(sources)
    machines = {}
    for machine, profiles in cpp['machines'].items():
        for profile in profiles:
            machines.setdefault(profile['name'], []).append(machine)
    profiles = {}
    for name, (markers, completion) in sorted(runner.EXTRA_PROFILES.items()):
        if len(machines.get(name, [])) != 1:
            raise ValueError('isolated profile has no unique BIOS owner: ' + name)
        markers = [markers] if isinstance(markers, str) else list(markers)
        profiles[name] = dict(machine=machines[name][0], required=markers,
                              reported_fields=[fields(marker) for marker in markers],
                              completion=completion)
    scopes = {}
    for name in ('BUS_REGISTER', 'BUS_IO', 'ATOMIC_IO', 'COMPOSE', 'BUS_STM',
                 'READ_A_MAPPED', 'DP_ABSOLUTE'):
        marker = getattr(runner, name + '_REQUIRED')
        markers = [marker] if isinstance(marker, str) else list(marker)
        scopes[name] = dict(required=markers, completion=getattr(runner, name + '_COMPLETION'))
    return dict(schema=1, cpp=cpp, isolated=profiles, scopes=scopes,
                core=dict(required=list(runner.REQUIRED), completion=runner.COMPLETION))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write', action='store_true', help='explicitly replace the reviewed baseline')
    parser.add_argument('--baseline', type=Path, default=BASELINE)
    args = parser.parse_args()
    try:
        actual = inventory()
        if args.write:
            args.baseline.write_text(json.dumps(actual, indent=2, sort_keys=True) + '\n')
        elif actual != json.loads(args.baseline.read_text()):
            raise ValueError('harness inventory changed; review the contract rather than rebanking automatically')
        print(f'C54x harness inventory: {len(actual["cpp"]["machines"])} machines, '
              f'{len(actual["isolated"])} isolated profiles, '
              f'{len(actual["core"]["required"])} core requirements, '
              f'{actual["cpp"]["assertion_sites"]} lexical assertion-pattern sites')
    except (OSError, ValueError) as error:
        parser.exit(1, f'C54x harness inventory: {error}\n')


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Collect the exact C54x opcodes executed by passing section cases.

Each section case loads its own program and asserts the complete final state,
so an opcode executed during a passing section is executed under assertion.
This replaces the retired default fixture's explicit [opassert] markers with
a weaker but reproducible claim: it does not show that each instruction's own
effect is distinguishable from the rest of its case program.
"""
import argparse
import json
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.c54x_opcode_coverage import summarize
from tools.run_c54x_section import load_sections

ROOT = Path(__file__).resolve().parents[1]


def section_opcodes(directory, name, cases):
    console = (directory / 'console.log').read_text(errors='replace')
    if f'C54x section: PASS cases={cases} restores={cases}' not in console.splitlines():
        raise ValueError(f'section {name} did not pass with {cases} cases')
    log = directory / 'error.log'
    if not log.is_file():
        raise ValueError(f'section {name} has no coverage log; rerun with --opcode-coverage')
    text = log.read_text(errors='replace')
    # Rejection, bus and clock sections can pass without admitting any opcode.
    if '[opcov]' not in text:
        return {}
    return summarize(text)['counts']


def inventory(scope='cpu', root=ROOT):
    """Return {opcode: {section: executions}} from a complete suite run."""
    trial = root / 'tmp/c54x-section-trial'
    report = json.loads((trial / 'all-summary.json').read_text())
    if not report.get('complete'):
        raise ValueError('section suite did not complete')
    sections = load_sections(root)
    completed = {item['section'] for item in report['completed']}
    wanted = {name for name, item in sections.items() if scope == 'all' or item['scope'] == scope}
    if wanted - completed:
        raise ValueError('suite run lacks sections: ' + ', '.join(sorted(wanted - completed)))
    result = {}
    for name in sorted(wanted):
        for opcode, count in section_opcodes(trial / name, name, sections[name]['cases']).items():
            result.setdefault(opcode, {})[name] = count
    return result


def fixture_log(opcodes):
    """Render an inventory in the [opcov]/[opassert] form c54x_opcode_coverage reads."""
    lines = []
    for opcode, owners in sorted(opcodes.items()):
        lines.append(f'[opcov] op={opcode:04x} first_pc=000000 count={sum(owners.values())}')
        lines.append(f'[opassert] op={opcode:04x}')
    return '\n'.join(lines) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path, help='fixture log to write')
    parser.add_argument('--scope', choices=('cpu', 'firmware', 'all'), default='cpu')
    parser.add_argument('--owners', type=Path, help='also write {opcode: [sections]} JSON')
    args = parser.parse_args()
    try:
        opcodes = inventory(args.scope)
    except (OSError, ValueError) as error:
        raise SystemExit(f'C54x section inventory rejected: {error}')
    args.output.write_text(fixture_log(opcodes))
    if args.owners:
        args.owners.write_text(json.dumps({f'{op:04x}': sorted(owners) for op, owners in sorted(opcodes.items())},
                                          indent=1) + '\n')
    print(f'C54x section inventory: scope={args.scope} opcodes={len(opcodes)}')


if __name__ == '__main__':
    main()

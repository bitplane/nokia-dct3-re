"""Attribute native RF reads; do not infer a physical sample clock or I/Q map."""
import argparse
import json
from pathlib import Path
import re
import sys

try:
    from tools.c54x_rom4_port_census import census
except ModuleNotFoundError:
    from c54x_rom4_port_census import census


FAMILIES = {
    'initial_filter': (0x3249, 0x324f, 0x3255, 0x325b),
    'unfiltered_history': (0x32cd, 0x32d0, 0x32d3, 0x32d6),
    'scan_filter': (0x330a, 0x3310, 0x3316, 0x331c),
    'capture_filter': (0x33c9, 0x33cf, 0x33d5, 0x33db),
}
EXPECTED_TOTALS = dict(initial_filter=372160, unfiltered_history=242144,
                       scan_filter=32768, capture_filter=960)


def check_program(image):
    sites = sorted(address for addresses in FAMILIES.values() for address in addresses)
    if census(image).get(('R', 0x27), []) != sites:
        raise ValueError('resident RF reader census changed')
    for family, addresses in FAMILIES.items():
        opcode = 0x74d4 if family == 'unfiltered_history' else 0x74d6
        expected = opcode.to_bytes(2, 'big') + b'\x00\x27'
        for address in addresses:
            if image[address * 2:address * 2 + 4] != expected:
                raise ValueError('resident RF reader operand changed')
    return sites


def check(text):
    pattern = re.compile(r'rom4_rf_consumer: xpc=([0-9a-f]{2}) pc=([0-9a-f]{4}) mode=([0-9a-f]{4}) '
                         r'control=([0-9a-f]{4}) count=(\d+) first=([0-9.]+) last=([0-9.]+)')
    rows = pattern.findall(text)
    if text.count('rom4_rf_consumer:') != len(rows):
        raise ValueError('malformed RF consumer row')
    summaries = re.findall(r'rom4_rf_consumer_summary: reads=(\d+) buckets=(\d+) physical_rate_claim=0 t=([0-9.]+)', text)
    if len(summaries) != 1 or float(summaries[0][2]) < 29.9:
        raise ValueError('missing complete RF consumer summary')
    total, buckets = map(int, summaries[0][:2])
    interface = re.findall(r'rom4_interface_summary: .*?\brf_reads=(\d+)\b', text)
    if interface != [str(total)] or len(rows) != buckets or buckets != 64:
        raise ValueError('RF consumer accounting differs')
    by_pc, seen = {}, set()
    clear_reads = 0
    for xpc, pc, mode, control, count, first, last in rows:
        if xpc != '00':
            raise ValueError('unclassified RF program page')
        key = (pc, mode, control)
        if key in seen or int(count) <= 0 or not (0 <= float(first) <= float(last) <= float(summaries[0][2])):
            raise ValueError('invalid RF consumer bucket')
        seen.add(key)
        by_pc.setdefault(int(pc, 16), {})[(mode, control)] = int(count)
        if not int(control, 16) & 1:
            clear_reads += int(count)
    expected_pcs = {address + 2 for addresses in FAMILIES.values() for address in addresses}
    if set(by_pc) != expected_pcs:
        raise ValueError('unclassified or missing RF consumer')
    families = {}
    for name, addresses in FAMILIES.items():
        groups = [by_pc[address + 2] for address in addresses]
        if any(group != groups[0] for group in groups[1:]):
            raise ValueError('RF consumer quartet is incomplete')
        count = sum(groups[0].values()) * 4
        if count != EXPECTED_TOTALS[name]:
            raise ValueError('RF consumer family snapshot changed')
        families[name] = dict(reads=count, control_clear_reads=4 * sum(
            value for (_, control), value in groups[0].items() if not int(control, 16) & 1),
            mode_controls=[dict(mode=mode, control=control, reads=value * 4)
                           for (mode, control), value in sorted(groups[0].items())])
    if sum(row['reads'] for row in families.values()) != total or clear_reads != 114144:
        raise ValueError('RF control-clear accounting changed')
    return dict(schema=1, reads=total, buckets=buckets, families=families,
                control_clear_reads=clear_reads, resident_read_sites=16,
                physical_rate_claim=False, iq_lane_claim=False, acquisition_claim=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--program', type=Path, required=True)
    parser.add_argument('--log', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    check_program(args.program.read_bytes())
    report = check(args.log.read_text())
    if args.output:
        args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(f"Native RF consumers: PASS sites=16 reads={report['reads']} control_clear={report['control_clear_reads']} physical_rate_claim=0")
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as error:
        print(f'Native RF consumers: FAIL: {error}', file=sys.stderr)
        raise SystemExit(1)

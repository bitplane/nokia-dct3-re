"""Own NSB-3 bounded native RF consumption; no physical sample-rate claim."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.c54x_rom4_rf_consumers_check import FAMILIES
from tools.nsb3_fdn_edit_check import check_card
from tools.nsb3_pin_check import check_trace as check_pin
from tools.nsb3_nominal_input import IMAGE_SHA256
from tools.nsb3_puk_check import check_link

TOTALS = dict(initial_filter=644704, unfiltered_history=382944,
              scan_filter=64512, capture_filter=128)
ROW = re.compile(r'rom4_rf_consumer: xpc=([0-9a-f]{2}) pc=([0-9a-f]{4}) mode=([0-9a-f]{4}) '
                 r'control=([0-9a-f]{4}) holdoff=([01]) count=(\d+) first=([0-9.]+) last=([0-9.]+)')


def check_consumers(text):
    rows = ROW.findall(text)
    summaries = re.findall(r'rom4_rf_consumer_summary: reads=(\d+) buckets=(\d+) physical_rate_claim=0 t=([0-9.]+)', text)
    if text.count('rom4_rf_consumer:') != len(rows) or len(summaries) != 1:
        raise ValueError('missing or malformed consumer accounting')
    total, buckets, end = summaries[0]
    if int(total) != sum(TOTALS.values()) or int(buckets) != 52 or len(rows) != 52 or float(end) < 60:
        raise ValueError('bounded NSB-3 consumer extent differs')
    if re.findall(r'rom4_interface_summary: .*?\brf_reads=(\d+)\b', text) != [total]:
        raise ValueError('consumer and device totals disagree')
    by_pc, seen = {}, set()
    for page, pc, mode, control, holdoff, count, first, last in rows:
        key = (page, pc, mode, control, holdoff)
        if key in seen or page != '00' or int(count) <= 0 or not 0 <= float(first) <= float(last) <= float(end):
            raise ValueError('invalid or duplicated RF bucket')
        seen.add(key)
        by_pc.setdefault(int(pc, 16), {})[(mode, control, holdoff)] = int(count)
    expected = {pc + 2 for sites in FAMILIES.values() for pc in sites}
    if set(by_pc) != expected:
        raise ValueError('unclassified or missing resident reader')
    for family, sites in FAMILIES.items():
        groups = [by_pc[pc + 2] for pc in sites]
        if any(group != groups[0] for group in groups[1:]) or sum(groups[0].values()) * 4 != TOTALS[family]:
            raise ValueError('RF quartet or family totals differ')
    scan = [row for row in rows if int(row[1], 16) == 0x330c]
    if len(scan) != 1 or scan[0][2:5] != ('0001', '0105', '0') or float(scan[0][7]) < 59.9:
        raise ValueError('late scan-consumption control missing')
    if 'nsb3_board_reset_request:' in text:
        raise ValueError('reset interrupts bounded observation')
    return dict(reads=int(total), buckets=int(buckets), families=TOTALS.copy(),
                physical_rate_claim=False, acquisition_claim=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('run', type=Path)
    args = parser.parse_args()
    try:
        check_link(args.source, args.run)
        storage = args.run / 'nvram/nsb3r4t'
        if hashlib.sha256((storage / 'flash').read_bytes()).hexdigest() != IMAGE_SHA256:
            raise ValueError('own executable changed')
        check_card((storage / 'sim_card').read_bytes())
        text = (args.run / 'error.log').read_text()
        check_pin(text, True)
        report = check_consumers(text)
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f'NSB-3 RF consumption: FAIL: {exc}\n')
    print(f"PASS: {report['reads']} RF reads at 16 resident sites; physical format/acquisition unproved")


if __name__ == '__main__':
    main()

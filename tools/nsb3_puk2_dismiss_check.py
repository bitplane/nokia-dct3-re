"""Check physical Back/End/Menu recovery after permanent NSB-3 PIN2 refusal."""
import argparse
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_puk2_exhaust import check_permanent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('run', type=Path)
    args = parser.parse_args()
    try:
        check_permanent(args.source, args.run, dismiss=True)
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f'NSB-3 PIN2 dismissal: FAIL: {exc}\n')
    print('PASS: physical dismissal restores clean menus; permanent PIN2 loss unchanged')


if __name__ == '__main__':
    main()

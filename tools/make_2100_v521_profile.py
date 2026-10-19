#!/usr/bin/env python3
"""Create a checksum-only own-image NAM-2 v5.21 research fixture, not a donor mix."""

import argparse
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.make_2100_pmm_profile import make_own_v521_profile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    try:
        if args.image.resolve() == args.output.resolve():
            raise ValueError('the original acquired image must not be overwritten')
        data = make_own_v521_profile(args.image.read_bytes())
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(data)
    except (OSError, ValueError) as error:
        parser.exit(1, f'NAM-2 v5.21 fixture: {error}\n')
    print(f'wrote {args.output}; only offsets 0x1fc144/0x1fc145 changed')


if __name__ == '__main__':
    main()

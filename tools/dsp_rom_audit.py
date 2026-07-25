#!/usr/bin/env python3
"""Classify local DSP ROM regions without treating fill files as firmware."""

from __future__ import annotations

import argparse
import hashlib
import pathlib
import sys


def classify(data: bytes) -> str:
    if not data:
        return "empty"
    if all(value == data[0] for value in data):
        return f"placeholder-fill-{data[0]:02x}"
    return "nonuniform-data"


def audit(path: pathlib.Path) -> tuple[str, int, str]:
    data = path.read_bytes()
    return classify(data), len(data), hashlib.sha256(data).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("paths", nargs="+", type=pathlib.Path)
    args = parser.parse_args()

    status = 0
    for path in args.paths:
        if not path.is_file():
            print(f"{path}: missing")
            status = 2
            continue
        kind, size, digest = audit(path)
        print(f"{path}: {kind} size={size} sha256={digest}")
    return status


if __name__ == "__main__":
    sys.exit(main())

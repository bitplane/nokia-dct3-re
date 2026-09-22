#!/usr/bin/env python3
"""Run MAME in one gate's directory while preserving legacy path arguments."""

from __future__ import annotations

import argparse
import os
from pathlib import Path


PATH_OPTIONS = {
    "-autoboot_script", "-cfg_directory", "-nvram_directory", "-rompath",
}


def rebase_paths(arguments: list[str], old_cwd: Path) -> list[str]:
    result = list(arguments)
    for index in range(1, len(result)):
        if result[index - 1] in PATH_OPTIONS:
            result[index] = str((old_cwd / result[index]).resolve())
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mame-dir", type=Path, required=True)
    parser.add_argument("--run-dir", type=Path, required=True)
    parser.add_argument("arguments", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    mame_dir = args.mame_dir.resolve()
    run_dir = args.run_dir.resolve()
    run_dir.mkdir(parents=True, exist_ok=True)
    argv = args.arguments
    if argv and argv[0] == "--":
        argv = argv[1:]
    command = [str(mame_dir / "mame"), *rebase_paths(argv, mame_dir)]
    os.chdir(run_dir)
    os.execvpe(command[0], command, os.environ.copy())


if __name__ == "__main__":
    raise SystemExit(main())

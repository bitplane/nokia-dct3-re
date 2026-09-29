#!/usr/bin/env python3
"""Replay the MAME overlay patches against a pinned commit in a temporary index."""

import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def check_patch_stack(repo: Path, commit: str, patches: list[Path]) -> None:
    with tempfile.TemporaryDirectory(prefix="dct3-patch-index-") as directory:
        index = Path(directory) / "index"
        env = {**os.environ, "GIT_INDEX_FILE": str(index)}

        def git(*args: str) -> None:
            subprocess.run(
                ["git", "-C", str(repo), *args],
                env=env,
                check=True,
                capture_output=True,
                text=True,
            )

        git("read-tree", commit)
        for patch in patches:
            try:
                git("apply", "--cached", "--check", str(patch))
                git("apply", "--cached", str(patch))
            except subprocess.CalledProcessError as error:
                detail = error.stderr.strip() or error.stdout.strip()
                raise RuntimeError(f"{patch}: {detail}") from error


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("patches", nargs="+", type=Path)
    args = parser.parse_args()
    try:
        check_patch_stack(
            args.repo.resolve(), args.commit,
            [patch.resolve() for patch in args.patches],
        )
    except (RuntimeError, subprocess.CalledProcessError) as error:
        detail = error.stderr.strip() if isinstance(error, subprocess.CalledProcessError) else str(error)
        parser.exit(1, f"patch stack failed: {detail}\n")
    print(f"patch stack: {len(args.patches)} patches apply to {args.commit}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

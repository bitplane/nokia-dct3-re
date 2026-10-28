#!/usr/bin/env python3
"""Replay the MAME overlay patches against a pinned commit in a temporary index."""

import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def check_patch_stack(repo: Path, commit: str, patches: list[Path], *, apply=False) -> None:
    with tempfile.TemporaryDirectory(prefix="dct3-patch-index-") as directory:
        index = Path(directory) / "index"
        env = {**os.environ, "GIT_INDEX_FILE": str(index)}

        def git(*args: str) -> str:
            return subprocess.run(
                ["git", "-C", str(repo), *args],
                env=env,
                check=True,
                capture_output=True,
                text=True,
            ).stdout

        git("read-tree", commit)
        for patch in patches:
            try:
                git("apply", "--cached", "--check", str(patch))
                git("apply", "--cached", str(patch))
            except subprocess.CalledProcessError as error:
                detail = error.stderr.strip() or error.stdout.strip()
                raise RuntimeError(f"{patch}: {detail}") from error
        if apply:
            paths = git("diff", "--cached", "--name-only", commit).splitlines()
            if not paths:
                return
            comparison = subprocess.run(
                ["git", "-C", str(repo), "diff", "--quiet", "--", *paths],
                env=env, capture_output=True, text=True,
            )
            if comparison.returncode == 0:
                return
            if comparison.returncode != 1:
                raise RuntimeError(comparison.stderr.strip())
            # Collapse the ordered index into one patch. Checking individual
            # reverse patches is invalid when later patches edit their context.
            combined = git("diff", "--cached", "--binary", commit)
            for arguments in (("--check",), ()):
                subprocess.run(
                    ["git", "-C", str(repo), "apply", *arguments, "-"],
                    input=combined, check=True, capture_output=True, text=True,
                )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--apply", action="store_true", help="apply the validated complete stack, or verify an already-patched tree")
    parser.add_argument("patches", nargs="+", type=Path)
    args = parser.parse_args()
    try:
        check_patch_stack(
            args.repo.resolve(), args.commit,
            [patch.resolve() for patch in args.patches],
            apply=args.apply,
        )
    except (RuntimeError, subprocess.CalledProcessError) as error:
        detail = error.stderr.strip() if isinstance(error, subprocess.CalledProcessError) else str(error)
        parser.exit(1, f"patch stack failed: {detail}\n")
    print(f"patch stack: {len(args.patches)} patches apply to {args.commit}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

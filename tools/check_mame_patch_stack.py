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

        def git(*args: str, input: str | None = None) -> str:
            return subprocess.run(
                ["git", "-C", str(repo), *args],
                env=env,
                input=input,
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
            expected = git("write-tree").strip()
            paths = [path for path in git("diff", "--cached", "--no-renames", "--name-only", "-z", commit).split("\0") if path]
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
            combined = git("diff", "--cached", "--binary", commit)
            # Snapshot only stack-owned paths into the disposable index. Never
            # stage overlays/unrelated files or modify the repository's index.
            tracked = set(git("ls-files", "-z", "--", *paths).split("\0"))
            present = [path for path in paths if (repo / path).exists() or (repo / path).is_symlink() or path in tracked]
            if present:
                git("add", "-A", "--", *present)
            current = git("write-tree").strip()

            # Accept a clean base or one missing patch in an otherwise applied
            # stack. The latter supports incremental registrations without
            # trusting per-patch reverse checks with overlapping contexts.
            for candidate in [combined, *(patch.read_text() for patch in patches)]:
                git("read-tree", current)
                try:
                    git("apply", "--cached", "--check", "-", input=candidate)
                    git("apply", "--cached", "-", input=candidate)
                except subprocess.CalledProcessError:
                    continue
                if git("write-tree").strip() != expected:
                    continue
                delta = git("diff", "--binary", current, expected)
                for arguments in (("--check",), ()):
                    subprocess.run(
                        ["git", "-C", str(repo), "apply", *arguments, "-"],
                        input=delta, check=True, capture_output=True, text=True,
                    )
                return
            raise RuntimeError("working tree is neither the pinned base nor a one-patch update; "
                               "unrecognized changes were left untouched")


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

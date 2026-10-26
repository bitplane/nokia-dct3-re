"""Inventory disposable MAME runs; protect referenced evidence and NAND inputs.

Only immediate, real directories beneath the repository's ignored tmp/ are
eligible. Unknown directories, archives, NAND images and referenced runs stay.
Deletion requires the unchanged inventory produced by this tool.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time

TOKEN = re.compile(r'[A-Za-z0-9_.-]+')
PREFIX = re.compile(r'tmp/([A-Za-z0-9_.-]+)[{*]')
ARCHIVES = ('.nand', '.zip', '.rar', '.7z', '.tar', '.tar.gz', '.tgz', '.a00')


def references(text, names):
    tokens = set(TOKEN.findall(text))
    prefixes = PREFIX.findall(text)
    return {name for name in names if name in tokens or
            any(name.startswith(prefix) for prefix in prefixes)}


def snapshot(path):
    size, newest, files, sensitive = 0, 0, 0, False
    for parent, dirs, entries in os.walk(path, followlinks=False):
        dirs[:] = [name for name in dirs if not (Path(parent) / name).is_symlink()]
        for name in entries:
            item = Path(parent) / name
            if item.is_symlink():
                continue
            stat = item.stat()
            size += stat.st_blocks * 512
            newest = max(newest, stat.st_mtime_ns)
            files += 1
            sensitive |= name.lower().endswith(ARCHIVES)
    stat = path.stat()
    return dict(bytes=size, files=files, newest_ns=newest, inode=stat.st_ino,
                device=stat.st_dev, sensitive=sensitive)


def inventory(root):
    scratch = root / 'tmp'
    directories = {p.name: p for p in scratch.iterdir() if p.is_dir() and not p.is_symlink()}
    names = set(directories)
    tracked = subprocess.check_output(['git', 'ls-files', '-z'], cwd=root).decode().split('\0')
    kept = set()
    for name in tracked:
        path = root / name
        if name and path.is_file():
            try:
                kept |= references(path.read_text(), names)
            except UnicodeError:
                pass
    # Follow recorded dependencies, including absolute source/seed paths.
    scanned = set()
    while kept - scanned:
        for name in sorted(kept - scanned):
            scanned.add(name)
            for parent, dirs, files in os.walk(directories[name], followlinks=False):
                dirs[:] = [d for d in dirs if not (Path(parent) / d).is_symlink()]
                for filename in files:
                    path = Path(parent) / filename
                    if path.suffix == '.json' and not path.is_symlink():
                        try:
                            kept |= references(path.read_text(), names)
                        except UnicodeError:
                            pass
    rows = []
    for name, path in sorted(directories.items()):
        state = snapshot(path)
        recognized = any((path / name).is_file() for name in
                         ('error.log', 'console.log', 'error.log.zst', 'console.log.zst'))
        if name in kept:
            reason = 'referenced evidence or dependency'
        elif state['sensitive']:
            reason = 'NAND/source archive retained'
        elif not recognized:
            reason = 'unknown/non-run directory retained'
        elif time.time_ns() - state['newest_ns'] < 24 * 60 * 60 * 10**9:
            reason = 'recent run retained (24-hour safety window)'
        else:
            reason = 'unreferenced generated run'
        rows.append(dict(name=name, delete=reason == 'unreferenced generated run',
                         reason=reason, **state))
    return dict(schema=1, root=str(root), runs=rows)


def active_run(candidate):
    for process in Path('/proc').iterdir():
        if not process.name.isdigit():
            continue
        try:
            cwd = (process / 'cwd').resolve(strict=True)
            if cwd == candidate or candidate in cwd.parents:
                return True
        except (OSError, RuntimeError):
            continue
    return False


def apply(root, report):
    if report.get('schema') != 1 or report.get('root') != str(root):
        raise ValueError('inventory root/schema differs')
    # Recompute references so new docs or seed dependencies cannot be deleted.
    current = {row['name']: row for row in inventory(root)['runs']}
    removed, freed = 0, 0
    for row in report['runs']:
        if not row['delete']:
            continue
        name = row['name']
        if Path(name).name != name or name in ('.', '..'):
            raise ValueError('invalid run name')
        path = root / 'tmp' / name
        if current.get(name) != row or active_run(path):
            raise ValueError('run changed, became protected or is active: ' + name)
        shutil.rmtree(path)
        removed += 1
        freed += row['bytes']
    return removed, freed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    try:
        if args.apply:
            report = json.loads(args.report.read_text())
            count, size = apply(root, report)
            print(f'Removed {count} unchanged disposable runs ({size / 2**30:.2f} GiB)')
        else:
            report = inventory(root)
            args.report.parent.mkdir(parents=True, exist_ok=True)
            args.report.write_text(json.dumps(report, indent=2) + '\n')
            rows = report['runs']
            candidates = [row for row in rows if row['delete']]
            print(f'{len(rows)} runs; {len(candidates)} candidates; '
                  f'{sum(row["bytes"] for row in candidates) / 2**30:.2f} GiB disposable')
            print('Inventory SHA256:', hashlib.sha256(args.report.read_bytes()).hexdigest())
    except (OSError, ValueError) as error:
        parser.exit(1, f'Research-run cleanup: {error}\n')


if __name__ == '__main__':
    main()

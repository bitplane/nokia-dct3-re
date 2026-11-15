#!/usr/bin/env python3
"""Rebuild MAME's TMS320C54x core object with profile-guided optimisation.

The C54x pipeline model is about 80 % of a native ROM4 call's host time and
its cost is spread over thousands of opcode tests, which suits branch
profiles. This compiles only that object instrumented, trains it on the
native lab traffic call (speech both ways), then recompiles it with the
profile and relinks. Profiles only reorder code: the emulation is unchanged.

Run after `make build`; a later ordinary build of the core drops the profile.
"""
import argparse
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile

SOURCE = 'src/devices/cpu/tms320c54x/tms320c54x.cpp'


def core_command(dry_run):
    """The core's compile command from `make -n` output, without the PCH."""
    lines = dry_run.splitlines()
    for marker, command in zip(lines, lines[1:]):
        if marker == f'echo Compiling {SOURCE}...':
            if ' -O3' not in command:
                raise SystemExit('unexpected core compile flags')
            # The precompiled header only matches the original flags.
            return command.replace('-include ../../../../linux_gcc/obj/x64/Release/emu.h',
                                   '-include ../../../../../src/emu/emu.h')
    raise SystemExit(f'no compile command for {SOURCE}')


def with_flags(command, flags):
    return command.replace(' -O3', ' -O3 ' + flags, 1)


def build(mame, project, command, flags, ldflags=None):
    subprocess.run(with_flags(command, flags), shell=True, cwd=project, check=True)
    link = ['make', '-C', str(project), 'config=release64', '-f', 'Makefile', 'mame']
    # Always relink: the object changed but the makefile tracks only its source.
    (mame / 'mame').unlink(missing_ok=True)
    subprocess.run(link + ([f'LDFLAGS={ldflags}'] if ldflags else []), check=True, stdout=subprocess.DEVNULL)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mame-dir', type=Path, default=Path('mame'))
    parser.add_argument('--python', default=sys.executable)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    mame = (root / args.mame_dir).resolve()
    project = mame / 'build/projects/sdl/mame/gmake-linux'
    command = core_command(subprocess.run(['make', '-n', '-B', '-f', 'optional.make', 'config=release64'], cwd=project,
                                          capture_output=True, text=True, check=True).stdout)
    profile = Path(tempfile.mkdtemp(prefix='c54x-pgo.', dir=root / 'tmp'))
    try:
        build(mame, project, command, f'-fprofile-generate -fprofile-update=single -fprofile-dir={shlex.quote(str(profile))}',
              '-fprofile-generate')
        subprocess.run([args.python, 'tools/run_rom4_lab_registration.py', 'nse1ap5', '--traffic-channel'],
                       cwd=root, check=True)
        if not any(profile.rglob('*.gcda')):
            raise SystemExit('training wrote no profile')
        build(mame, project, command, f'-fprofile-use -fprofile-dir={shlex.quote(str(profile))} -Wno-missing-profile')
    finally:
        shutil.rmtree(profile, ignore_errors=True)
    print('C54x core rebuilt with profile-guided optimisation')


if __name__ == '__main__':
    main()

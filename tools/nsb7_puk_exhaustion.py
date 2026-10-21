"""Cold physical NSB-7 PUK exhaustion; preserve each predecessor's storage."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb7_puk_check import card_state, check_puk
from tools.nsb7_storage_check import inspect
from tools.sim_security_trace_check import statuses
from tools.nsb7_pin_check import check_frame

REJECTED_HASH = '28d2c33c296179f37c24b66a42dd40014d9a8671e257f41338ca066c55bce3e0'


def run_chain(root, source, output, pin2=False, check_only=False, resume=False):
    state = card_state
    if pin2:
        from tools.nsb7_puk2_check import card_state as state
    if output.exists() and not (check_only or resume):
        raise ValueError('use a fresh output directory; existing evidence is not overwritten')
    initial = source / 'nvram/nsb7hle'
    if (initial / 'sim_card').read_bytes() != state(0):
        raise ValueError('source must be the organically PIN-exhausted own card with ten PUK retries')
    inspect((initial / 'flash').read_bytes())
    previous = initial
    for attempt in range(1, 12):
        cold = attempt == 11
        run = output / ('permanent-cold' if cold else f'wrong-{attempt:02}')
        storage = run / 'nvram/nsb7hle'
        retained = check_only or (resume and run.exists())
        if not retained:
            storage.mkdir(parents=True)
            (run / 'cfg').mkdir()
            shutil.copy2(root / 'fixtures/nsb7_sim_security/nsb7hle.cfg', run / 'cfg/nsb7hle.cfg')
            for name in ('flash', 'sim_card', 'ccont'):
                shutil.copy2(previous / name, storage / name)
            (run / 'initial_sim_card.bin').write_bytes((storage / 'sim_card').read_bytes())
        before = (run / 'initial_sim_card.bin').read_bytes()
        if before != (previous / 'sim_card').read_bytes():
            raise ValueError('cold run input differs from its actual predecessor')
        script = ('nsb7_puk2_permanent.lua' if cold else 'nsb7_puk2_wrong.lua') if pin2 else (
            'nsb7_puk_permanent.lua' if cold else 'nsb7_puk_wrong.lua')
        command = [sys.executable, str(root / 'tools/run_mame_isolated.py'),
                   '--mame-dir', str(root / 'mame'), '--run-dir', str(run), '--',
                   'nsb7hle', '-rompath', '../roms', '-video', 'none', '-sound', 'none',
                   '-nothrottle', '-seconds_to_run', '75', '-log', '-verbose', '-debug',
                   '-debugger', 'none', '-autoboot_delay', '0', '-autoboot_script',
                   '../tools/' + script]
        if not retained:
            with (run / 'console.log').open('w') as console:
                subprocess.run(command, cwd=root, stdout=console, stderr=subprocess.STDOUT, check=True)
        text = (run / 'error.log').read_text()
        if cold:
            if (storage / 'sim_card').read_bytes() != before or before != state(0, 0):
                raise ValueError('cold permanently blocked card changed')
            if pin2:
                check_pin2_cold(text)
                from tools.nsb7_menu_check import FRAMES
                for name, digest in FRAMES.items():
                    check_frame(run / 'snap' / name, digest)
                inspect((storage / 'flash').read_bytes())
                print('PASS: cold permanent PIN2 block retains PIN1 and physical menus', flush=True)
                continue
            if 'nsb7_puk_permanent: complete' not in text or '[LUA ERROR]' in text or any(
                    statuses(text, ins) for ins in (0x20, 0x24, 0x26, 0x28, 0x2c)):
                raise ValueError('cold permanent rejection retried credentials or failed to complete')
            inspect((storage / 'flash').read_bytes())
            check_frame(run / 'snap/nsb7_puk_permanent.png', REJECTED_HASH)
            print('PASS: cold permanent block retained without credential retry', flush=True)
            continue
        remaining = 10-attempt
        if pin2:
            from tools.nsb7_pin_change_check import check_change
            check_change(text, True, pin2=True, unblock=True, puk_remaining=remaining)
        else:
            check_puk(text, True, remaining=remaining)
        if before != state(0, remaining+1) or (storage / 'sim_card').read_bytes() != state(0, remaining):
            raise ValueError(f'attempt {attempt}: complete-card retry transition differs')
        inspect((storage / 'flash').read_bytes())
        print(f'PASS: cold physical wrong PUK {attempt}/10, remaining={remaining}', flush=True)
        previous = storage


def check_pin2_cold(text):
    from tools.nsb7_pin_check import check_trace
    from tools.nsb7_menu_check import check_trace as check_menu
    check_trace(text.split('nsb7_menu_input:', 1)[0], True)
    check_menu(text)
    if any(statuses(text, ins) for ins in (0x24, 0x26, 0x28, 0x2c)):
        raise ValueError('permanent PIN2 cold control must not mutate credentials')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--pin2', action='store_true')
    parser.add_argument('--check-only', action='store_true', help='validate retained captures without modifying evidence')
    parser.add_argument('--resume', action='store_true', help='revalidate existing complete captures, then continue missing runs')
    args = parser.parse_args()
    try:
        run_chain(Path(__file__).resolve().parents[1], args.source.resolve(), args.output.resolve(),
                  pin2=args.pin2, check_only=args.check_only, resume=args.resume)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'NSB-7 PUK exhaustion: FAIL: {error}\n')


if __name__ == '__main__':
    main()

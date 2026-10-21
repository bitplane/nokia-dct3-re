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


def run_chain(root, source, output):
    if output.exists():
        raise ValueError('use a fresh output directory; existing evidence is not overwritten')
    initial = source / 'nvram/nsb7hle'
    if (initial / 'sim_card').read_bytes() != card_state(0):
        raise ValueError('source must be the organically PIN-exhausted own card with ten PUK retries')
    inspect((initial / 'flash').read_bytes())
    previous = initial
    for attempt in range(1, 12):
        cold = attempt == 11
        run = output / ('permanent-cold' if cold else f'wrong-{attempt:02}')
        storage = run / 'nvram/nsb7hle'
        storage.mkdir(parents=True)
        (run / 'cfg').mkdir()
        shutil.copy2(root / 'fixtures/nsb7_sim_security/nsb7hle.cfg', run / 'cfg/nsb7hle.cfg')
        for name in ('flash', 'sim_card', 'ccont'):
            shutil.copy2(previous / name, storage / name)
        before = (storage / 'sim_card').read_bytes()
        (run / 'initial_sim_card.bin').write_bytes(before)
        command = [sys.executable, str(root / 'tools/run_mame_isolated.py'),
                   '--mame-dir', str(root / 'mame'), '--run-dir', str(run), '--',
                   'nsb7hle', '-rompath', '../roms', '-video', 'none', '-sound', 'none',
                   '-nothrottle', '-seconds_to_run', '42', '-log', '-verbose', '-debug',
                   '-debugger', 'none', '-autoboot_delay', '0', '-autoboot_script',
                   '../tools/' + ('nsb7_puk_permanent.lua' if cold else 'nsb7_puk_wrong.lua')]
        with (run / 'console.log').open('w') as console:
            subprocess.run(command, cwd=root, stdout=console, stderr=subprocess.STDOUT, check=True)
        text = (run / 'error.log').read_text()
        if cold:
            if (storage / 'sim_card').read_bytes() != before or before != card_state(0, 0):
                raise ValueError('cold permanently blocked card changed')
            if 'nsb7_puk_permanent: complete' not in text or '[LUA ERROR]' in text or any(
                    statuses(text, ins) for ins in (0x20, 0x24, 0x26, 0x28, 0x2c)):
                raise ValueError('cold permanent rejection retried credentials or failed to complete')
            inspect((storage / 'flash').read_bytes())
            check_frame(run / 'snap/nsb7_puk_permanent.png', REJECTED_HASH)
            print('PASS: cold permanent block retained without credential retry', flush=True)
            continue
        remaining = 10-attempt
        check_puk(text, True, remaining=remaining)
        if before != card_state(0, remaining+1) or (storage / 'sim_card').read_bytes() != card_state(0, remaining):
            raise ValueError(f'attempt {attempt}: complete-card retry transition differs')
        inspect((storage / 'flash').read_bytes())
        print(f'PASS: cold physical wrong PUK {attempt}/10, remaining={remaining}', flush=True)
        previous = storage


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    try:
        run_chain(Path(__file__).resolve().parents[1], args.source.resolve(), args.output.resolve())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'NSB-7 PUK exhaustion: FAIL: {error}\n')


if __name__ == '__main__':
    main()

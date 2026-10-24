"""Physical NSB-3 cold PUK2 exhaustion and independently cold menu refusal."""
import argparse
from pathlib import Path
import re
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_puk2_check import check_recovery
from tools.nsb3_puk2_retry_check import check_rejected_card
from tools.nsb3_pin_check import check_trace as check_pin
from tools.nsb3_pin_change_check import CHANGE_KEYS
from tools.nsb3_pin2_change_check import MENU_FRAME
from tools.nsb3_puk_check import check_link
from tools.nsb3_phonebook_check import prepare_reload, wire
from tools.nsb3_puk1_exhaust import check_own, execute, stored
from tools.nsk3_puk2_exhaust import trace_text, compress_trace
from tools.nsk1_phonebook_roundtrip import frame_check

ERROR = 'fd5b8fb1ab0c3397bf958d567a46aee84a72f75123802a00d5bc802a1814543e'
REJECTED = 'dad155be27d2f6b1c665380ecea483fa6c2cef44b0b3b2cb43740e956d0f2ff0'


def check_rejection(previous, run, remaining):
    check_link(previous, run)
    check_own(run)
    check_rejected_card(stored(run), remaining)
    check_recovery(trace_text(run), wrong=True, blocked=remaining == 0)
    frame_check(run / 'snap/nsb3_phonecode_menu.png', MENU_FRAME)
    frame_check(run / 'snap/nsb3_security_33.png', ERROR if remaining else REJECTED)


def check_permanent(previous, run, dismiss=False):
    check_link(previous, run)
    check_own(run)
    check_rejected_card(stored(run), 0)
    if stored(run) != stored(previous):
        raise ValueError('permanent PUK2 cold card changed')
    text = trace_text(run)
    check_pin(text, True)
    if any(row[0] in (4, 0x44, 0xd6, 0xdc, 0x32, 0xc2) for row in wire(text)):
        raise ValueError('unrelated storage/Toolkit mutation')
    tail = ('Right Softkey / C', 'End', 'Left Softkey / Menu') if dismiss else ('Scroll Down',)
    keys = CHANGE_KEYS[:12] + ('Scroll Down', 'Left Softkey / Menu') + tail
    contacts = re.findall(r'nsb3_security_key: key=(.*?) down=([01]) t=[0-9.]+ physical_only=1', text)
    if contacts != [(key, state) for key in keys for state in ('1', '0')]:
        raise ValueError('permanent PIN2 refusal/navigation contacts differ')
    frame_check(run / 'snap/nsb3_phonecode_menu.png', MENU_FRAME)
    frame_check(run / 'snap/nsb3_security_13.png',
                '3c437c450b1abdee8fc0e53dd16211268ed594c6f66a9fa4c620ae6ed6a3898a')
    frame_check(run / 'snap/nsb3_security_14.png', REJECTED)
    if dismiss:
        frame_check(run / 'snap/nsb3_security_15.png',
                    'beafe144521e7ce65794b9e26fae65e5e1ccee699d568363aa1e4acb348a270d')
        frame_check(run / 'snap/nsb3_security_17.png', MENU_FRAME)
        frame_check(run / 'snap/nsb3_puk2_dismiss_settled.png',
                    'b2bde13413750ea3a6442a8a4d6c90362d4e8b16cde72bbf2f981a00089cecd0')
        return
    # The neighboring item redraws, but rejection pixels persist even at90s.
    # This is an observed rendering limit, not a clean-menu acceptance oracle.
    frame_check(run / 'snap/nsb3_puk2_permanent_settled.png',
                '9b5b702eb1018abbd927391eab198676a9c2bc61d47e6ddee0c3833d29d1b456')


def chain(seed, wrong, output, check_only=False, resume=False):
    check_link(seed, wrong)
    check_own(seed)
    check_own(wrong)
    check_rejected_card(stored(seed), 10)
    check_rejected_card(stored(wrong), 9)
    check_recovery(trace_text(wrong), wrong=True)
    if output.exists() and not (check_only or resume):
        raise ValueError('requires fresh output or explicit resume/check-only')
    previous = wrong
    for remaining in range(8, -1, -1):
        run = output / f'remaining-{remaining}'
        if not check_only and not (resume and run.exists()):
            prepare_reload(previous, run, lambda card: check_rejected_card(card, remaining + 1))
            execute(run, 'nsb3_puk2_wrong_input.lua', 92)
        check_rejection(previous, run, remaining)
        if not check_only:
            compress_trace(run)
        previous = run
        print(f'PASS: physical wrong PUK2, remaining={remaining}, whole cold card', flush=True)
    for name in ('permanent-menu-qualified', 'permanent-menu-repeat-qualified'):
        run = output / name
        if not check_only and not (resume and run.exists()):
            prepare_reload(previous, run, lambda card: check_rejected_card(card, 0))
            execute(run, 'nsb3_puk2_permanent_input.lua', 92)
        check_permanent(previous, run)
        if not check_only:
            compress_trace(run)
        previous = run
        print(f'PASS: {name}, PIN1/phone authentication and unchanged card', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('seed', 'wrong', 'output'):
        parser.add_argument(name, type=Path)
    parser.add_argument('--check-only', action='store_true')
    parser.add_argument('--resume', action='store_true')
    args = parser.parse_args()
    try:
        chain(args.seed.resolve(), args.wrong.resolve(), args.output.resolve(), args.check_only, args.resume)
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as exc:
        parser.exit(1, f'NSB-3 PUK2 exhaustion: FAIL: {exc}\n')


if __name__ == '__main__':
    main()

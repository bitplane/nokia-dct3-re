"""Own NSK-3 clean-idle architectural restore, DSP I/O and physical Menu."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.make_sim_card_profile import make_current_profile
from tools.nse2_native_state_check import check_state
from tools.nsk3_diagnostic_display_check import (
    FRAMES, check_bank, check_link, contract, input_signature,
)
from tools.nsk3_static_check import inspect
from tools.nsk3_pin_check import check_trace as check_pin
from tools.nsk3_security_check import check_trace as check_phone
from tools.nsk3_boot_check import check_upload_handoffs


def check_trace(text, card, after, reference, restored):
    if text.count('nsk3_state: phase=') != 2:
        raise ValueError('native architecture record count differs')
    report = check_state(text, card, after, reference, restored,
                         prefix='nsk3', checkpoint='40.000000000',
                         card_size=len(card), menu_frame=FRAMES[0])
    # First validate both complete architecture records, including ILLEGAL:0.
    # Their register names are data, not execution-error messages. Preserve
    # every other line for the existing strict authentication/error checks.
    protocol = re.sub(r'^(?:\[luaengine\] )?nsk3_state: phase=(?:saved|restored) t=[^\n]+\n?',
                      '', text, flags=re.M)
    check_pin(protocol, True, tx_limit=4096)
    check_phone(protocol, True)
    check_upload_handoffs(protocol, 1)
    if 'nsk3_reset_request:' in text:
        raise ValueError('board reset interrupted restoration')
    return report


def check(source, seed, run):
    _, image = inspect(source, 'boot4')
    contract(image)
    check_link(seed, run)
    if input_signature(seed / 'cfg/nsk3r4t.cfg') != input_signature(run / 'cfg/nsk3r4t.cfg'):
        raise ValueError('external input configuration changed')
    card = make_current_profile(True)
    for directory in (seed, run):
        storage = directory / 'nvram/nsk3r4t'
        if (storage / 'flash').read_bytes() != image or (storage / 'sim_card').read_bytes() != card:
            raise ValueError('own executable or complete original card changed')
        check_bank((storage / 'eeprom').read_bytes(), True)
    if json.loads((run / 'exit_status.json').read_text())['returncode'] != 0:
        raise ValueError('MAME did not complete')
    text = (run / 'error.log').read_text()
    pixels = []
    for phase in ('reference', 'restored'):
        with Image.open(run / f'snap/nsk3_state_{phase}.png') as frame:
            if frame.size != (84, 48):
                raise ValueError('own LCD geometry differs')
            pixels.append(frame.convert('RGB').tobytes())
    report = check_trace(text, card, (run / 'nvram/nsk3r4t/sim_card').read_bytes(), *pixels)
    saved = run / 'sta/nsk3r4t/nsk3_native_ui.sta'
    if not saved.is_file() or saved.stat().st_size == 0:
        raise ValueError('native saved artifact missing')
    report['save_sha256'] = hashlib.sha256(saved.read_bytes()).hexdigest()
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'seed', 'run'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    try:
        report = check(args.source, args.seed, args.run)
    except (OSError, ValueError, KeyError, ET.ParseError) as error:
        parser.exit(1, f'NSK-3 native restoration FAIL: {error}\n')
    print(f"PASS: own native architecture, {report['io_replay_events']} DSP I/O writes and physical Messages replay; RF/speech unproved")


if __name__ == '__main__':
    main()

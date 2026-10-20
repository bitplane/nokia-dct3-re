"""Verify physical NSB-1 SIM unblock and cold authentication with the new PIN."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.make_sim_card_profile import CHV1, CHV2, PUK1, PUK2, make_profile
from tools.nsb1_pin_roundtrip import (
    ACCEPTED_RGB_SHA256, PHONE_PROMPT_RGB_SHA256, PIN_PROMPT_RGB_SHA256,
    check_contacts,
)
from tools.nsb1_rom4_compare import (
    INPUTS, check_card_run, check_frame, prepare, prepare_cold_run, run_mame,
)
from tools.sim_security_trace_check import (
    AOC_NVRAM_SIZE, ATTEMPTS_OFFSET, CHV_OFFSET, ENABLED_OFFSET, statuses,
)


NEW_PIN = b'4321\xff\xff\xff\xff'
PUK_PROMPT = 'adb3b43fd07660d4d11d8a8fff787086b743f078043d32df4d584f6778549f8d'
NEW_PIN_PROMPT = 'c95ef2cf4ff1aabc393a88453e59039fec25173def9c269d1eb9a581f0a85095'
WRONG_PUK_FRAME = '55b898bce51fda88f082ee5a0fc9e8b35a7a9a5cfa0f6799a2b93eba5373a3f8'


def blocked_profile():
    card = bytearray(make_profile(True))
    card[ATTEMPTS_OFFSET] = 0
    return bytes(card)


def check_card(card):
    if len(card) != AOC_NVRAM_SIZE or card[ENABLED_OFFSET] != 1:
        raise ValueError('unblock changed card geometry or CHV1 enabled state')
    if card[CHV_OFFSET:CHV_OFFSET+32] != NEW_PIN+CHV2+PUK1+PUK2:
        raise ValueError('unblock did not persist only the expected new credential')
    if card[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET+4] != bytes((3,3,10,10)):
        raise ValueError('unblock retry counters changed')


def check_failed_card(card):
    if len(card) != AOC_NVRAM_SIZE or card[ENABLED_OFFSET] != 1:
        raise ValueError('wrong PUK changed card geometry or enabled state')
    if card[CHV_OFFSET:CHV_OFFSET+32] != CHV1+CHV2+PUK1+PUK2:
        raise ValueError('wrong PUK changed credentials')
    if card[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET+4] != bytes((0,3,9,10)):
        raise ValueError('wrong PUK did not consume exactly one PUK retry')


def check_wrong(trace,card,snapshots):
    for phase,count in (('puk',9),('new',5),('confirm',5)):
        check_contacts(trace,phase,count)
    if statuses(trace,0x2c) != [(0x9804,0,9)] or statuses(trace,0x20):
        raise ValueError('wrong PUK APDU lifecycle differs')
    check_failed_card(card)
    check_frame(snapshots/'nsb1_puk_wrong_1.png',PUK_PROMPT)
    check_frame(snapshots/'nsb1_puk_wrong_4.png',WRONG_PUK_FRAME)


def check_unblock(trace, card, snapshots):
    for phase,count in (('puk',9),('new',5),('confirm',5)):
        check_contacts(trace,phase,count)
    if statuses(trace,0x2c) != [(0x9000,3,10)] or statuses(trace,0x20):
        raise ValueError('physical unblock APDU lifecycle differs')
    check_card(card)
    for index,expected in ((1,PUK_PROMPT),(2,NEW_PIN_PROMPT),(4,ACCEPTED_RGB_SHA256)):
        check_frame(snapshots/f'nsb1_puk_unblock_{index}.png',expected)


def check_cold(trace, card, snapshots):
    check_contacts(trace,'pin',5)
    check_contacts(trace,'phone',6)
    if statuses(trace,0x20) != [(0x9000,3,10)] or statuses(trace,0x2c):
        raise ValueError('cold new-PIN authentication lifecycle differs')
    check_card(card)
    for index,expected in ((1,PIN_PROMPT_RGB_SHA256),(2,ACCEPTED_RGB_SHA256),
                            (3,PHONE_PROMPT_RGB_SHA256),(4,ACCEPTED_RGB_SHA256)):
        check_frame(snapshots/f'nsb1_puk_cold_{index}.png',expected)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('binary','rom_root','run_dir'):
        parser.add_argument(name,type=Path)
    parser.add_argument('--checksum-control-from',required=True,type=Path)
    parser.add_argument('--wrong-puk-first',action='store_true',
                        help='prove failed-PUK persistence before cold successful recovery')
    args = parser.parse_args()
    try:
        binary,root,run = args.binary.resolve(),args.rom_root.resolve(),args.run_dir.resolve()
        if run.exists():
            raise ValueError('PUK round trip requires fresh storage')
        first,cold = run/'unblock',run/'cold'
        initial = run/'wrong' if args.wrong_puk_first else first
        rompath = prepare(root,initial,args.checksum_control_from.resolve(),True,True)
        card = blocked_profile()
        (initial/'nvram/nsb1r4t/sim_card').write_bytes(card)
        path = initial/'input_manifest.json'
        manifest = json.loads(path.read_text())
        manifest['synthetic_pin_blocked_profile_sha256'] = hashlib.sha256(card).hexdigest()
        manifest['synthetic_pin_blocked_profile'] = 'laboratory CHV1 retries zero; no donor storage'
        path.write_text(json.dumps(manifest,indent=2)+'\n')
        if args.wrong_puk_first:
            code = run_mame(binary,rompath,initial,Path(__file__).with_name('nsb1_puk_wrong.lua').resolve(),32,True)
            trace,card = check_card_run(initial,code,32)
            check_wrong(trace,card,initial/'snap')
            rompath = prepare_cold_run(first,initial,check_failed_card,INPUTS)
        code = run_mame(binary,rompath,first,Path(__file__).with_name('nsb1_puk_input.lua').resolve(),32,True)
        trace,card = check_card_run(first,code,32)
        check_unblock(trace,card,first/'snap')
        rompath = prepare_cold_run(cold,first,check_card,INPUTS)
        code = run_mame(binary,rompath,cold,Path(__file__).with_name('nsb1_puk_reload.lua').resolve(),26,True)
        trace,card = check_card_run(cold,code,26)
        check_cold(trace,card,cold/'snap')
        (run/'result.json').write_text(json.dumps({
            'physical_unblock':True,'cold_new_pin':True,'wrong_puk_cold_recovery':args.wrong_puk_first,
            'fitted_mask_proved':False,
            'network_registration_proved':False},indent=2)+'\n')
    except (OSError,ValueError,subprocess.TimeoutExpired) as error:
        parser.exit(1,f'NSB-1 PUK round trip failed: {error}\n')
    print(f'NSB-1 physical PUK/new-PIN/cold authentication PASS; compatibility limits retained; evidence={run}')


if __name__ == '__main__':
    main()

"""NSK-3 physical security acceptance/menu and cold storage, not idle/radio."""
import argparse
import hashlib
from pathlib import Path
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsk3_record_check import check,replies
from tools.nsk3_startup_check import check_record_math
from tools.nsk3_static_check import inspect
from tools.nsk3_security_check import check_trace
from tools.nsk3_security_contract import inspect as security_contract
from tools.nsk1_phonebook_roundtrip import frame_check

PROMPT='24cbab0ad5b13376772a44f0eec7835a7ea865795c9ee229c7631fa2e0ea1fb2'
MENU='2ab2adf527d11235a1ce8e0dfbc5b640e0f1011d70c7aadff363cd573b61ff9f'
EEPROM='a096906ffbe735143cce44932ac088b6f8aad8c8ae471c319a90b54ae1d2144a'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','control','native_roms','initial','cold','run','reload'):
        parser.add_argument(name,type=Path)
    args=parser.parse_args()
    try:
        check(args.source,args.control,args.native_roms,args.initial,args.cold)
        _,image=inspect(args.source,'boot4')
        security_contract(image)
        chip=bytes.fromhex(check_record_math((args.control/'error.log').read_text())[0]['chip_field'])
        trace=(args.run/'error.log').read_text()
        check_trace(trace,True,(0x19,))
        replies(trace,chip,True)
        frame_check(args.run/'snap/nsk3_boot_5.png',PROMPT)
        frame_check(args.run/'snap/nsk3_boot_15.png',MENU)
        for run in (args.run,args.reload):
            if (run/'nvram/nsk3r4t/flash').read_bytes()!=image:
                raise ValueError('own executable flash changed')
            if hashlib.sha256((run/'nvram/nsk3r4t/eeprom').read_bytes()).hexdigest()!=EEPROM:
                raise ValueError('reviewed firmware storage writes/cold persistence differ')
        reload=(args.reload/'error.log').read_text()
        replies(reload,chip,True)
        for time in (5,15):frame_check(args.reload/f'snap/nsk3_boot_{time}.png',PROMPT)
    except (ValueError,OSError) as error:
        print(f'NSK-3 menu acceptance failed: {error}',file=sys.stderr)
        return 1
    print('NSK-3 physical code/menu and cold storage PASS; cold phone lock remains, idle/radio unproved')
    return 0


if __name__=='__main__':raise SystemExit(main())

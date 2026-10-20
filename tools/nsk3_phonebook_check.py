"""Physical NSK-3 SIM phonebook save and separate cold UI retrieval."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsk3_static_check import inspect
from tools.nsk3_security_check import check_trace
from tools.nsk3_boot_check import check_upload_handoffs
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.sim_phonebook_check import SUPPORTED_NVRAM_LENGTHS

SAVE_KEYS=(0x1a,0x18,0x19,2,0x19,1,2,3,4,5,0x19)
READ_KEYS=(0x1a,0x19,0x19,0x19)
SAVED='fadcc0b3b9dbabd90d23602e2b3d698ddb88606a2bdd25748a009ae499d2f402'
DETAIL='a089a5b358dd43b2f090d972dadb617c935180f95923efa43f99e9e330c055d0'


def expected_record():
    record=bytearray(b'\xff'*32)
    record[0]=ord('A')
    record[18:23]=bytes.fromhex('04812143f5')
    return bytes(record)


def check_card(data):
    if len(data) not in SUPPORTED_NVRAM_LENGTHS:
        raise ValueError('unknown card storage layout')
    if data[:32]!=expected_record() or data[32:1600]!=b'\xff'*1568:
        raise ValueError('A/12345 record or erased remainder differs')


def transmitted(trace,limit=1024,label='nsk3_sim_tx'):
    rows=re.findall(re.escape(label)+r': index=(\d+) value=([0-9a-f]+)',trace)
    if not rows or len(rows)>=limit or [int(i) for i,_ in rows]!=list(range(1,len(rows)+1)):
        raise ValueError('TX stream incomplete or capture cap reached')
    return bytes(int(value,16) for _,value in rows)


def check_runs(source,write,cold):
    _,image=inspect(source,'boot4')
    streams=[]
    cards=[]
    for run,keys in ((write,SAVE_KEYS),(cold,READ_KEYS)):
        trace=(run/'error.log').read_text()
        check_trace(trace,True,keys)
        check_upload_handoffs(trace,1)
        streams.append(transmitted(trace))
        if (run/'nvram/nsk3r4t/flash').read_bytes()!=image:
            raise ValueError('own executable flash changed')
        card=(run/'nvram/nsk3r4t/sim_card').read_bytes()
        check_card(card)
        cards.append(card)
    update=bytes.fromhex('a0dc010420')
    if streams[0].count(update+expected_record())!=1 or update in streams[1]:
        raise ValueError('organic UPDATE RECORD/cold read-only contrast differs')
    if cards[0]!=cards[1]:raise ValueError('cold card storage changed')
    frame_check(write/'snap/nsk3_phonebook_23.png',SAVED)
    frame_check(cold/'snap/nsk3_phonebook_reload_17.png',DETAIL)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','write','cold'):parser.add_argument(name,type=Path)
    args=parser.parse_args()
    try:check_runs(args.source,args.write,args.cold)
    except (ValueError,OSError) as error:
        print(f'NSK-3 phonebook failed: {error}',file=sys.stderr)
        return 1
    print('NSK-3 physical A/12345 SIM save/cold Details PASS; idle/radio still unproved')
    return 0


if __name__=='__main__':raise SystemExit(main())

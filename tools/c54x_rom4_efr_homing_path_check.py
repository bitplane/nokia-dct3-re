"""Verify acquired EFR patch-path ownership, not codec conformance."""
import argparse
from collections import Counter
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))

SEQUENCES={
    0xc8dc:(0x7719,0,0xf074,0x24a2),
    0x24a2:(0xf073,0x2692),
    0x2692:(0x4a12,0x4a06,0xe782,0x6dea,2,0x6082,0xc8e0,
            0xfa30,0x269e,0x8a06,0x8a12,0xfc00,0xee01,0xf073,0x2870),
    0x28af:(0xea15,0x10f8,0x1343,0x00f8,0x0079,0xf844,0x28f4,
            0x10f8,0x0ab9,0xf845,0x28f4),
    0x28dc:(0x9e0f,),
    0xc8ad:(0xe804,0x18f8,0x0ac8,0xf47e,0xfe00,0x80f8,0x0ab9),
    0xc8c4:(0xfe00,0x8039,0x803a),
    0x28ee:(0x76f8,0x0ab9,0,0x68f8,0x09c0,0xff7f,
            0x76f8,0x0ac7,0,0xf073,0xc8ff),
}
ROW=re.compile(r'efr_homing_path: pc=([0-9a-f]{4}) bfi=([0-9a-f]{4}) flags=([0-9a-f]{4}) recovery=([0-9a-f]{4}) parameter=([0-9a-f]{4}) ar2=([0-9a-f]{4}) sp=([0-9a-f]{4})')
HISTORY=re.compile(r'efr_history_write: pc=([0-9a-f]{4}) address=0ab9 value=([0-9a-f]{4}) bfi=([0-9a-f]{4}) flags=([0-9a-f]{4})')


def check_program(image):
    for address,words in SEQUENCES.items():
        expected=b''.join(word.to_bytes(2,'big') for word in words)
        if image[address*2:address*2+len(expected)]!=expected:
            raise ValueError(f'acquired patch words differ at {address:04x}')


def check_trace(text,staged=False):
    groups=[]
    history=[]
    for line in text.splitlines():
        if line.startswith('efr_history_write:'):
            match=HISTORY.fullmatch(line)
            if not match: raise ValueError('malformed history write')
            history.append(tuple(int(value,16) for value in match.groups()))
        if not line.startswith('efr_homing_path:'): continue
        match=ROW.fullmatch(line)
        if not match: raise ValueError('malformed patch-path observation')
        row=tuple(int(value,16) for value in match.groups())
        if row[0]==0xc8de: groups.append([])
        if not groups: raise ValueError('patch observation precedes wrapper entry')
        groups[-1].append(row)
    if len(groups)!=98: raise ValueError('expected 49 reference/replay frame pairs')
    changed=[]
    for frame in range(49):
        group,replay=groups[frame*2:frame*2+2]
        if group!=replay: raise ValueError('patch path differs after frame restoration')
        path=[row[0] for row in group if row[0]!=0x28dc]
        if path!=[0xc8de,0x2697,0x269e,0x2870,0x28b0,0x28f7,0xc8ff]:
            raise ValueError('original return-address interception path differs')
        # Debug hooks sample frontend execution, not a shared retirement boundary.
        entry_sp=0x1ec2 if staged else 0x1ec3
        if group[0][6]!=entry_sp or group[1][5:]!=(0x1ec2,0x1ec0) or group[3][5:]!=(0x1344,entry_sp):
            raise ValueError('patch return-stack/parameter-pointer contract differs')
        if group[0][4]!=group[-1][4]:
            if (group[0][1],group[0][3],group[0][4],group[-1][4])!=(0,1,11,0):
                raise ValueError('unexpected recovery parameter mutation')
            if not any(row[0]==0x28dc and row[5]==0x1371 for row in group):
                raise ValueError('missing original recovery-store observation')
            changed.append(frame)
    if changed!=[1,24,47]: raise ValueError('recovery frame set differs')
    # Write callbacks report an advanced/sample PC, not necessarily the opcode.
    tail_pc=0xc8b3 if staged else 0xc8b4
    expected=Counter({(tail_pc,0,0):92,(tail_pc,1,1):6,
                      (0xc8c6,0,0):7,(0x28f1,0,0):6})
    if Counter(row[:3] for row in history)!=expected:
        raise ValueError('previous-BFI publication/clear inventory differs')
    if any((flags>>2)&1!=bfi for _,_,bfi,flags in history):
        raise ValueError('history write disagrees with live BFI flag')
    return changed


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('program',type=Path)
    parser.add_argument('log',type=Path)
    parser.add_argument('--staged',action='store_true')
    args=parser.parse_args()
    try:
        check_program(args.program.read_bytes())
        changed=check_trace(args.log.read_text(encoding='utf-8-sig'),staged=args.staged)
    except (OSError,ValueError) as error:
        parser.exit(1,f'EFR patch path: FAIL: {error}\n')
    print(f'PASS: 49 paired original patch paths; recovery mutation frames={changed}; conformance_claim=0')


if __name__=='__main__': main()

"""Physical NSM-1 FDN activation, identity-session handshake and cold Off/On."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsm1_security_check import check_trace as check_phone
from tools.nsm1_published_record_check import records
from tools.nsm1_v523_static_check import SHA1
from tools.nsk3_phonebook_check import transmitted
from tools.sim_security_trace_check import statuses,padded_pin,ATTEMPTS_OFFSET
from tools.sim_fdn_profile import EXTENDED_LENGTH
from tools.nsk1_phonebook_roundtrip import frame_check

PREFIX=(0x19,4,5,3)
FRAMES=('7338f017b8f3789ad3d0fbce0e843e05ec100d2b1e49f3baa01b82b08d578a37',
        '23498f21c080150bf7c905a2cd3a07367646dbf9c55cfdb60a9bb39f3f6e7c34',
        '3f75bfa2a876b1f085d1f68cb7de4d371193c563a53eee05893399eae9456a19',
        '83ce62d3fdc312e107ac2a506aef83746d79d9d9cfcd1273c611d3d7e0a5678d')


def rejected_card(before):
    if len(before)!=EXTENDED_LENGTH or before[3570:3576]!=b'FDN\x01\x01\x01' or before[ATTEMPTS_OFFSET+1]!=3:
        raise ValueError('FDN-Off profile with fresh PIN2 retries required')
    expected=bytearray(before);expected[ATTEMPTS_OFFSET+1]=2
    return bytes(expected)


def validity_targets(tx,instruction):
    offset=0;selected=None;targets=[]
    incoming={0xa4,0x20,0x24,0x26,0x28,0x2c,0xdc,0xd6,0x10,0x14,0xc2,0x88,0x32,0xa2}
    while offset<len(tx):
        if tx[offset]==0xff:
            if offset+3>len(tx):raise ValueError('truncated PPS')
            offset+=3;continue
        if offset+5>len(tx) or tx[offset]!=0xa0:raise ValueError('truncated/unknown APDU header')
        _,ins,p1,p2,length=tx[offset:offset+5];offset+=5
        count=length if ins in incoming else 0
        if offset+count>len(tx):raise ValueError('truncated command body')
        body=tx[offset:offset+count];offset+=count
        if ins==0xa4 and len(body)==2:selected=int.from_bytes(body,'big')
        if ins==instruction:
            if p1 or p2 or length:raise ValueError('validity parameters differ')
            targets.append(selected)
    return targets


def check_trace(trace,phase):
    if phase not in range(4):raise ValueError('unknown FDN phase')
    keys=PREFIX if phase%2 else PREFIX+(0x19,0x17 if phase==0 else 0x18,0x19,5,6,7,8,0x19)
    check_phone(trace,True,keys)
    if trace.index('SIM status ins=20 sw=9000')>trace.index('nsm1_security_submit:'):
        raise ValueError('phone submission preceded SIM authentication')
    application=list(re.finditer(r'nsm1_security_contact: key=[0-9a-f]+ down=1 ',trace))
    if len(application)<7 or application[6].start()<trace.index('nsm1_security_accepted:'):
        raise ValueError('application input preceded phone authentication')
    contacts=re.findall(r'nsm1_pin_contact: correct=1 key=([0-9a-f]+) down=([01]) ',trace)
    if contacts!=[(f'{key:02x}',down) for key in (1,2,3,4,0x19) for down in ('1','0')]:
        raise ValueError('physical startup PIN differs')
    if statuses(trace,0x20)!=[(0x9000,3,10)]*(1 if phase%2 else 2):raise ValueError('VERIFY results differ')
    if any(statuses(trace,ins) for ins in (0x24,0x26,0x28,0x2c)):
        raise ValueError('unexpected credential mutation')
    expected_rehab=([0x6f7e,0x6f07],[0x6f7e,0x6f07],[0x6f7e,0x6f07,0x6f3a],[])[phase]
    tx=transmitted(trace,4096,'nsm1_sim_tx')
    if validity_targets(tx,0x44)!=expected_rehab:raise ValueError('identity/ADN rehabilitation route differs')
    if validity_targets(tx,4)!=([0x6f3a] if phase==0 else []):raise ValueError('ADN invalidation route differs')
    if statuses(trace,0x44)!=[(0x9000,3,10)]*len(expected_rehab):raise ValueError('rehabilitation result differs')
    if statuses(trace,4)!=([(0x9000,3,10)] if phase==0 else []):raise ValueError('invalidation result differs')
    pin1=bytes.fromhex('a020000108')+padded_pin('1234')
    if tx.count(pin1)!=1:raise ValueError('startup PIN APDU differs')
    pin2=bytes.fromhex('a020000208')+padded_pin('5678')
    if tx.count(pin2)!=(0 if phase%2 else 1):raise ValueError('PIN2 APDU differs')
    if not phase%2:
        tx.index(bytes((0xa0,4 if phase==0 else 0x44,0,0,0)),tx.index(pin2)+len(pin2))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('on','on_cold','off','off_cold'):parser.add_argument(name,type=Path)
    parser.add_argument('--rejected',type=Path);parser.add_argument('--retry',type=Path)
    args=parser.parse_args()
    try:
        previous=None
        for phase,run in enumerate((args.on,args.on_cold,args.off,args.off_cold)):
            before=(run/'initial_sim_card.bin').read_bytes();storage=run/'nvram/nsm1r4t'
            if len(before)!=EXTENDED_LENGTH:raise ValueError('extended card required')
            expected=bytearray(before)
            if phase in (0,2):expected[3575]=0 if phase==0 else 1
            if (storage/'sim_card').read_bytes()!=expected:raise ValueError('whole-card transition differs')
            if previous:
                if before!=(previous/'nvram/nsm1r4t/sim_card').read_bytes():raise ValueError('cold card input differs')
                if (run/'initial_eeprom.bin').read_bytes()!=(previous/'nvram/nsm1r4t/eeprom').read_bytes():raise ValueError('cold EEPROM input differs')
            trace=(run/'error.log').read_text();check_trace(trace,phase);records(trace,True)
            if hashlib.sha1((storage/'flash').read_bytes()).hexdigest()!=SHA1:raise ValueError('own flash changed')
            frame_check(run/f'snap/nsm1_fdn_{24 if phase%2 else 32}.png',FRAMES[phase])
            previous=run
        if bool(args.rejected)!=bool(args.retry):raise ValueError('rejected and retry controls must be paired')
        if args.rejected:
            from tools.nsm1_fdn_record_check import check_authentication,ERROR
            before=(args.rejected/'initial_sim_card.bin').read_bytes();wrong=rejected_card(before)
            if (args.rejected/'nvram/nsm1r4t/sim_card').read_bytes()!=wrong:raise ValueError('wrong activation changed more than PIN2 retries')
            trace=(args.rejected/'error.log').read_text()
            check_authentication(trace,PREFIX+(0x19,0x17,0x19,5,6,7,5,0x19),[(0x9000,3,10),(0x9804,3,10)])
            if any(statuses(trace,ins) for ins in (4,0x44,0x24,0x26,0x28,0x2c,0xdc)):
                raise ValueError('rejected activation changed validity or files')
            tx=transmitted(trace,4096,'nsm1_sim_tx')
            if tx.count(bytes.fromhex('a020000208')+padded_pin('5675'))!=1:raise ValueError('wrong activation PIN2 payload differs')
            if (args.retry/'initial_sim_card.bin').read_bytes()!=wrong:raise ValueError('cold retry card link differs')
            if (args.retry/'initial_eeprom.bin').read_bytes()!=(args.rejected/'nvram/nsm1r4t/eeprom').read_bytes():raise ValueError('cold retry EEPROM link differs')
            expected=bytearray(before);expected[3575]=0
            if (args.retry/'nvram/nsm1r4t/sim_card').read_bytes()!=expected:raise ValueError('cold retry changed more than enabling FDN/restoring retries')
            check_trace((args.retry/'error.log').read_text(),0)
            for run in (args.rejected,args.retry):
                records((run/'error.log').read_text(),True)
                if hashlib.sha1((run/'nvram/nsm1r4t/flash').read_bytes()).hexdigest()!=SHA1:raise ValueError('retry flash differs')
            frame_check(args.rejected/'snap/nsm1_fdn_30.png',ERROR)
            frame_check(args.retry/'snap/nsm1_fdn_32.png',FRAMES[0])
    except (OSError,ValueError) as error:
        print(f'NSM-1 FDN cycle failed: {error}',file=sys.stderr);return 1
    print('NSM-1 physical FDN On/cold-On/Off/cold-Off with identity rehabilitation PASS')
    if args.rejected:print('NSM-1 wrong-PIN2 activation denial and exact cold retry recovery PASS')
    return 0


if __name__=='__main__':raise SystemExit(main())

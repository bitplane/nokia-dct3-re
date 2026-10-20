"""Own physical phone-code retry contrast, Messages menu and cold input link."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsm1_published_record_check import records
from tools.nsm1_security_contract import inspect
from tools.nsm1_v523_static_check import SHA1
from tools.nsk1_phonebook_roundtrip import frame_check

MENU='51a20a16acead610184225c78632724f9f20025565bd29a93beffec0c271d3a3'
ERROR='fd5b8fb1ab0c3397bf958d567a46aee84a72f75123802a00d5bc802a1814543e'


def check_trace(trace,accepted,following_keys=(0x19,),extra_codes=()):
    if extra_codes and not accepted:raise ValueError('extra confirmations require accepted-code fixture')
    contacts=re.findall(r'nsm1_security_contact: key=([0-9a-f]+) down=(\d) ',trace)
    if contacts!=[(f'{key:02x}',str(down)) for key in (1,2,3,4,5,0x19)+tuple(following_keys) for down in (1,0)]:
        raise ValueError('physical code/menu input sequence differs')
    submissions=re.findall(r'nsm1_security_submit: text=([0-9a-f]+) caller=([0-9a-f]+)',trace)
    expected=[('3132333435000000','0029faf3')]
    expected.extend((text,'0029faf3') for text in extra_codes)
    if not accepted:expected.append(('0032333435000000','0029faf3'))
    if submissions!=expected:raise ValueError('submitted code differs')
    gates=re.findall(r'nsm1_security_gate: match=([0-9a-f]+) retry_pointer=([0-9a-f]+) retry=([0-9a-f]+)',trace)
    expected=[('00000001','00112824','00' if accepted else '2b')]*(1+len(extra_codes))
    if not accepted:expected.append(('00000000','00112824','2b'))
    if gates!=expected:raise ValueError('matching-code retry contrast differs')
    results=re.findall(r'nsm1_security_accepted: result=([0-9a-f]+)',trace)
    if results!=(['00000001']*(1+len(extra_codes)) if accepted else []):raise ValueError('phone-code acceptance differs')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('blocked','accepted','cold'):parser.add_argument(name,type=Path)
    args=parser.parse_args()
    try:
        before=(args.blocked/'initial_eeprom.bin').read_bytes()
        initial=(args.accepted/'initial_eeprom.bin').read_bytes()
        expected=bytearray(before);expected[0x3d0]=0
        if before[0x3d0]!=0x2b or initial!=expected:raise ValueError('retry fixture changed other EEPROM input')
        previous=None
        for run,accepted in ((args.blocked,False),(args.accepted,True),(args.cold,True)):
            trace=(run/'error.log').read_text();check_trace(trace,accepted);records(trace,True)
            storage=run/'nvram/nsm1r4t'
            flash=(storage/'flash').read_bytes()
            if hashlib.sha1(flash).hexdigest()!=SHA1:raise ValueError('own flash changed')
            inspect(flash)
            saved=(storage/'eeprom').read_bytes()
            source=(run/'initial_eeprom.bin').read_bytes()
            if accepted and saved[0x3cc:0x3d4]!=source[0x3cc:0x3d4]:
                raise ValueError('accepted run changed stored code/security block')
            if run==args.cold and source!=previous:raise ValueError('cold input did not preserve complete saved EEPROM')
            previous=saved
        frame_check(args.blocked/'snap/nsm1_security_12.png',ERROR)
        for run in (args.accepted,args.cold):frame_check(run/'snap/nsm1_security_18.png',MENU)
    except (ValueError,OSError) as error:
        print(f'NSM-1 security qualification failed: {error}',file=sys.stderr);return 1
    print('NSM-1 physical code acceptance, Messages menu and cold saved-input continuation PASS')
    return 0


if __name__=='__main__':raise SystemExit(main())

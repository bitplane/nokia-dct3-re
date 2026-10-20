"""Check physical matching-code/retry rejection versus acceptance, not idle."""
import argparse
from pathlib import Path
import re
import sys


def check_trace(trace,retry_zero,following_keys=()):
    contacts=re.findall(r'nsk3_security_contact: key=([0-9a-f]+) down=1 ',trace)
    if contacts!=['01','02','03','04','05','19']+[f'{key:02x}' for key in following_keys]:
        raise ValueError('own physical digit/submit contacts differ')
    submissions=re.findall(r'nsk3_security_submit: pointer=[0-9a-f]+ text=([0-9a-f]+) caller=([0-9a-f]+)',trace)
    if submissions!=[('3132333435000000','0028a1d7')]:
        raise ValueError('own submitted ASCII code differs')
    gates=re.findall(r'nsk3_security_gate: match=([0-9a-f]+) retry_pointer=([0-9a-f]+) retry=([0-9a-f]+)',trace)
    if gates!=[('00000001','0010c520','00' if retry_zero else 'ff')]:
        raise ValueError('matching-code retry predicate differs')
    accepted=re.findall(r'nsk3_security_accepted: result=([0-9a-f]+)',trace)
    if accepted!=(['00000001'] if retry_zero else []):
        raise ValueError('validator acceptance differs')
    if re.search(r'illegal|unsupported|unhandled|fatal|\[LUA ERROR\]',trace,re.I):
        raise ValueError('execution or observer error')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('blocked',type=Path)
    parser.add_argument('accepted',type=Path)
    args=parser.parse_args()
    try:
        check_trace((args.blocked/'error.log').read_text(),False)
        check_trace((args.accepted/'error.log').read_text(),True)
    except (ValueError,OSError) as error:
        print(f'NSK-3 security predicate failed: {error}',file=sys.stderr)
        return 1
    print('NSK-3 matching-code/retry contrast PASS; accepted validator, idle unproved')
    return 0


if __name__=='__main__':raise SystemExit(main())

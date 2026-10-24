"""Own NSM-1 Phone save/cold selection, contrasted with a code-only cold boot."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsm1_security_check import check_trace
from tools.nsm1_published_record_check import records
from tools.nsm1_v523_static_check import SHA1
from tools.nsm1_phonebook_name_check import CLEAN
from tools.nsk3_phonebook_check import check_card, transmitted, READ_KEYS
from tools.nsk1_phonebook_roundtrip import frame_check

PHONE = 0x19d4
# Reviewed bytes for this one native write, not a general Phone-record grammar.
RECORD = bytes.fromhex('046789060000000600254a8b00000000440000000000000000000000000000000000000005')
WRITE_KEYS = ((0x1a,) + (0x18,) * 4 + (0x19, 0x19, 0x18, 0x19, 0x0f,
              0x1a, 0x18, 0x19, 3, 0x19, 6, 7, 8, 9, 0x19))
COLD_KEYS = (0x1a, 0x19, 0x19, 0x18, 0x19)
FRAMES = {
    'choices': '3f706ee622d335137f582a3697e1cb2bfa856f3fcecb31112c16071d849cb13d',
    'choice_phone': '75d4e75bb5c03598bbfdab6057d4c21871d7091fd4f50fc4843eb5180e1a7894',
    'selected': 'fadb856092813af3ac733d35fe1638447706395a5052b2fb4f86f2ae4c0efbb7',
    'name': 'e3f0fc3eef35fb3021efa188447ac92c9907a006a150e510db94adab55ba8c7a',
    'number': '0e428cda324645404d4d19ba8413aa6260f640607cb1a90ecbae473c979ec251',
    'saved': 'cb96b27ff3e063674329fff1cfe5a90fb4e00553c246e5e98c772ac63d2eb7c4',
}
COLD_LIST = '055e292f373a65b04ae991f4ff9dd91f5608847fdd7ae490b7a189ebf1e0f54c'
COLD_NUMBER = 'bb275966d755aa413f608a833c36b4a4b1279080c21c32f741dd81b7e210a97f'
CONTROL_IDLE = '7c854d1787c07ce6cf7a1027673a8a836e318ac126b39dd6be776e2a1fc21111'


def check_writer_trace(trace):
    check_trace(trace, True, ())
    contacts = re.findall(r'nsm1_phone_local_key: key=([0-9a-f]{2}) down=([01])\s*$', trace, re.MULTILINE)
    expected = [(f'{key:02x}', str(down)) for key in WRITE_KEYS for down in (1, 0)]
    if contacts != expected or trace.count('nsm1_phone_local_key:') != len(expected):
        raise ValueError('Phone save physical contact/release sequence differs')
    markers = re.findall(r'nsm1_phone_local: mode=write (begin|complete)=1\s*$', trace, re.MULTILINE)
    if markers != ['begin', 'complete'] or trace.count('nsm1_phone_local:') != 2:
        raise ValueError('Phone writer did not execute exactly once')
    if not (trace.index('nsm1_security_accepted: result=00000001') <
            trace.index('nsm1_phone_local: mode=write begin=1') <
            trace.index('nsm1_phone_local: mode=write complete=1')):
        raise ValueError('Phone save preceded accepted physical authentication')


def check_writer_storage(before, after):
    if len(before) != 0x4000 or len(after) != len(before) or before[PHONE] != 0:
        raise ValueError('complete erased-slot EEPROM input required')
    if after[PHONE:PHONE + len(RECORD)] != RECORD or after[0x3d6] != 0 or after[0x3e8] != 6:
        raise ValueError('reviewed Phone D/6789 write or settings differ')
    if any(x != y and i not in (0x3d6, 0x3e8) and not PHONE <= i < PHONE + len(RECORD)
           for i, (x, y) in enumerate(zip(before, after))):
        raise ValueError('Phone save changed unrelated EEPROM bytes')


def check_cold_storage(writer, reader, control):
    if len(reader) != len(writer) or len(control) != len(writer) or reader != control:
        raise ValueError('contact reader differs from independent cold boot control')
    if reader[PHONE:PHONE + len(RECORD)] != writer[PHONE:PHONE + len(RECORD)]:
        raise ValueError('cold boot changed the Phone contact')
    if reader[:64] != writer[:64] or reader[0x3cc:0x3d4] != writer[0x3cc:0x3d4]:
        raise ValueError('cold boot changed identity/security inputs')


def check_runs(source, writer, reader, control):
    if len({run.resolve() for run in (source, writer, reader, control)}) != 4:
        raise ValueError('four independent capture directories required')
    previous = source / 'nvram/nsm1r4t'
    before = (previous / 'eeprom').read_bytes()
    card = (previous / 'sim_card').read_bytes()
    check_card(card)
    frame_check(source / 'snap/nsm1_phonebook_read_17.png', CLEAN)
    for run, keys in ((source, READ_KEYS), (writer, None), (reader, COLD_KEYS), (control, ())):
        trace = (run / 'error.log').read_text()
        records(trace, True)
        if keys is None:
            check_writer_trace(trace)
        else:
            check_trace(trace, True, keys)
            if 'nsm1_phone_local_key:' in trace or 'nsm1_phone_local: mode=write' in trace:
                raise ValueError('cold/source fixture contains Phone-save contacts')
        if (b'\xa0\xdc' in transmitted(trace, 4096, 'nsm1_sim_tx') or 'SIM status ins=dc ' in trace):
            raise ValueError('Phone fixture attempted a SIM UPDATE RECORD')
        storage = run / 'nvram/nsm1r4t'
        if hashlib.sha1((storage / 'flash').read_bytes()).hexdigest() != SHA1:
            raise ValueError('own executable flash differs')
        if (storage / 'sim_card').read_bytes() != card:
            raise ValueError('complete SIM changed')
        if run != source and (run / 'initial_sim_card.bin').read_bytes() != card:
            raise ValueError('complete initial SIM link differs')
    saved = (writer / 'nvram/nsm1r4t/eeprom').read_bytes()
    if (writer / 'initial_eeprom.bin').read_bytes() != before:
        raise ValueError('writer did not inherit the complete source EEPROM')
    check_writer_storage(before, saved)
    for run in (reader, control):
        if (run / 'initial_eeprom.bin').read_bytes() != saved:
            raise ValueError('cold input did not inherit the complete saved EEPROM')
    check_cold_storage(saved, (reader / 'nvram/nsm1r4t/eeprom').read_bytes(),
                       (control / 'nvram/nsm1r4t/eeprom').read_bytes())
    for phase, digest in FRAMES.items():
        frame_check(writer / f'snap/nsm1_phone_local_{phase}.png', digest)
    frame_check(reader / 'snap/nsm1_phone_local_read_18.png', COLD_LIST)
    frame_check(reader / 'snap/nsm1_phone_local_read_20.png', COLD_NUMBER)
    frame_check(control / 'snap/nsm1_security_20.png', CONTROL_IDLE)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'writer', 'reader', 'control'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    try:
        check_runs(args.source, args.writer, args.reader, args.control)
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSM-1 Phone storage FAIL: {error}\n')
    print('PASS: physical Phone D/6789 save/cold selection, unchanged SIM and independent boot-write control')


if __name__ == '__main__':
    main()

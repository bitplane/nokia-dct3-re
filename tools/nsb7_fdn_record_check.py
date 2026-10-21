"""Protected NSB-7 FDN Save/cold Details and rejected-PIN2 preservation."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb7_pin_check import check_trace as check_pin, check_frame
from tools.nsb7_security_check import check_trace as check_phone
from tools.nsb7_storage_check import inspect
from tools.nsb7_fdn_check import PREFIX
from tools.nsk3_phonebook_check import expected_record, transmitted
from tools.sim_fdn_profile import EXTENDED_LENGTH
from tools.sim_fdn_protocol_check import apdus, validity_targets
from tools.sim_security_trace_check import statuses, padded_pin, ATTEMPTS_OFFSET

FRAMES = {
    'write': [(75,'fadcc0b3b9dbabd90d23602e2b3d698ddb88606a2bdd25748a009ae499d2f402')],
    'cold': [(53,'1e824b13197f421c04863244c3bc5ea19163d72bbfd74c1ef8acf5a8d1a1bdf8'),
             (56,'acf677164f77fcd6dc7df53c3e98dee65f4ec28e6c6ed69965a3dea358544e9e')],
    'wrong': [(60,'460778ecafd4fb9a23ec8c6632d3b4272b29c63c059b8c2eb764ad810f6c53c8')],
    'cancel': [(72,'2501be7ff54218d5a541e0ae7c0d2a19430bfefe4b550b3d46942d172d88606c'),
               (75,'3a222e40dd0379311978dcbb674c1990e374e2e99f752c8e85a97c7451e187c0')],
    'erase': [(72,'2501be7ff54218d5a541e0ae7c0d2a19430bfefe4b550b3d46942d172d88606c'),
              (75,'b6d00bc47bb41b2a485d56e188c9058fbbb3e943f0b469fdee8d669fec3f4ff4')],
    'empty': [(53,'0e582d0a026f1a5aa59e3c57af1aaa0db3e0feef618ccb6067da9e5ea76f9288')],
    'edit': [(81,'fadcc0b3b9dbabd90d23602e2b3d698ddb88606a2bdd25748a009ae499d2f402')],
    'edited-cold': [(53,'1e824b13197f421c04863244c3bc5ea19163d72bbfd74c1ef8acf5a8d1a1bdf8'),
                    (56,'f25284f55cacd3dcfc231f1ad2b753de0f744991b88b90ba66763c508a78ff2e')],
}


def expected_card(before, wrong=False):
    if len(before) != EXTENDED_LENGTH or before[3570:3578] != b'FDN\x01\x01\x00\x00\x00' or \
            before[3578:] != b'\xff'*320 or before[ATTEMPTS_OFFSET+1] != 3:
        raise ValueError('fresh FDN-On card and PIN2 retries required')
    after = bytearray(before)
    if wrong:
        after[ATTEMPTS_OFFSET+1] = 2
    else:
        after[3578:3610] = expected_record()
    return bytes(after)


def edited_record():
    record = bytearray(expected_record())
    record[22] = 0xf6
    return bytes(record)


def check_trace(text, mode):
    if mode not in ('write', 'cold', 'wrong', 'cancel', 'erase', 'empty', 'edit', 'edited-cold'):
        raise ValueError('unknown FDN record mode')
    check_pin(text.split('nsb7_menu_input:', 1)[0], True)
    check_phone(text)
    keys = PREFIX + [('Left Softkey / Menu', 0x19), ('Scroll Down', 0x18),
                     ('Scroll Down', 0x18), ('Left Softkey / Menu', 0x19)]
    if mode in ('cold','cancel','erase','empty','edit','edited-cold'):
        keys += [('Left Softkey / Menu', 0x19)]*(2 if mode == 'empty' else 3)
        if mode in ('cancel','erase'):
            keys += [('Left Softkey / Menu',0x19)]
            keys += [('Keypad '+str(i),i) for i in (5,6,7,8)]
            keys += [('Left Softkey / Menu',0x19),('Scroll Down',0x18),
                     ('Left Softkey / Menu',0x19),
                     ('Right Softkey / C',0x1a) if mode == 'cancel' else ('Left Softkey / Menu',0x19)]
        elif mode == 'edit':
            keys += [('Left Softkey / Menu',0x19)]
            keys += [('Keypad '+str(i),i) for i in (5,6,7,8)]
            keys += [('Left Softkey / Menu',0x19)]*3
            keys += [('Right Softkey / C',0x1a),('Keypad 6',6),('Left Softkey / Menu',0x19)]
    else:
        keys += [('Scroll Down', 0x18), ('Left Softkey / Menu', 0x19)]
        keys += [('Keypad '+str(i), i) for i in (5,6,7,5 if mode == 'wrong' else 8)]
        keys += [('Left Softkey / Menu', 0x19)]
        if mode == 'write':
            keys += [('Keypad 2', 2), ('Left Softkey / Menu', 0x19)]
            keys += [('Keypad '+str(i), i) for i in (1,2,3,4,5)]
            keys += [('Left Softkey / Menu', 0x19)]
    rows = list(re.finditer(r'nsb7_menu_input: key=(.*?) state=([01]) ', text))
    if [(r[1],r[2]) for r in rows] != [(key,edge) for key,_ in keys for edge in ('1','0')]:
        raise ValueError('FDN record physical sequence differs')
    for index, (_,code) in enumerate(keys):
        if f'nsb7_key_decode: key={code:02x} observer_writes=0' not in text[rows[2*index].end():rows[2*index+1].start()]:
            raise ValueError('FDN record contact did not decode')
    outcomes = [(0x9000,3,10)]
    if mode not in ('cold','empty','edited-cold'):
        outcomes.append((0x9804 if mode == 'wrong' else 0x9000,3,10))
    if statuses(text,0x20) != outcomes:
        raise ValueError('FDN record PIN1/PIN2 result differs')
    if any(statuses(text,ins) for ins in (4,0x24,0x26,0x28,0x2c)):
        raise ValueError('FDN record changed validity or credentials')
    tx = transmitted(text,4096,'nsb7_sim_tx')
    if validity_targets(tx,0x44) != [0x6f7e,0x6f07]:
        raise ValueError('FDN-On startup identity rehabilitation differs')
    if statuses(text,0x44) != [(0x9000,3,10)]*2:
        raise ValueError('FDN-On identity rehabilitation failed')
    updates = [row for row in apdus(tx) if row[0] == 0xdc]
    body = edited_record() if mode == 'edit' else expected_record() if mode == 'write' else b'\xff'*32
    expected = [(0xdc,1,4,32,body,0x6f3b)] if mode in ('write','erase','edit') else []
    if updates != expected:
        raise ValueError('FDN UPDATE target/body differs or rejected/read phase wrote a record')
    if statuses(text,0xdc) != ([(0x9000,3,10)] if mode in ('write','erase','edit') else []):
        raise ValueError('FDN UPDATE outcome differs')
    if mode not in ('cold','empty','edited-cold'):
        pin2 = bytes.fromhex('a020000208')+padded_pin('5675' if mode == 'wrong' else '5678')
        if tx.count(pin2) != 1:
            raise ValueError('FDN record PIN2 payload differs')
        if updates and tx.index(bytes.fromhex('a0dc010420')+updates[0][4]) < tx.index(pin2):
            raise ValueError('protected UPDATE preceded PIN2')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source','write','cold','wrong'):
        parser.add_argument(name,type=Path)
    for name in ('cancel','erase','empty','edit','edit-cold'):
        parser.add_argument('--'+name,type=Path)
    args = parser.parse_args()
    try:
        initial = (args.source/'nvram/nsb7hle/sim_card').read_bytes()
        saved = expected_card(initial)
        for run,mode in ((args.write,'write'),(args.cold,'cold'),(args.wrong,'wrong')):
            if (run/'initial_sim_card.bin').read_bytes() != (saved if mode == 'cold' else initial):
                raise ValueError('FDN record cold/source card link differs')
            storage = run/'nvram/nsb7hle'
            if (storage/'sim_card').read_bytes() != (expected_card(initial,True) if mode == 'wrong' else saved):
                raise ValueError('FDN record whole-card transition differs')
            inspect((storage/'flash').read_bytes())
            check_trace((run/'error.log').read_text(),mode)
            for second,digest in FRAMES[mode]:
                check_frame(run/f'snap/nsb7_menu_{second}.png',digest)
        if any((args.cancel,args.erase,args.empty)) and not all((args.cancel,args.erase,args.empty)):
            raise ValueError('cancel/erase/empty must be qualified together')
        if args.cancel:
            erased = bytearray(saved)
            erased[3578:3610] = b'\xff'*32
            previous = args.cold
            for run,mode in ((args.cancel,'cancel'),(args.erase,'erase'),(args.empty,'empty')):
                if (run/'initial_sim_card.bin').read_bytes() != (previous/'nvram/nsb7hle/sim_card').read_bytes():
                    raise ValueError('FDN erase predecessor chain differs')
                storage = run/'nvram/nsb7hle'
                if (storage/'sim_card').read_bytes() != (saved if mode == 'cancel' else bytes(erased)):
                    raise ValueError('FDN cancel/erase/empty changed unrelated card bytes')
                inspect((storage/'flash').read_bytes())
                check_trace((run/'error.log').read_text(),mode)
                for second,digest in FRAMES[mode]:
                    check_frame(run/f'snap/nsb7_menu_{second}.png',digest)
                previous = run
        if bool(args.edit) != bool(args.edit_cold):
            raise ValueError('edit and edit-cold must be paired')
        if args.edit:
            edited = bytearray(saved)
            edited[3578:3610] = edited_record()
            for run,mode,before in ((args.edit,'edit',saved),(args.edit_cold,'edited-cold',bytes(edited))):
                if (run/'initial_sim_card.bin').read_bytes() != before or \
                        (run/'nvram/nsb7hle/sim_card').read_bytes() != bytes(edited):
                    raise ValueError('FDN edit/cold whole-card transition differs')
                inspect((run/'nvram/nsb7hle/flash').read_bytes())
                check_trace((run/'error.log').read_text(),mode)
                for second,digest in FRAMES[mode]:
                    check_frame(run/f'snap/nsb7_menu_{second}.png',digest)
    except (OSError,ValueError) as error:
        parser.exit(1,f'NSB-7 FDN record: FAIL: {error}\n')
    print('PASS: PIN2-protected FDN Save/cold Details and rejected-write full-card preservation')
    if args.cancel:
        print('PASS: physical erase cancellation, confirmed EF_FDN erase and cold empty search')
    if args.edit:
        print('PASS: physical protected FDN edit and exact cold edited-number retrieval')


if __name__ == '__main__':
    main()

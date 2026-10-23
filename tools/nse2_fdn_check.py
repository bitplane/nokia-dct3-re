"""Check NSE-2 physical FDN validity transitions, not call restrictions."""
import re

from tools.nse2_sim_check import HEADER
from tools.sim_fdn_profile import expected_validity_card
from tools.sim_security_trace_check import ATTEMPTS_OFFSET, statuses

FDN_RECORD = bytes.fromhex('41ffffffffffffffffffffffffffffffffff04812143f5ffffffffffffffffff')
FDN_EDITED_RECORD = FDN_RECORD[:22] + b'\xf6' + FDN_RECORD[23:]


def check_dial(text,before,after,*,enabled):
    if len(before)!=3898 or before[3570:3578]!=b'FDN\x01\x01'+bytes((0 if enabled else 1,1,0)):
        raise ValueError('explicit FDN card validity differs')
    if before[3578:3610]!=FDN_EDITED_RECORD or after!=before:
        raise ValueError('dial lacks saved12346 record or changed card')
    headers=[tuple(int(v,16) for v in m.groups()[:6])+(float(m[7]),)
             for m in HEADER.finditer(text)]
    relevant=[h for h in headers if h[1] in (0x04,0x20,0x24,0x26,0x28,0x2c,0x44,0xd6,0xdc)]
    expected=[(0xa0,0x44,0,0,0,fid) for fid in (0x6f7e,0x6f07)] if enabled else []
    if [h[:6] for h in relevant]!=expected or any(not 4<h[-1]<10 for h in relevant):
        raise ValueError('dial security/file commands differ from cold startup')
    if statuses(text,0x44,chv=2)!=[(0x9000,3,10)]*len(expected):
        raise ValueError('cold active-FDN rehabilitation failed')
    edges=re.findall(r'nse2_fdn_dial_contact: event=(\d+) down=([01]) '
                     r't=([0-9.]+) physical_only=1',text)
    if len(edges)!=12:
        raise ValueError('dial physical contacts differ')
    for i,(event,down,when) in enumerate(edges):
        target=18.5+(i//2)*.35+(i%2)*.15
        if int(event)!=i+1 or int(down)!=1-i%2 or abs(float(when)-target)>.04:
            raise ValueError('dial physical order or timing differs')
    return dict(fdn_enabled=enabled,card_unchanged=True,connected_call_verified=False,
                native_registration_verified=False)


def check_record(text, before, after, mode):
    if mode not in ('write', 'read', 'wrong', 'edit', 'read-edited', 'erase', 'cancel', 'empty'):
        raise ValueError('known FDN record mode required')
    if len(before)!=3898 or before[3570:3578]!=b'FDN\x01\x01\x01\x01\x00':
        raise ValueError('complete explicit FDN-Off card required')
    expected=bytearray(before)
    reading=mode in ('read','read-edited')
    if reading or mode in ('edit','erase','cancel'):
        wanted=FDN_EDITED_RECORD if mode in ('read-edited','erase','cancel') else FDN_RECORD
        if before[3578:3610]!=wanted:
            raise ValueError('cold read lacks physically saved FDN record')
        if mode=='edit':
            expected[3578:3610]=FDN_EDITED_RECORD
        if mode=='erase':
            expected[3578:3610]=b'\xff'*32
    else:
        if before[3578:]!=b'\xff'*320 or before[ATTEMPTS_OFFSET+1]!=3:
            raise ValueError('empty FDN and three PIN2 attempts required')
        if mode=='wrong':
            expected[ATTEMPTS_OFFSET+1]=2
        elif mode=='write':
            expected[3578:3610]=FDN_RECORD
    if after!=expected:
        raise ValueError('FDN record operation changed unexpected storage')
    headers=[tuple(int(v,16) for v in m.groups()[:6])+(float(m[7]),)
             for m in HEADER.finditer(text)]
    security=[h for h in headers if h[1] in (0x20,0x24,0x26,0x28,0x2c,0x04,0x44,0xd6)]
    no_verify=reading or mode=='empty'
    if [h[:6] for h in security] != ([] if no_verify else [(0xa0,0x20,0,2,8,0x6f3b)]):
        raise ValueError('FDN record security command differs')
    if security and not ((25.5<security[0][-1]<26.5) if mode in ('edit','erase','cancel') else (24.5<security[0][-1]<25.5)):
        raise ValueError('FDN PIN2 verification is outside physical submission')
    wanted=[] if no_verify else [(0x9804 if mode=='wrong' else 0x9000,2 if mode=='wrong' else 3,10)]
    if statuses(text,0x20,chv=2)!=wanted:
        raise ValueError('FDN record PIN2 status differs')
    updates=[h for h in headers if h[1]==0xdc]
    if mode in ('write','edit','erase'):
        low,high=(26,27) if mode=='erase' else (27,28)
        if len(updates)!=1 or updates[0][:6]!=(0xa0,0xdc,1,4,32,0x6f3b) or not low<updates[0][-1]<high:
            raise ValueError('FDN UPDATE RECORD target or timing differs')
        if updates[0][-1]<=security[0][-1]:
            raise ValueError('FDN UPDATE RECORD preceded PIN2 verification')
        if statuses(text,0xdc,chv=2)!=[(0x9000,3,10)]:
            raise ValueError('FDN UPDATE RECORD did not succeed')
    elif updates or statuses(text,0xdc,chv=2):
        raise ValueError('non-writing FDN path updated a record')
    if reading and not any(h[:6]==(0xa0,0xb2,1,4,32,0x6f3b) and h[-1]>22 for h in headers):
        raise ValueError('cold menu did not read FDN record one')
    edges=re.findall(r'nse2_fdn_record_contact: event=(\d+) down=([01]) '
                     r't=([0-9.]+) physical_only=1',text)
    count={'write':54,'read':30,'wrong':38,'edit':50,'read-edited':30,'erase':46,'cancel':46,'empty':28}[mode]
    if len(edges)!=count:
        raise ValueError('FDN record physical contact count differs')
    for index,(event,down,when) in enumerate(edges):
        duration=4 if reading and index==count-1 else .15
        wanted=18.5+(index//2)*.35+(duration if index%2 else 0)
        if int(event)!=index+1 or int(down)!=1-index%2 or abs(float(when)-wanted)>.04:
            raise ValueError('FDN record physical timing or order differs')
    return dict(mode=mode,fdn_record_persisted=reading,
                wrong_pin2_rejected=mode=='wrong',call_restrictions_verified=False)


def check_phase(text, before, after, phase):
    expected = expected_validity_card(before, phase)
    if after != expected:
        raise ValueError('FDN changed unrelated card storage or lost validity')
    if before[3576:3578] != b'\x01\x00':
        raise ValueError('FDN probe requires explicit invalid-ADN access profile')
    commands = []
    for match in HEADER.finditer(text):
        cla, ins, p1, p2, p3, selected = (int(v, 16) for v in match.groups()[:6])
        when = float(match[7])
        if ins in (0x20, 0x04, 0x44):
            commands.append((cla, ins, p1, p2, p3, selected, when))
        elif ins in (0x24, 0x26, 0x28, 0x2c):
            raise ValueError('FDN unexpectedly changed security credentials')
    verify = (0xa0, 0x20, 0, 2, 8, 0x6f3b)
    invalidate = (0xa0, 0x04, 0, 0, 0, 0x6f3a)
    rehabilitate = lambda fid: (0xa0, 0x44, 0, 0, 0, fid)
    startup = [rehabilitate(0x6f7e), rehabilitate(0x6f07)]
    wanted = ([verify, invalidate] + startup, startup,
              startup + [verify, rehabilitate(0x6f3a)], [])[phase]
    if [row[:6] for row in commands] != wanted:
        raise ValueError('FDN command order, target or CHV differs')
    if any(a[-1] >= b[-1] for a, b in zip(commands, commands[1:])):
        raise ValueError('FDN command timestamps are not increasing')
    for row in commands:
        # Only an already-active FDN card rehabilitates files at startup.
        boot = phase in (1, 2) and row[:6] in startup
        if not ((4 < row[-1] < 10) if boot else (24 < row[-1] < 25)):
            raise ValueError('FDN command occurred outside its physical lifecycle')
    for ins in (0x20, 0x04, 0x44):
        count = sum(row[1] == ins for row in commands)
        if statuses(text, ins, chv=2) != [(0x9000, 3, 10)] * count:
            raise ValueError('FDN command success or retry counters differ')
    edges = re.findall(r'nse2_fdn_contact: event=(\d+) down=([01]) '
                       r't=([0-9.]+) physical_only=1', text)
    count = 34 if phase in (0, 2) else 18
    if len(edges) != count:
        raise ValueError('FDN physical contact count differs')
    for index, (event, down, when) in enumerate(edges):
        expected_time = 18.5 + (index // 2) * .35 + (index % 2) * .15
        if int(event) != index + 1 or int(down) != (1 - index % 2) or abs(float(when) - expected_time) > .04:
            raise ValueError('FDN physical input order or timing differs')
    return dict(phase=phase, fdn_enabled=phase in (0, 1),
                physical_contacts=count, native_long_run_stability=False,
                call_restrictions_verified=False)

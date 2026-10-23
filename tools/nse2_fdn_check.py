"""Check NSE-2 physical FDN validity transitions, not call restrictions."""
import re

from tools.nse2_sim_check import HEADER
from tools.sim_fdn_profile import expected_validity_card
from tools.sim_security_trace_check import statuses


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

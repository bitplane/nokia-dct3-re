"""Check physical midnight consumption and cold firmware-calendar restoration."""
import re


# Observed own-ROM date scalars after physical entry, not an inferred epoch.
SCALARS = {
    'ordinary': ('d44b1580', 'd44c6700'),
    'leap-day': ('cf640180', 'cf655300'),
    'year-end': ('d4bb2500', 'd4bc7680'),
}


def verify(seed, cold, *, boundary='ordinary'):
    if boundary not in SCALARS:
        raise ValueError('unknown calendar boundary')
    before, after = SCALARS[boundary]
    if re.search(r'LUA ERROR|LUA error', seed + cold):
        raise ValueError('runtime fixture error')
    patterns = (
        rf'8890_calendar: stage=before scalar={before}',
        r'event=second time=23:59:59 day=0',
        r'event=second time=00:00:00 day=1 status=33 mask=50',
        r'event=read reg=0a data=01',
        r'event=counter_write reg=0a data=00',
        rf'8890_calendar: stage=after scalar={after}',
    )
    offset = 0
    for pattern in patterns:
        match = re.search(pattern, seed[offset:])
        if not match:
            raise ValueError('missing ordered midnight observation: ' + pattern)
        offset += match.end()
    if not re.search(r'kind=app_write pc=003062cc address=00137420 '
                     rf'data={after} mask=ffffffff', cold):
        raise ValueError('advanced date was not restored from own journal')
    if not re.search(r'8890_clock_nv_result: result=00000001 flags=00', cold):
        raise ValueError('clock journal rejected')
    if '8890_clock_physical:' in cold:
        raise ValueError('cold fixture re-entered time/date')
    steps = re.findall(r'8890_calendar_physical: step=(\d+)', cold)
    if steps != [str(index) for index in range(1, 10)]:
        raise ValueError('physical Calendar selection incomplete')

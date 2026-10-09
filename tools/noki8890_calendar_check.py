"""Check physical midnight consumption and cold firmware-calendar restoration."""
import re


# Observed own-ROM date scalars after physical entry, not an inferred epoch.
SCALARS = {
    'non-leap': ('d1468680', 'd147d800'),
    'ordinary': ('d44b1580', 'd44c6700'),
    'leap-day': ('cf640180', 'cf655300'),
    'year-end': ('d4bb2500', 'd4bc7680'),
}


def verify_restore(text):
    if re.search(r'LUA ERROR|LUA error|calendar_restore: FAIL', text):
        raise ValueError('Calendar restoration fixture failed')
    pattern = (r'8890_calendar_restore: event=(saved|reference|restored|replayed) '
               r't=([0-9.]+) pc=([0-9a-f]{8}) sp=([0-9a-f]{8}) '
               r'ram=([0-9a-f]{8}) cpu=([0-9a-f,]+) date=([0-9a-f]{8})')
    records = re.findall(pattern, text)
    if [record[0] for record in records] != ['saved', 'reference', 'restored', 'replayed']:
        raise ValueError('missing or duplicated Calendar restoration observations')
    if records[0][1:] != records[2][1:] or records[1][1:] != records[3][1:]:
        raise ValueError('Calendar CPU/RAM/time/date replay differs')
    for record in records:
        if len(record[5].split(',')) != 37:
            raise ValueError('incomplete banked ARM architecture')
    if (records[0][1], records[1][1], records[0][6], records[1][6]) != (
            '80.000000000', '100.000000000', 'd44b1580', 'd44c6700'):
        raise ValueError('Calendar checkpoint does not cross the reviewed midnight')
    for start, end in (('saved', 'reference'), ('restored', 'replayed')):
        window = text.split('8890_calendar_restore: event=' + start, 1)[1].split(
            '8890_calendar_restore: event=' + end, 1)[0]
        cursor = 0
        for event in ('event=second time=00:00:00 day=1 status=33 mask=50',
                      'event=read reg=0a data=01', 'event=counter_write reg=0a data=00'):
            position = window.find(event, cursor)
            if position < 0:
                raise ValueError('missing ordered RTC midnight replay: ' + event)
            cursor = position + len(event)
    if text.count('8890_calendar_restore: result=pass elapsed=20 native_speech=0') != 1:
        raise ValueError('Calendar restoration completion absent or duplicated')


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

"""Verify logged ARM/banked architecture, RAM digest and time across one load."""
import re


def verify_roundtrip(text, *, prefix, scenario):
    if f'{prefix}: FAIL' in text:
        raise ValueError('architectural fixture did not complete')
    rows = re.findall(re.escape(prefix) + r': event=(saved|restored) pc=([0-9a-f]{8}) '
                      r'sp=([0-9a-f]{8}) ram=([0-9a-f]{8}) cpu=([0-9a-f,]+) t=([0-9]+\.[0-9]{9})', text)
    if (text.count(f'{prefix}: event=') != 2 or len(rows) != 2 or
            [row[0] for row in rows] != ['saved', 'restored'] or rows[0][1:] != rows[1][1:]):
        raise ValueError('CPU/RAM/time restoration differs')
    for row in rows:
        if not re.fullmatch(r'[0-9a-f]{8}(?:,[0-9a-f]{8}){36}', row[4]):
            raise ValueError('incomplete ARM/banked register inventory')
        registers = row[4].split(',')
        if row[1] != registers[15] or row[2] != registers[13]:
            raise ValueError('PC/SP snapshot disagrees with register inventory')
    marker = f'state_roundtrip: result=pass scenario={scenario} '
    roundtrips = re.findall(re.escape(marker) +
                           r'requested_at=([0-9]+\.[0-9]{9}) t=([0-9]+\.[0-9]{9})', text)
    if text.count(marker) != 1 or roundtrips != [(rows[0][5], rows[1][5])]:
        raise ValueError('restoration marker disagrees with architectural timeline')

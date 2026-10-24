"""Check request-derived NSM-3 lab timing failure and organic registration."""
import re

ROW = re.compile(
    r'8210_timing_result: site=(dispatch|receiver|update|returned) ordinal=(\d+) '
    r'message=([0-9a-f]{8}) error=([0-9a-f]{2}) target=([0-9a-f]{4}) '
    r'record=([0-9a-f]{8}) carrier=([0-9a-f]{4}) control=([0-9a-f]{8}) '
    r'errors=([0-9a-f]{2}) t=([0-9.]+) observer_writes=0')


def verify(text):
    rows = [match.groups() for match in ROW.finditer(text)]
    if text.count('8210_timing_result:') != 4 or len(rows) != 4:
        raise ValueError('missing, duplicate or malformed timing-result observation')
    if [row[0] for row in rows] != ['dispatch', 'receiver', 'update', 'returned']:
        raise ValueError('timing-result consumer order differs')
    if any(row[1] != '1' or row[3:5] != ('01', '0338') for row in rows):
        raise ValueError('timing result is not the requested carrier failure')
    if len({row[2] for row in rows}) != 1:
        raise ValueError('timing-result object identity changed')
    if any(not 0x100000 <= int(row[2], 16) < 0x17fff0 for row in rows):
        raise ValueError('timing-result object is outside ordinary RAM')
    if [row[6] for row in rows] != ['0337', '0337', '0338', '0338']:
        raise ValueError('receiver did not reselect the directed target')
    before, after = rows[2], rows[3]
    if before[5] != after[5] or rows[0][5] == before[5]:
        raise ValueError('timing failure aliases the serving record')
    old, new = int(before[7], 16), int(after[7], 16)
    if old >> 16 & 15 != 2 or new != ((old & ~0x000f0000) | 0x00010000) or (before[8], after[8]) != ('00', '01'):
        raise ValueError('firmware failure counter/selector update differs')
    times = list(map(lambda row: float(row[9]), rows))
    if times != sorted(times) or not 12 < times[0] < times[-1] < 13:
        raise ValueError('timing-result observations are out of order or phase')
    markers = [
        r'directed timing failure requested arfcn=824 mode=4',
        # The HLE logs TX after invoking its peer receiver.
        r'TX packet type=11 payload=12[^\n]*data=040000000000033800000000',
        r'RX enqueue type=88 payload=10[^\n]*data=01000000033800000001',
        r'directed timing failure published arfcn=824 mode=4 status=1',
        r'8210_timing_result: site=dispatch',
        r'8210_timing_result: site=returned',
        r'LAPDm Location Updating Accept acknowledged nr=1',
        r'neighbour measurement instruction arfcn=824[^\n]*accepted=1',
    ]
    position = 0
    for marker in markers:
        match = re.search(marker, text[position:])
        if not match:
            raise ValueError('missing ordered request/failure/consumer/registration chain')
        position += match.end()
    if text.count('RX enqueue type=88 ') != 1 or text.count('directed timing failure requested ') != 1:
        raise ValueError('unexpected additional timing-failure publications')
    return {'scope': 'lab HLE failed directed timing followed by organic registration',
            'target_arfcn': 824, 'serving_arfcn': 823, 'firmware_selector': [2, 1],
            'native_dsp_claim': False, 'physical_rf_claim': False}

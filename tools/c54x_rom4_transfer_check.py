"""Check firmware-owned timed descriptor chains, not physical RF timing."""
import re


def records(text, prefix, fields):
    result = []
    pattern = re.compile(prefix + ': ' + fields + '$')
    for line in text.splitlines():
        if prefix + ':' not in line:
            continue
        match = pattern.search(line)
        if not match:
            raise ValueError('malformed ' + prefix)
        result.append(match.groupdict())
    return result


def words(value):
    return [int(item, 16) for item in value.split(',')]


def check(text, *, minimum_seconds=8):
    controls = records(text, 'rom4_transfer_control',
        r'sequence=(?P<sequence>\d+) value=(?P<value>[0-9a-f]{4}) mask=ffff pc=(?P<pc>[0-9a-f]{4}) '
        r'flags=[0-9a-f,]+ pointers=(?P<pointers>[0-9a-f,]+) descriptor=[0-9a-f,]+ '
        r'counter=[0-9a-f]{4} counter_age=[0-9.]+ t=(?P<time>[0-9.]+)')
    tables = records(text, 'rom4_transfer_table',
        r'descriptors=(?P<descriptors>[0-9a-f,]+) payload=(?P<payload>[0-9a-f,]+) t=(?P<time>[0-9.]+)')
    pairs = records(text, 'rom4_transfer_pair',
        r'sequence=(?P<sequence>\d+) low=(?P<low>[0-9a-f]{4}) high=(?P<high>[0-9a-f]{4}) '
        r'pc=(?P<pc>[0-9a-f]{4}) t=(?P<time>[0-9.]+)')
    timers = records(text, 'rom4_transfer_timer',
        r'expiry=(?P<sequence>\d+) target=(?P<target>[0-9a-f]{4}) counter=(?P<counter>[0-9a-f]{4}) '
        r'delay=(?P<delay>\d+) t=(?P<time>[0-9.]+)')
    summaries = records(text, 'rom4_transfer_summary',
        r'controls=(?P<controls>\d+) requests=(?P<requests>\d+) service_requests=(?P<service>\d+) '
        r'pairs=(?P<pairs>\d+) completion_injected=0 t=(?P<time>[0-9.]+)')
    if len(summaries) != 1 or float(summaries[0]['time']) < minimum_seconds - 0.1:
        raise ValueError('missing complete transfer summary')
    summary = summaries[0]
    inventory = records(text, 'rom4_transfer_pair_count',
        r'pc=(?P<pc>[0-9a-f]{4}) low=(?P<low>[0-9a-f]{4}) high=(?P<high>[0-9a-f]{4}) count=(?P<count>\d+)')
    keys = [(row['pc'], row['low'], row['high']) for row in inventory]
    if len(set(keys)) != len(keys) or any(int(row['count']) <= 0 for row in inventory):
        raise ValueError('invalid transfer pair inventory')
    if sum(int(row['count']) for row in inventory) != int(summary['pairs']):
        raise ValueError('transfer pair inventory does not cover all writes')
    by_producer = {}
    for row in inventory:
        by_producer[row['pc']] = by_producer.get(row['pc'], 0) + int(row['count'])
    if by_producer.get('a240') != 4 or by_producer.get('3712') != 20:
        raise ValueError('transfer pair producer totals changed')
    for rows, count, cap in ((controls, int(summary['controls']), 64), (pairs, int(summary['pairs']), 64)):
        if len(rows) != min(count, cap) or [int(row['sequence']) for row in rows] != list(range(1, len(rows) + 1)):
            raise ValueError('missing or unordered transfer observations')
    prefix_counts = {}
    for row in pairs:
        key = (row['pc'], row['low'], row['high'])
        prefix_counts[key] = prefix_counts.get(key, 0) + 1
    inventory_counts = dict(zip(keys, (int(row['count']) for row in inventory)))
    if any(inventory_counts.get(key, 0) < count for key, count in prefix_counts.items()):
        raise ValueError('transfer pair inventory contradicts captured prefix')
    if len(controls) != int(summary['controls']):
        raise ValueError('transfer-control capture is truncated')
    expiry_counts = re.findall(r'\btransfer_expiries=(\d+)\b', text)
    if len(expiry_counts) != 1 or len(timers) != min(int(expiry_counts[0]), 256):
        raise ValueError('missing transfer timer accounting')
    if [int(row['sequence']) for row in timers] != list(range(1, len(timers) + 1)):
        raise ValueError('unordered transfer timer prefix')
    hist = records(text, 'rom4_transfer_request_count', r'value=(?P<value>[0-9a-f]{4}) count=(?P<count>\d+)')
    if len({row['value'] for row in hist}) != len(hist):
        raise ValueError('duplicate request histogram values')
    if sum(int(row['count']) for row in hist) != int(summary['requests']):
        raise ValueError('request histogram does not cover all reads')
    if any(int(row['value'], 16) & ~0x100a for row in hist):
        raise ValueError('unmodeled request bits appeared')
    if sum(int(row['count']) for row in hist if int(row['value'], 16) & 0x1000) != int(summary['service']):
        raise ValueError('transfer service histogram differs')
    if int(summary['service']) < 20:
        raise ValueError('missing native transfer-service requests')
    starts = [i for i, row in enumerate(controls) if row['pc'] == 'a244']
    if len(starts) != 4 or len(tables) != 4:
        raise ValueError('expected four firmware-owned transfer bursts')
    output = []
    for start, table in zip(starts, tables):
        chain = controls[start:start + 6]
        descriptors, payload = words(table['descriptors']), words(table['payload'])
        if len(descriptors) != 12 or len(payload) != 12:
            raise ValueError('incomplete original transfer table')
        positions = descriptors[1::2]
        if positions != [2100, 2180, 2260, 2340, 2420, 15000]:
            raise ValueError('transfer descriptor positions changed')
        if len(chain) != 6 or [int(row['value'], 16) for row in chain] != positions:
            raise ValueError('native descriptor rearming did not reach its sentinel')
        if [row['pc'] for row in chain] != ['a244'] + ['3717'] * 5 or table['time'] != chain[0]['time']:
            raise ValueError('descriptor ownership or table correlation changed')
        if words(chain[0]['pointers']) != [0x1914, 0x1920]:
            raise ValueError('initial queue pointers differ')
        previous = float(controls[start - 1]['time']) if start else -1
        initial = [row for row in pairs if previous < float(row['time']) <= float(chain[0]['time'])]
        if not initial or initial[-1]['pc'] != 'a240' or [int(initial[-1][key], 16) for key in ('low', 'high')] != payload[:2]:
            raise ValueError('initial native pair does not match its source table')
        for index in range(5):
            begin, end = float(chain[index]['time']), float(chain[index + 1]['time'])
            emitted = [row for row in pairs if begin < float(row['time']) <= end]
            if len(emitted) != 1 or emitted[0]['pc'] != '3712':
                raise ValueError('missing or interleaved queued transfer')
            if [int(emitted[0][key], 16) for key in ('low', 'high')] != payload[2 * index + 2:2 * index + 4]:
                raise ValueError('queued pair differs from firmware-owned data')
            matched = [row for row in timers if begin < float(row['time']) < float(emitted[0]['time'])]
            if len(matched) != 1 or int(matched[0]['target'], 16) != positions[index] or matched[0]['target'] != matched[0]['counter']:
                raise ValueError('queued transfer lacks its matching counter event')
            if words(chain[index + 1]['pointers']) != [0x1916 + 2 * index, 0x1924 + 2 * index]:
                raise ValueError('firmware queue pointers did not advance')
        output.append(dict(start=float(chain[0]['time']), positions=positions, payload=payload))
    return dict(schema=2, bursts=output, queued_pairs=20, controls=int(summary['controls']),
                total_pairs=int(summary['pairs']), pairs_by_producer=by_producer,
                pair_inventory=inventory,
                timer_expiries=int(expiry_counts[0]), captured_timer_prefix=len(timers),
                service_reads=int(summary['service']), physical_timing_claim=False,
                acquisition_claim=False, handset_speech_claim=False)

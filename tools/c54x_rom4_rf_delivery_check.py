#!/usr/bin/env python3
"""Check the observed native ROM4 scan-response delivery, not RF acquisition."""
import argparse
from pathlib import Path
import re
import sys


EVENT = re.compile(r"rom4_rf_(\w+): (.*)")
FIELDS = re.compile(r"(\w+)=([^ ]+)")


def check_observer_health(text):
    if '[LUA ERROR]' in text or 'Error running autoboot script' in text:
        raise ValueError('native RF observer failed; partial trace is not acceptance evidence')


def decode_search_bitmap(words):
    """Static 48f7/4909 reference; physical band interpretation is separate."""
    if len(words) != 33 or any(not 0 <= word <= 0xffff for word in words):
        raise ValueError("search request requires 33 captured words")
    if words[0] & 1:
        raise ValueError("default-range request bypasses bitmap expansion")
    scratch = [words[1] & 0xff] + words[2:]
    entries = []
    for group, word in enumerate(reversed(scratch)):
        base = 1 + group * 16 if group < 8 else 512 + (group - 8) * 16
        entries.extend(base + bit for bit in range(16) if word & (1 << bit))
    return entries


def check_request(text):
    request = re.search(r"rom4_rf_search_request: t=([\d.]+) ar2=[0-9a-f]+ words=([0-9a-f,]+)", text)
    if request is None:
        raise ValueError("missing native search request")
    entries = decode_search_bitmap([int(word, 16) for word in request[2].split(",")])
    if len(entries) != 1:
        raise ValueError("single-entry control did not decode exactly one entry")
    scan = re.search(r"rom4_rf_scan_entry: address=3129 (.*)", text[request.end():])
    if scan is None:
        raise ValueError("missing subsequent native scan entry")
    fields = dict(FIELDS.findall(scan[1]))
    if float(fields["t"]) < float(request[1]):
        raise ValueError("scan precedes request")
    if int(fields["selected1867"], 16) != entries[0] or int(fields["cursor_word"], 16) != entries[0]:
        raise ValueError("native selected entry differs from request bitmap")
    return entries[0]


def check_tuning(text):
    """Validate the captured selected-entry port pair, not synthesizer layout."""
    writes = [dict(FIELDS.findall(match[1])) for match in
              re.finditer(r"rom4_rf_tuning_write: (.*)", text)]
    for index, low in enumerate(writes[:-1]):
        if low.get("pc") != "4023" or low.get("selected1867") != "006d":
            continue
        high = writes[index + 1]
        if (low.get("port") != "0031" or high.get("port") != "0032"
                or high.get("pc") != "4028" or low.get("ar2") != "0031"
                or high.get("selected1867") != low.get("selected1867")
                or high.get("ar2") != "0031" or high.get("a") != low.get("a")
                or float(high["t"]) < float(low["t"])):
            raise ValueError("selected-entry programming pair is inconsistent")
        accumulator = int(low["a"], 16)
        if int(low["value"], 16) != accumulator & 0xffff or int(high["value"], 16) != (accumulator >> 16) & 0xffff:
            raise ValueError("programming ports differ from accumulator halves")
        return accumulator
    raise ValueError("missing selected-entry programming pair")


def pack_tuning_nonnegative(value):
    """403a..4048 reference for the captured nonnegative, non-doubled path."""
    if not 0 <= value <= 0xffff:
        raise ValueError("packing reference requires a nonnegative 16-bit input")
    quotient, remainder = divmod(value, 64)
    return (quotient << 7) | remainder | (0x30 << 16)


def nse1_gsm900_tuning_consistency(packed):
    """Manual/ROM numeric inference, not a validated synthesizer-wire decoder."""
    if not 0 <= packed <= 0xffffffff or packed >> 16 != 0x30 or packed & 0x40:
        raise ValueError("not the bounded NSE-1 packing family")
    low = packed & 0xffff
    divisor = (low >> 7) * 64 + (low & 0x3f)
    channel = divisor - 0x13a6
    if not 1 <= channel <= 124:
        raise ValueError("numeric inference is bounded to P-GSM 900")
    lo_hz = divisor * 200000
    rx_hz = 935000000 + channel * 200000
    tx_hz = 890000000 + channel * 200000
    if lo_hz - rx_hz != 71000000 or lo_hz - tx_hz != 116000000:
        raise ValueError("divider does not match the NSE-1 frequency plan")
    return dict(channel=channel, lo_hz=lo_hz, rx_hz=rx_hz, tx_hz=tx_hz,
                evidence_class="numeric-consistency-inference", wire_validated=False)


def nsk1_dcs1800_tuning_consistency(packed, direction):
    """Own NSK-1 addends versus the NSK-3 frequency plan, not wire decoding."""
    if direction not in ('rx','tx'):
        raise ValueError('explicit rx or tx direction is required')
    if not 0 <= packed <= 0xffffffff or packed >> 16 != 0x30 or packed & 0x40:
        raise ValueError('not the bounded NSK-1 packing family')
    low = packed & 0xffff
    divisor = (low >> 7)*64 + (low & 0x3f)
    channel = divisor - (0x17bf if direction == 'rx' else 0x1797)
    if not 512 <= channel <= 885:
        raise ValueError('numeric inference is bounded to DCS 1800')
    tx_hz = 1710200000 + (channel-512)*200000
    rx_hz = tx_hz + 95000000
    lo_hz = divisor*200000
    if_hz = (rx_hz if direction == 'rx' else tx_hz)-lo_hz
    if if_hz != (487000000 if direction == 'rx' else 400000000):
        raise ValueError('divider does not match the bounded NSK frequency plan')
    return dict(channel=channel,direction=direction,lo_hz=lo_hz,rx_hz=rx_hz,
                tx_hz=tx_hz,if_hz=if_hz,evidence_class='numeric-consistency-inference',
                wire_validated=False)


def check_tuning_stages(text):
    stages = [dict(FIELDS.findall(match[1])) for match in
              re.finditer(r"rom4_rf_tuning_stage: (.*)", text)]
    selected = [stage for stage in stages if stage.get("selected1867") == "006d"]
    expected = ("3ffb", "400f", "402e", "403a", "4048")
    if tuple(stage.get("address") for stage in selected) != expected:
        raise ValueError("missing or reordered selected-entry packing stages")
    times = [float(stage["t"]) for stage in selected]
    if times != sorted(times):
        raise ValueError("packing stage timestamps regress")
    result = check_tuning_values(selected)
    if check_tuning(text) != result:
        raise ValueError('programming ports differ from the packing result')
    return result


def check_tuning_values(selected):
    """Shared numeric contract; sampling ownership is checked by the caller."""
    required = {'address', 'a', 'b', 'ar2', 'flags6be', 'flags216a', 'addend216b', 'field60'}
    if any(not required <= stage.keys() for stage in selected):
        raise ValueError('missing tuning operand fields')
    if tuple(stage.get('address') for stage in selected) != ('3ffb', '400f', '402e', '403a', '4048'):
        raise ValueError('missing or reordered selected-entry packing stages')
    if any(int(stage["flags6be"], 16) & 0x80 or stage["flags216a"] != "0004"
           or stage["ar2"] != "0031" for stage in selected):
        raise ValueError("capture does not use the bounded packing branch")
    entry, addition, transformed, packing, returned = selected
    argument = int(entry["a"], 16)
    addend = int(addition["addend216b"], 16)
    value = argument + addend
    if argument != 0x6d or int(addition["a"], 16) != argument or int(addition["b"], 16) != addend:
        raise ValueError("selected-entry addition operands differ")
    if int(transformed["a"], 16) != value or int(packing["a"], 16) != value:
        raise ValueError("packing input differs from selected entry plus addend")
    result = pack_tuning_nonnegative(value)
    if (int(returned["a"], 16) != result or int(returned["b"], 16) != result
            or int(returned["field60"], 16) != value // 64):
        raise ValueError("packing result differs from observed quotient or port pair")
    return result


def check_execution_tuning(text):
    """Correlate execution-entry packing with actual uploads and port writes."""
    marker = 'rom4_rf_tuning_probe: points=3ffb,400f,402e,403a,4048 phase=instruction_entry'
    if text.count(marker) != 1:
        raise ValueError('missing or duplicate execution-owned tuning probe')
    events = [(index, match[1], dict(FIELDS.findall(match[2])))
              for index, line in enumerate(text.splitlines())
              if (match := EVENT.search(line))]
    stages = [(index, fields) for index, name, fields in events if name == 'execution_tuning']
    if not stages or len(stages) % 5:
        raise ValueError('incomplete execution-owned tuning groups')
    uploaded = [(index, fields) for index, name, fields in events
                if name == 'tuning_field_write' and fields.get('space') == 'program']
    expected = (('216a', '0004'), ('216b', '13a6'), ('216c', '13a6'))
    if (tuple((fields.get('address'), fields.get('value')) for _, fields in uploaded) != expected
            or any(fields.get('pc') != '31d7' for _, fields in uploaded)
            or uploaded[-1][0] >= stages[0][0]):
        raise ValueError('tuning upload differs or follows execution-owned packing')
    if any('t' not in fields for _, fields in uploaded):
        raise ValueError('missing tuning upload timestamps')
    upload_times = [float(fields['t']) for _, fields in uploaded]
    if upload_times != sorted(upload_times):
        raise ValueError('tuning upload timestamps regress')
    result = None
    for offset in range(0, len(stages), 5):
        group = stages[offset:offset + 5]
        if any(fields.get('selected1867') != '006d' for _, fields in group):
            raise ValueError('execution tuning selected a different entry')
        result = check_tuning_values([fields for _, fields in group])
        next_group = stages[offset + 5][0] if offset + 5 < len(stages) else float('inf')
        # No program-counter ownership is inferred from the I/O callback.
        writes = [(index, fields) for index, name, fields in events
                  if name == 'tuning_write' and group[-1][0] < index < next_group
                  and fields.get('selected1867') == '006d']
        if (len(writes) < 2 or writes[0][1].get('port') != '0031'
                or writes[1][1].get('port') != '0032'
                or any(not {'value', 't'} <= fields.keys() for _, fields in writes[:2])
                or int(writes[0][1]['value'], 16) != result & 0xffff
                or int(writes[1][1]['value'], 16) != result >> 16
                or not upload_times[-1] <= float(writes[0][1]['t']) <= float(writes[1][1]['t'])):
            raise ValueError('execution-owned packing differs from subsequent port pair')
    return result


def check_tuning_upload(text):
    writes = [dict(FIELDS.findall(match[1])) for match in
              re.finditer(r"rom4_rf_tuning_field_write: (.*)", text)]
    expected = (("216a", "0004"), ("216b", "13a6"), ("216c", "13a6"))
    uploaded = [write for write in writes if write.get("space") == "program"]
    if len(uploaded) != 3 or tuple((write.get("address"), write.get("value"))
                                  for write in uploaded) != expected:
        raise ValueError("missing or changed native tuning overlay upload")
    if any(write.get("pc") != "31d7" for write in uploaded):
        raise ValueError("tuning upload did not use the observed native copy loop")
    times = [float(write["t"]) for write in uploaded]
    stage = re.search(r"rom4_rf_tuning_stage: (.*)", text)
    if stage is None or times != sorted(times) or times[-1] >= float(dict(FIELDS.findall(stage[1]))["t"]):
        raise ValueError("tuning fields were not uploaded before programming")
    return check_tuning_stages(text)


def check_tuning_blob(text):
    reads = [dict(FIELDS.findall(match[1])) for match in
             re.finditer(r"rom4_rf_tuning_blob_read: (.*)", text)]
    header = [read for read in reads if read.get("pc") == "00272122"]
    expected = (("00299894", "216a0000", "ffff0000"),
                ("00299894", "00001e80", "0000ffff"),
                ("00299898", "011c0000", "ffff0000"),
                ("00299898", "00000130", "0000ffff"),
                ("0029989c", "00780000", "ffff0000"))
    if tuple((read.get("address"), read.get("value"), read.get("mask"))
             for read in header) != expected:
        raise ValueError("selected tuning descriptor differs from native capture")
    payload = [read for read in reads if read.get("pc") == "002936f0"]
    if tuple((read.get("address"), read.get("value"), read.get("mask"))
             for read in payload) != (("002998a0", "000413a6", "ffffffff"),
                                     ("002998a4", "13a62a04", "ffffffff")):
        raise ValueError("missing selected tuning payload source reads")
    ordered = header + payload
    if any(read.get("blob") != "00299894" for read in ordered):
        raise ValueError("tuning reads refer to different blobs")
    times = [float(read["t"]) for read in ordered]
    upload = re.search(r"rom4_rf_tuning_field_write: space=program (.*)", text)
    if upload is None or times != sorted(times) or times[-1] >= float(dict(FIELDS.findall(upload[1]))["t"]):
        raise ValueError("selected blob was not read before native upload")
    return 0x299894


def check(text, consumer_case=False, consumer_effects=False, consumer_decision=False, *, execution=False):
    check_observer_health(text)
    if execution and text.count('rom4_rf_publication_probe: points=37ce,3807 phase=instruction_entry') != 1:
        raise ValueError('missing or duplicate execution-owned publication probe')
    events = []
    for line in text.splitlines():
        match = EVENT.search(line)
        if match:
            fields = dict(FIELDS.findall(match[2]))
            if "t" in fields or (execution and match[1] in ('execution_publication', 'execution_publish_exit')):
                events.append((match[1], fields))

    def after(start, kind, predicate):
        for index in range(start, len(events)):
            name, fields = events[index]
            if name == kind and predicate(fields):
                return index, fields
        raise ValueError(f"missing ordered {kind} observation")

    def matches(**expected):
        return lambda fields: all(fields.get(key) == value for key, value in expected.items())

    index, publication = after(0, "execution_publication" if execution else "native_publication", matches(type="008a", bytes="2"))
    if not re.fullmatch(r'[0-9a-f]{4}', publication.get('words', '')):
        raise ValueError('native two-byte response body is missing or malformed')
    index, queued = after(index + 1, "execution_publish_exit" if execution else "native_publish_exit", matches(address="3807", type="008a", bytes="2"))
    if any(not re.fullmatch(r'[0-9a-f]{4}', queued.get(key, '')) for key in ('producer', 'consumer')):
        raise ValueError('missing or malformed receive-ring cursors')
    producer = int(queued["producer"], 16)
    consumer = int(queued["consumer"], 16)
    if not 0x80 <= producer < 0xe4 or not 0x80 <= consumer < 0xe4:
        raise ValueError("invalid receive-ring cursors")
    next_word = lambda cursor: 0x80 if cursor == 0xe3 else cursor + 1
    if next_word(next_word(consumer)) != producer:
        raise ValueError("response is not the next two-word ring packet")
    times = [] if execution else [float(publication["t"]), float(queued["t"])]
    for cursor in (next_word(consumer), producer):
        index, consumed = after(index + 1, "mcu_ring_write",
            matches(address="000101c8", value=f"{cursor:08x}", mask="0000ffff"))
        times.append(float(consumed["t"]))
    index, header = after(index + 1, "status_header_write",
        matches(pc="0026be4a", value="13900000", mask="ffff0000"))
    object_address = header["address"]
    times.append(float(header["t"]))
    index, posted = after(index + 1, "post_fetch",
        matches(address="00254c84", pc="00254c84", r0="0000000a",
                r1=object_address, lr="0026be5d"))
    times.append(float(posted["t"]))
    index, received = after(index + 1, "task10_lifecycle",
        matches(pc="00211a9a", r0=object_address, status="1390"))
    times.append(float(received["t"]))
    if consumer_case or consumer_effects or consumer_decision:
        index, consumed = after(index + 1, "task10_consumer",
            matches(pc="00211eb0", object=object_address, status="1390"))
        if not re.fullmatch(r'[0-9a-f]{8}', consumed.get('control', '')):
            raise ValueError('missing controller observation at task-10 case')
        times.append(float(consumed['t']))
        if consumer_effects or consumer_decision:
            if int(consumed['control'], 16) & (1 << 25):
                raise ValueError('consumer bit-25 gate rejects the observed effects path')
            index, counter = after(index + 1, 'task10_counter',
                matches(pc='00211ec4', address='0010fee0', mask='ffff0000'))
            if not re.fullmatch(r'[0-9a-f]{4}0000', counter.get('value', '')):
                raise ValueError('counter store is not an isolated upper halfword')
            times.append(float(counter['t']))
            index, helper = after(index + 1, 'task10_helper',
                matches(pc='00210f5c', object=object_address, status='1390', caller='00211ed3'))
            times.append(float(helper['t']))
            if consumer_decision:
                decisions = []
                for pc in ('002114fc', '00211508', '002115a2'):
                    index, decision = after(index + 1, 'task10_decision',
                        matches(pc=pc, object=object_address, status='1390'))
                    decisions.append(decision)
                    times.append(float(decision['t']))
                before, compared, returned = decisions
                count = int(compared['count'], 16)
                limit = int(compared['r0'], 16)
                if (count != ((int(before['count'], 16) + 1) & 0xffff) or
                        not 0 <= limit <= 0xffff or count <= limit or
                        int(compared['flags'], 16) & 0x10 or
                        returned['count'] != compared['count'] or
                        returned['flags'] != compared['flags']):
                    raise ValueError('status-1390 count/limit/flag return path differs')
    if any(later < earlier for earlier, later in zip(times, times[1:])):
        raise ValueError("delivery timestamps regress")
    return object_address, publication["words"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument('--console', type=Path,
                        help='observer console; otherwise check sibling console.log when present')
    parser.add_argument("--search-list", action="store_true", help="also validate the first single-entry request bitmap")
    parser.add_argument('--consumer-case', action='store_true',
                        help='require correlated downstream task-10 case entry')
    parser.add_argument('--consumer-effects', action='store_true',
                        help='also require ordered counter store and correlated helper entry')
    parser.add_argument('--consumer-decision', action='store_true',
                        help='require observed status-1390 count/limit/flag return path')
    args = parser.parse_args()
    try:
        text = args.log.read_text()
        console = args.console or args.log.with_name('console.log')
        if args.console or console.is_file():
            check_observer_health(console.read_text())
        object_address, body = check(text, consumer_case=args.consumer_case,
                                     consumer_effects=args.consumer_effects,
                                     consumer_decision=args.consumer_decision)
        if args.search_list and check_request(text) != int(body, 16):
            raise ValueError("native response body differs from requested entry")
    except (OSError, ValueError, KeyError) as error:
        print(f"ROM4 RF delivery: FAIL: {error}", file=sys.stderr)
        return 1
    print(f"ROM4 RF delivery: PASS object={object_address} native_body={body} acquisition_claim=0")
    return 0


if __name__ == "__main__":
    sys.exit(main())

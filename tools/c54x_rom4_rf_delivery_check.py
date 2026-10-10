#!/usr/bin/env python3
"""Check the observed native ROM4 scan-response delivery, not RF acquisition."""
import argparse
from pathlib import Path
import re
import sys


EVENT = re.compile(r"rom4_rf_(\w+): (.*)")
FIELDS = re.compile(r"(\w+)=([^ ]+)")


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
            or int(returned["field60"], 16) != value // 64
            or check_tuning(text) != result):
        raise ValueError("packing result differs from observed quotient or port pair")
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


def check(text):
    events = []
    for line in text.splitlines():
        match = EVENT.search(line)
        if match:
            fields = dict(FIELDS.findall(match[2]))
            if "t" in fields:
                events.append((match[1], fields))

    def after(start, kind, predicate):
        for index in range(start, len(events)):
            name, fields = events[index]
            if name == kind and predicate(fields):
                return index, fields
        raise ValueError(f"missing ordered {kind} observation")

    def matches(**expected):
        return lambda fields: all(fields.get(key) == value for key, value in expected.items())

    index, publication = after(0, "native_publication", matches(type="008a", bytes="2"))
    index, queued = after(index + 1, "native_publish_exit", matches(address="3807", type="008a", bytes="2"))
    producer = int(queued["producer"], 16)
    consumer = int(queued["consumer"], 16)
    if not 0x80 <= producer < 0xe4 or not 0x80 <= consumer < 0xe4:
        raise ValueError("invalid receive-ring cursors")
    next_word = lambda cursor: 0x80 if cursor == 0xe3 else cursor + 1
    if next_word(next_word(consumer)) != producer:
        raise ValueError("response is not the next two-word ring packet")
    times = [float(publication["t"]), float(queued["t"])]
    for cursor in (next_word(consumer), producer):
        index, consumed = after(index + 1, "mcu_ring_write",
            matches(address="000101c8", value=f"{cursor:08x}", mask="0000ffff"))
        times.append(float(consumed["t"]))
    index, header = after(index + 1, "status_header_write",
        matches(pc="0026be48", value="13900000", mask="ffff0000"))
    object_address = header["address"]
    times.append(float(header["t"]))
    index, posted = after(index + 1, "post_fetch",
        matches(address="00254c84", pc="00254c84", r0="0000000a",
                r1=object_address, lr="0026be5d"))
    times.append(float(posted["t"]))
    _, received = after(index + 1, "task10_lifecycle",
        matches(pc="00211a9a", r0=object_address, status="1390"))
    times.append(float(received["t"]))
    if any(later < earlier for earlier, later in zip(times, times[1:])):
        raise ValueError("delivery timestamps regress")
    return object_address, publication["words"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--search-list", action="store_true", help="also validate the first single-entry request bitmap")
    args = parser.parse_args()
    try:
        text = args.log.read_text()
        object_address, body = check(text)
        if args.search_list and check_request(text) != int(body, 16):
            raise ValueError("native response body differs from requested entry")
    except (OSError, ValueError, KeyError) as error:
        print(f"ROM4 RF delivery: FAIL: {error}", file=sys.stderr)
        return 1
    print(f"ROM4 RF delivery: PASS object={object_address} native_body={body} acquisition_claim=0")
    return 0


if __name__ == "__main__":
    sys.exit(main())

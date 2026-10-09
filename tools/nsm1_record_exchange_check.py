#!/usr/bin/env python3
"""Compare captured NSM-1 compatibility replies with the recovered ROM4 codec.

This checks execution against independent arithmetic, not fitted-mask identity,
EEPROM validity, graphical boot or permission to synthesize a DSP response.
"""

import argparse
import json
from pathlib import Path
import re
import struct
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.dct3_msid_codec import decode_msid
from tools.make_5110_eeprom_profile import ENCOD, LOCK_ENC_82
from tools.nse5_transform_trace_check import inverse_transform_words


REQUEST = re.compile(r"nsm1_record_request: .*?bytes=([0-9a-f]+)\b")
REPLY = re.compile(r"nsm1_restart_packet: .*?bytes=([0-9a-f]+)\b")
VALIDATION = re.compile(r"nsm1_record_validation: pc=([0-9a-f]+) r0=([0-9a-f]+) "
                        r"r1=[0-9a-f]+ r5=([0-9a-f]+) object=([0-9a-f]+)\b")


def check_validation(text: str, records: list[dict]) -> dict:
    steps = [tuple(int(value, 16) for value in match.groups())
             for match in VALIDATION.finditer(text)]
    if len(steps) != sum("nsm1_record_validation:" in line for line in text.splitlines()):
        raise ValueError("unrecognized record-validation trace format")
    if not steps:
        return {"observed": False}
    path = (0x27E688, 0x27E6B0, 0x27E6EA, 0x27E796)
    if len(steps) != len(records) * len(path):
        raise ValueError("incomplete or changed record-validation path")
    for index, record in enumerate(records):
        group = steps[index * 4:(index + 1) * 4]
        decoded = bytes.fromhex(record["decoded"])
        if tuple(step[0] for step in group) != path or len({step[3] for step in group}) != 1:
            raise ValueError("changed record-validation path/object")
        if any(step[2] != 1 for step in group):
            raise ValueError("record validity changed before the classified rejection")
        if (group[0][1], group[1][1], group[2][1], group[3][1]) != (
                0x32, decoded[9], decoded[9], decoded[21]):
            raise ValueError("record-validation inputs disagree with decoded response")
        if 0x78 <= decoded[21] < 0x80:
            raise ValueError("classified rejecting byte is inside the accepted range")
    return {"observed": True, "rejections": len(records), "path": [hex(pc) for pc in path],
            "scope": "known incompatible-template rejection, not boot acceptance"}


def retained_record_transform(record: bytes, identity_record: bytes) -> bytes:
    """Reproduce NSM-1 0x27d4ac for a captured request, without provisioning."""
    if len(record) != 24 or len(identity_record) != 12:
        raise ValueError("request/identity record must contain twenty-four/twelve bytes")
    work = bytearray(identity_record * 2)
    for index in range(0, 24, 2):
        product = work[index] * work[index + 1]
        work[index:index + 2] = product.to_bytes(2, "little")
    pad = []
    for value in reversed(work):
        reversed_complement = 0
        for bit in range(8):
            reversed_complement = (reversed_complement << 1) | (1 ^ ((value >> bit) & 1))
        pad.append(reversed_complement)
    return bytes(value ^ mask for value, mask in zip(record, pad))


def decode_record(encoded: bytes, chip: bytes) -> bytes:
    if len(encoded) != 12 or len(chip) != 4:
        raise ValueError("record/chip must contain twelve/four bytes")
    table = bytes(value ^ (chip[index] if index < 4 else 0)
                  for index, value in enumerate(LOCK_ENC_82))
    words = inverse_transform_words(
        struct.unpack(">6H", encoded), struct.unpack(">6H", table),
        tuple(value * 0x101 for value in ENCOD))
    return struct.pack(">6H", *words)


def check(text: str, *, minimum: int = 2) -> dict:
    if minimum < 1:
        raise ValueError("minimum exchange count must be positive")
    checksum = None
    chip = None
    pending = None
    identities = []
    records = []
    for line in text.splitlines():
        match = REQUEST.search(line)
        if match:
            packet = bytes.fromhex(match[1])
            if len(packet) < 6 or packet[3] != 0x70:
                raise ValueError("invalid captured request envelope")
            command, size = packet[4:6]
            if len(packet) < 6 + size:
                raise ValueError("truncated captured request")
            body = packet[6:6 + size]
            if command == 0x13:
                if size != 4 or pending is not None:
                    raise ValueError("invalid identity request ordering/size")
                checksum, chip = body, None
            elif command == 0x16:
                if size != 24 or chip is None or pending is not None:
                    raise ValueError("record request lacks identity or overlaps a request")
                pending = body
            continue
        match = REPLY.search(line)
        if not match:
            continue
        packet = bytes.fromhex(match[1])
        if len(packet) < 6 or packet[3] != 0x74:
            raise ValueError("invalid captured reply envelope")
        payload = packet[4:]
        if payload[0] == 0x34:
            if len(payload) != 16 or payload[:3] != bytes.fromhex("340e00"):
                raise ValueError("invalid MSID reply size/format")
            if payload[3] != 0x82 or checksum is None or chip is not None:
                raise ValueError("unexpected MSID family/ordering")
            decoded = decode_msid(payload[3:])
            if decoded[:4] != checksum:
                raise ValueError("MSID does not carry the requested flash value")
            chip = decoded[4:8]
            if chip != bytes.fromhex("00160010"):
                raise ValueError("MSID differs from the modeled COBBA serial")
            identities.append(decoded.hex())
        elif payload[0] == 0x35:
            if pending is None or len(payload) != 52 or payload[:4] != bytes.fromhex("35320000"):
                raise ValueError("invalid record reply size/format/ordering")
            decoded = bytearray()
            markers = []
            for start in (0, 12):
                block = bytearray(decode_record(pending[start:start + 12], chip))
                markers.append(block[10:12].hex())
                # The native builder removes its private marker from each block.
                block[10:12] = bytes(2)
                decoded.extend(block)
            if payload[4:28] != decoded or payload[28:] != pending:
                raise ValueError("native record reply differs from the recovered transform/echo")
            records.append({"request": pending.hex(), "decoded": decoded.hex(),
                            "private_markers": markers})
            pending = None
    if pending is not None or len(records) < minimum or len(identities) != len(records):
        raise ValueError("missing or incomplete identity/record exchanges")
    return {"identity_completions": identities, "record_completions": records,
            "validation_trace": check_validation(text, records),
            "scope": "ROM4 arithmetic/transport comparison; no fitted-mask or EEPROM acceptance"}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        result = check(args.log.read_text())
    except (OSError, ValueError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

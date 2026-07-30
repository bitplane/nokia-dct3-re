#!/usr/bin/env python3
"""Verify two queued multipart Nokia ringtone SMS transactions organically."""

import pathlib
import re
import sys


RINGTONE_COMMAND = bytes.fromhex(
    "024a3a7d5195cdd08199bdc88111a1a5c985b404009b28caea22822849a41c41"
    "a61c4184104288a08a12690718698418612410550550610590558590a22c491613"
    "6154156156106156184102288a08a12690710698710610410a22822d49081a41c6"
    "1a4184904154154184164156164288b124584d85505584d04d8450")
LONG_RINGTONE = RINGTONE_COMMAND * 2 + b"\x00"

SMS_NVRAM_OFFSET = 50 * 32 + 11 + 9 + 16
FREE_SMS_RECORD_PREFIX = bytes([0x00]) + bytes([0xff]) * 35

INITIAL_CHECKPOINTS = (
    ("registration release", re.compile(
        r"LAPDm Channel Release acknowledged nr=2")),
    ("initial steady PCH", re.compile(
        r"PCH no-identity fill channel=60 fn=")),
)

RECEIVED_FRAME = re.compile(
    r"RX enqueue type=80 payload=34 .*data=[0-9a-f]{20}"
    r"(?P<frame>0f[0-9a-f]{46})")


def expected_part(part_index: int) -> bytes:
    payload = LONG_RINGTONE[part_index * 128:(part_index + 1) * 128]
    user_data = (
        bytes.fromhex("0b0504158100000003")
        + bytes([0x7a, 0x02, part_index + 1])
        + payload
    )
    tpdu = (
        bytes([0x40 if part_index == 0 else 0x44])
        + bytes.fromhex("0781551532f400f562704221000000")
        + bytes([len(user_data)])
        + user_data
    )
    rpdu = (
        bytes([0x01, 0x40 + part_index])
        + bytes.fromhex("0691214365870900")
        + bytes([len(tpdu)])
        + tpdu
    )
    return bytes.fromhex("0901") + bytes([len(rpdu)]) + rpdu


def require(text: str, pattern: str, cursor: int, label: str) -> int:
    match = re.search(pattern, text[cursor:])
    if not match:
        raise ValueError(
            "missing or out-of-order incoming-Smart-Message checkpoint: "
            f"{label}")
    return cursor + match.end()


def verify_part(text: str, cursor: int, part_index: int) -> int:
    label = f"part {part_index + 1}"
    cursor = require(
        text, r"PCH IMSI page transmitted channel=60 fn=", cursor,
        f"{label} page")
    cursor = require(
        text, r"TX packet type=1b .*data=0080013f410627", cursor,
        f"{label} Paging Response")
    cursor = require(
        text, r"RX enqueue type=80 payload=34 .*data=[0-9a-f]{20}0f3f01",
        cursor, f"{label} SAPI 3 SABM")
    cursor = require(
        text, r"TX packet type=1b .*data=00800f7301", cursor,
        f"{label} SAPI 3 UA")

    expected = expected_part(part_index)
    information = bytearray()
    send_sequence = 0
    segment_count = (len(expected) + 19) // 20
    for segment_number in range(1, segment_count + 1):
        match = RECEIVED_FRAME.search(text, cursor)
        if not match:
            raise ValueError(f"missing {label} LAPDm segment {segment_number}")
        frame = bytes.fromhex(match.group("frame"))
        control = frame[1]
        length_octet = frame[2]
        length = length_octet >> 2
        more = bool(length_octet & 0x02)
        if ((control >> 1) & 7) != send_sequence:
            raise ValueError(
                f"{label} segment {segment_number} has unexpected N(S)")
        expected_length = min(20, len(expected) - len(information))
        if length != expected_length or more != (
                segment_number < segment_count):
            raise ValueError(
                f"{label} segment {segment_number} has invalid length/M bit")
        information.extend(frame[3:3 + length])
        cursor = match.end()

        receive_sequence = (send_sequence + 1) & 7
        acknowledgement = re.search(
            rf"TX packet type=1b .*data=00800f"
            rf"{(receive_sequence << 5) | 1:02x}01", text[cursor:])
        next_segment = RECEIVED_FRAME.search(text, cursor)
        if not acknowledgement:
            raise ValueError(
                f"{label} segment {segment_number} was not acknowledged")
        acknowledgement_start = cursor + acknowledgement.start()
        if next_segment and next_segment.start() < acknowledgement_start:
            raise ValueError(
                f"{label} segment {segment_number} was not stop-and-wait "
                "acknowledged")
        cursor += acknowledgement.end()
        send_sequence = receive_sequence

    if bytes(information) != expected:
        raise ValueError(
            f"reassembled multipart ringtone {label} does not match the "
            "port/concatenation/RTPL fixture")

    reference = 0x40 + part_index
    cursor = require(
        text,
        r"GSM service uplink sapi=3 pd=09 message=04 length=2 data=8904",
        cursor, f"{label} handset CP-ACK")
    cursor = require(
        text,
        rf"GSM service uplink sapi=3 pd=09 message=01 length=5 "
        rf"data=89010202{reference:02x}",
        cursor, f"{label} handset RP-ACK")
    cursor = require(
        text,
        r"GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2",
        cursor, f"{label} network CP-ACK")
    cursor = require(
        text, r"LAPDm service Channel Release acknowledged nr=", cursor,
        f"{label} RR release")
    return cursor


def verify(text: str, sim_nvram: bytes) -> None:
    cursor = 0
    for label, pattern in INITIAL_CHECKPOINTS:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(
                "missing or out-of-order incoming-Smart-Message checkpoint: "
                f"{label}")
        cursor = match.end()

    cursor = verify_part(text, cursor, 0)
    cursor = verify_part(text, cursor, 1)
    require(
        text, r"PCH no-identity fill channel=60 fn=", cursor,
        "steady PCH after part 2")

    if len(LONG_RINGTONE) != 251 or LONG_RINGTONE[-1] != 0:
        raise ValueError("internal long-ringtone fixture is malformed")

    pages = re.findall(r"PCH IMSI page transmitted channel=60", text)
    if len(pages) != 2:
        raise ValueError(f"expected exactly two IMSI pages, observed {len(pages)}")

    updates = re.findall(r"sim_device: update fid=6f3c", text)
    if updates:
        raise ValueError(
            "port-addressed ringtone was incorrectly filed as an EF_SMS text "
            "record")

    stored = sim_nvram[
        SMS_NVRAM_OFFSET:SMS_NVRAM_OFFSET + len(FREE_SMS_RECORD_PREFIX)]
    if stored != FREE_SMS_RECORD_PREFIX:
        raise ValueError(
            "EF_SMS record 1 changed during port-addressed ringtone delivery")


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit(
            "usage: radio_incoming_smart_message_trace_check.py "
            "MAME_ERROR_LOG SIM_CARD_NVRAM")
    try:
        verify(
            pathlib.Path(sys.argv[1]).read_text(),
            pathlib.Path(sys.argv[2]).read_bytes())
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(
        "OK - both parts of a 251-byte port-0x1581 RTPL ringtone completed "
        "independent paging, SAPI-3, CP/RP and RR-release transactions")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

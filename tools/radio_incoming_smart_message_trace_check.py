#!/usr/bin/env python3
"""Verify the organic first part of a queued multipart Nokia ringtone."""

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

PREFIX_CHECKPOINTS = (
    ("registration release", re.compile(
        r"LAPDm Channel Release acknowledged nr=2")),
    ("IMSI page", re.compile(
        r"PCH IMSI page transmitted channel=60 fn=")),
    ("Paging Response", re.compile(
        r"TX packet type=1b .*data=0080013f410627")),
    ("SAPI 3 SABM", re.compile(
        r"RX enqueue type=80 payload=34 .*data=[0-9a-f]{20}0f3f01")),
    ("SAPI 3 UA", re.compile(
        r"TX packet type=1b .*data=00800f7301")),
)

RECEIVED_FRAME = re.compile(
    r"RX enqueue type=80 payload=34 .*data=[0-9a-f]{20}"
    r"(?P<frame>0f[0-9a-f]{46})")


def expected_first_part() -> bytes:
    payload = LONG_RINGTONE[:128]
    user_data = bytes.fromhex("0b05041581000000037a0201") + payload
    tpdu = (
        bytes.fromhex("400781551532f400f562704221000000")
        + bytes([len(user_data)])
        + user_data
    )
    rpdu = (
        bytes.fromhex("01400691214365870900")
        + bytes([len(tpdu)])
        + tpdu
    )
    return bytes.fromhex("0901") + bytes([len(rpdu)]) + rpdu


def verify(text: str, sim_nvram: bytes) -> None:
    cursor = 0
    for label, pattern in PREFIX_CHECKPOINTS:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(
                "missing or out-of-order incoming-Smart-Message checkpoint: "
                f"{label}")
        cursor = match.end()

    information = bytearray()
    send_sequence = 0
    for segment_number in range(1, 10):
        match = RECEIVED_FRAME.search(text, cursor)
        if not match:
            raise ValueError(
                f"missing multipart ringtone LAPDm segment {segment_number}")
        frame = bytes.fromhex(match.group("frame"))
        control = frame[1]
        length_octet = frame[2]
        length = length_octet >> 2
        more = bool(length_octet & 0x02)
        if ((control >> 1) & 7) != send_sequence:
            raise ValueError(
                f"segment {segment_number} has unexpected N(S)")
        expected_length = 20 if segment_number < 9 else 11
        if length != expected_length or more != (segment_number < 9):
            raise ValueError(
                f"segment {segment_number} has invalid length/M bit")
        information.extend(frame[3:3 + length])
        cursor = match.end()

        receive_sequence = (send_sequence + 1) & 7
        rr = re.compile(
            rf"TX packet type=1b .*data=00800f"
            rf"{(receive_sequence << 5) | 1:02x}01")
        acknowledgement = rr.search(text, cursor)
        next_segment = RECEIVED_FRAME.search(text, cursor)
        if (not acknowledgement or
                (next_segment and next_segment.start() < acknowledgement.start())):
            raise ValueError(
                f"segment {segment_number} was not stop-and-wait acknowledged")
        cursor = acknowledgement.end()
        send_sequence = receive_sequence

    expected = expected_first_part()
    if bytes(information) != expected:
        raise ValueError(
            "reassembled multipart ringtone part 1 does not match the "
            "port/concatenation/RTPL fixture")

    if len(LONG_RINGTONE) != 251 or LONG_RINGTONE[-1] != 0:
        raise ValueError("internal long-ringtone fixture is malformed")

    pages = re.findall(r"PCH IMSI page transmitted channel=60", text)
    if len(pages) != 1:
        raise ValueError(f"expected exactly one IMSI page, observed {len(pages)}")

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
        "OK - part 1/2 of a 251-byte port-0x1581 RTPL ringtone crossed "
        "SAPI 3 in nine stop-and-wait segments; CP/RP close remains the "
        "organic gate for part 2")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

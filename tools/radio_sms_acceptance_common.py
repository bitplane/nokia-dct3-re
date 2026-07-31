#!/usr/bin/env python3
"""Shared evidence primitives for ordinary MT-SMS acceptance checks."""

import hashlib
import pathlib
import re


SMS_NVRAM_OFFSET = 50 * 32 + 11 + 9 + 16
SMS_RECORD_SIZE = 176
SMS_RECORD_COUNT = 10
FIRST_SMS_DELIVER_BODY = bytes.fromhex(
    "06912143658709040781551532f400006270422100000005e8329bfd06")
SECOND_SMS_DELIVER_BODY = bytes.fromhex(
    "06912143658709040781559587f600006270422110000005f7b79c4d06")

TRANSPORT_CLOSURE = (
    ("EF_SMS write", re.compile(
        r"sim_device: update fid=6f3c record=1 length=176")),
    ("handset CP-ACK", re.compile(
        r"GSM service uplink sapi=3 pd=09 message=04 length=2 data=8904")),
    ("handset RP-ACK", re.compile(
        r"GSM service uplink sapi=3 pd=09 message=01 length=5 "
        r"data=8901020240")),
    ("network CP-ACK", re.compile(
        r"GSM service downlink kind=17 sapi=3 pd=09 message=04")),
    ("RR release", re.compile(
        r"LAPDm service Channel Release acknowledged nr=3")),
)


def frame_hashes(snapshot_dir: pathlib.Path) -> set[str]:
    return {
        hashlib.sha256(path.read_bytes()).hexdigest()
        for path in snapshot_dir.glob("nokia_dct3_lcdmirror_*.pgm")
    }


def sms_record(sim_nvram: bytes, number: int = 1) -> bytes:
    if not 1 <= number <= SMS_RECORD_COUNT:
        raise ValueError(f"EF_SMS record number is out of range: {number}")
    start = SMS_NVRAM_OFFSET + (number - 1) * SMS_RECORD_SIZE
    end = start + SMS_RECORD_SIZE
    if len(sim_nvram) < end:
        raise ValueError(f"SIM NVRAM is too short for EF_SMS record {number}")
    return sim_nvram[start:end]


def require_ordered(
        log: str,
        checkpoints: tuple[tuple[str, re.Pattern[str]], ...]) -> None:
    cursor = 0
    for label, pattern in checkpoints:
        match = pattern.search(log, cursor)
        if not match:
            raise ValueError(f"missing or out-of-order SMS lifecycle: {label}")
        cursor = match.end()


def require_single_transport(log: str) -> None:
    require_ordered(log, TRANSPORT_CLOSURE)
    if len(re.findall(r"PCH IMSI page transmitted channel=60", log)) != 1:
        raise ValueError("ordinary SMS did not use exactly one IMSI page")

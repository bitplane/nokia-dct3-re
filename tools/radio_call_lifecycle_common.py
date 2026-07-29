"""Shared semantic trace vocabulary for organic mobile-terminated calls."""

import re
from typing import Iterable, Pattern


PatternLike = str | Pattern[str]

REGISTRATION_RELEASE = re.compile(
    r"LAPDm Channel Release acknowledged nr=2")
IMSI_PAGE = re.compile(r"PCH IMSI page transmitted channel=60 fn=")
PAGING_RESPONSE = re.compile(r"TX packet type=1b .*data=0080013f410627")
PAGING_CONTENTION_UA = re.compile(
    r"RX enqueue type=80 payload=34 .*data=80[0-9a-f]*0173410627")
NO_CIPHER_COMMAND = re.compile(
    r"RX enqueue type=80 payload=34 .*data=80[0-9a-f]{18}03000d063500")
MM_INFORMATION = re.compile(
    r"RX enqueue type=80 payload=34 .*data=80[0-9a-f]{18}"
    r"03[0-9a-f]{2}2905324762704221000000")
CIPHER_MODE_COMPLETE = re.compile(
    r"GSM service uplink sapi=0 pd=06 message=32 length=2")
INCOMING_SETUP = re.compile(
    r"RX enqueue type=80 payload=34 .*data=80[0-9a-f]{18}"
    r"03[0-9a-f]{2}45030504046002008134015c0581551532f4")
ALERTING = re.compile(r"GSM service uplink sapi=0 pd=03 message=01 length=2")
TRAFFIC_SABM = re.compile(r"TX packet type=1b .*data=00b0013f01")
TRAFFIC_UA = re.compile(
    r"RX enqueue type=80 payload=34 .*data=b0[0-9a-f]{18}017301")
ASSIGNMENT_COMPLETE = re.compile(
    r"GSM service uplink sapi=0 pd=06 message=29 length=3")
CONNECT = re.compile(r"GSM service uplink sapi=0 pd=03 message=07 length=2")
CONNECT_ACKNOWLEDGE = re.compile(
    r"RX enqueue type=80 payload=34 .*data=b0[0-9a-f]{18}"
    r"03[0-9a-f]{2}09030f")
DISCONNECT = re.compile(
    r"GSM service uplink sapi=0 pd=03 message=25 length=5")
NETWORK_RELEASE = re.compile(
    r"RX enqueue type=80 payload=34 .*data=b0[0-9a-f]{18}"
    r"03[0-9a-f]{2}09032d")
RELEASE_COMPLETE = re.compile(
    r"GSM service uplink sapi=0 pd=03 message=2a length=2")
RR_CHANNEL_RELEASE = re.compile(
    r"RX enqueue type=80 payload=34 .*data=b0[0-9a-f]{18}"
    r"03[0-9a-f]{2}0d060d00")
TRAFFIC_RELEASE_UA = re.compile(
    r"RX enqueue type=80 payload=34 .*data=b0[0-9a-f]{18}017301")
RELEASE_CONFIRMATION = re.compile(
    r"RX enqueue type=89 payload=8 .*data=0000000000000000")
IDLE_PCH = re.compile(
    r"RX enqueue type=80 payload=34 .*data=60[0-9a-f]{18}"
    r"1506210001f0")


def require_ordered(
        text: str,
        checkpoints: Iterable[tuple[str, PatternLike]],
        product: str,
        cursor: int = 0) -> int:
    for label, pattern in checkpoints:
        compiled = re.compile(pattern) if isinstance(pattern, str) else pattern
        match = compiled.search(text, cursor)
        if not match:
            raise ValueError(
                f"missing or out-of-order {product} call checkpoint: {label}")
        cursor = match.end()
    return cursor


def require_count(
        text: str, pattern: PatternLike, expected: int, message: str) -> None:
    compiled = re.compile(pattern) if isinstance(pattern, str) else pattern
    if len(compiled.findall(text)) != expected:
        raise ValueError(message)

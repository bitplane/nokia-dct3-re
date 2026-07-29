"""Shared extraction and paging-group helpers for GSM radio trace gates."""

import re


LOCATION_UPDATE_IDENTITY_RE = re.compile(
    r"TX packet type=1b [^\n]*data=0080013f[0-9a-f]*"
    r"490508[0-9a-f]{12}3[03]08(?P<identity>[0-9a-f]{16})")
PAGING_REQUEST_RE = re.compile(
    r"RX enqueue type=80 payload=34 [^\n]*data=60[0-9a-f]{18}"
    r"31062110(?P<length>[0-9a-f]{2})(?P<identity>[0-9a-f]{16})")
PAGING_RESPONSE_RE = re.compile(
    r"TX packet type=1b [^\n]*data=0080013f410627"
    r"[0-9a-f]{10}08(?P<identity>[0-9a-f]{16})")

CCCH_BLOCK_OFFSETS = (6, 12, 16, 22, 26, 32, 36, 42, 46)


def registered_mobile_identity(text: str) -> str:
    match = LOCATION_UPDATE_IDENTITY_RE.search(text)
    if not match:
        raise ValueError("missing registered mobile identity")
    return match.group("identity")


def paging_group(identity: str) -> tuple[int, int]:
    if len(identity) != 16:
        raise ValueError("registered mobile identity is not eight octets")
    octets = bytes.fromhex(identity)
    if octets[0] & 0x07 != 0x01:
        raise ValueError("registered mobile identity is not an IMSI")

    digits = [octets[0] >> 4]
    for octet in octets[1:]:
        digits.extend((octet & 0x0f, octet >> 4))
    last_three = digits[12] * 100 + digits[13] * 10 + digits[14]
    group = last_three % (len(CCCH_BLOCK_OFFSETS) * 2)
    return group // len(CCCH_BLOCK_OFFSETS), CCCH_BLOCK_OFFSETS[
        group % len(CCCH_BLOCK_OFFSETS)]

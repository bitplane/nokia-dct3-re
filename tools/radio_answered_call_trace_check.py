#!/usr/bin/env python3
"""Verify an answered MT call and classify its post-answer DSP traffic."""

import pathlib
import re
import sys


CHECKPOINTS = (
    ("TCH/F Assignment Command", re.compile(
        r"RX enqueue type=80 payload=34 .*data=80[0-9a-f]{18}"
        r"03[0-9a-f]{2}21062e094001006301")),
    ("TCH/F channel configuration", re.compile(
        r"TX packet type=02 payload=20 .*"
        r"data=041202860271012fc12b0001002b00042b000000")),
    ("Assignment Complete", re.compile(
        r"GSM service uplink sapi=0 pd=06 message=29 length=3")),
    ("organic CC Connect", re.compile(
        r"GSM service uplink sapi=0 pd=03 message=07 length=2")),
    ("network Connect Acknowledge", re.compile(
        r"RX enqueue type=80 payload=34 .*data=b0[0-9a-f]{18}"
        r"03[0-9a-f]{2}09030f")),
)

TX_RE = re.compile(
    r"TX packet type=(?P<type>[0-9a-f]{2}) payload=(?P<payload>\d+) "
    r".*data=(?P<data>[0-9a-f]+)")
EMPTY_TCH_POLL_RE = re.compile(r"00f0010301(?:2b){20}00$")
TCH_RR_RE = re.compile(r"00b0032101(?:2b){20}$")
EXTERNAL_POLL_RE = re.compile(r"1e020000000c01015f[0-9a-f]{18}$")


def verify(text: str) -> dict[str, int]:
    cursor = 0
    for label, pattern in CHECKPOINTS:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(
                f"missing or out-of-order answered-call checkpoint: {label}")
        cursor = match.end()

    post_answer = list(TX_RE.finditer(text, cursor))
    unexpected_types = sorted({
        match.group("type") for match in post_answer
        if match.group("type") != "1b"
        and not (match.group("type") == "05"
                 and EXTERNAL_POLL_RE.fullmatch(match.group("data")))})
    if unexpected_types:
        raise ValueError(
            "unexpected post-answer DSP TX packet types: "
            + ", ".join(unexpected_types))

    empty_polls = [
        match for match in post_answer
        if EMPTY_TCH_POLL_RE.fullmatch(match.group("data"))]
    if len(empty_polls) < 10:
        raise ValueError(
            f"expected a stable answered interval, observed only "
            f"{len(empty_polls)} empty TCH polls")

    unexplained = [
        match.group("data") for match in post_answer
        if match.group("type") == "1b"
        if not EMPTY_TCH_POLL_RE.fullmatch(match.group("data"))
        and not TCH_RR_RE.fullmatch(match.group("data"))]
    if unexplained:
        raise ValueError(
            "unclassified post-answer type-1b payload: " + unexplained[0])

    external_polls = sum(
        match.group("type") == "05"
        and EXTERNAL_POLL_RE.fullmatch(match.group("data")) is not None
        for match in post_answer)
    return {
        "post_answer_tx_packets": len(post_answer),
        "empty_tch_polls": len(empty_polls),
        "external_polls": external_polls,
        "speech_or_codec_packets": 0,
    }


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit(
            "usage: radio_answered_call_trace_check.py MAME_ERROR_LOG")
    try:
        counts = verify(pathlib.Path(sys.argv[1]).read_text(errors="replace"))
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(
        "OK - physical Answer produced Connect/Connect Ack; post-answer DSP "
        f"TX contained {counts['empty_tch_polls']} empty TCH polls, "
        f"{counts['external_polls']} known external-service poll(s), and no "
        "speech/codec packet family")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

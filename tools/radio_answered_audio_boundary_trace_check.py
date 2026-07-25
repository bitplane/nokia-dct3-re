#!/usr/bin/env python3
"""Verify the lower DSP shared-control burst around physical call Answer."""

import pathlib
import re
import sys


CONNECT_RE = re.compile(
    r"GSM service uplink sapi=0 pd=03 message=07 length=2 t=(?P<time>[0-9.]+)")
CONNECT_ACK_RE = re.compile(
    r"RX enqueue type=80 payload=34 .*data=b0[0-9a-f]{18}"
    r"03[0-9a-f]{2}09030f.*t=(?P<time>[0-9.]+)")
CONTROL_RE = re.compile(
    r"dsp_shared_control: command=(?P<command>[0-9a-f]{2}) "
    r"value=(?P<value>[0-9a-f]{4}) commit=(?P<commit>[01]) "
    r"caller=(?P<caller>[0-9a-f]{8}) task=(?P<task>[0-9a-f]{2}) "
    r"t=(?P<time>[0-9.]+)")
WRITE_RE = re.compile(
    r"dsp_shared_write: off=(?P<offset>[0-9a-f]{3}) "
    r"old=[0-9a-f]{4} data=(?P<data>[0-9a-f]{4}) "
    r"pc=[0-9a-f]{8} t=(?P<time>[0-9.]+)")

CONTROL_SEQUENCE = (
    ("answer-side lower control", "08", "060b", "1", "05"),
    ("audio shadow update 1", "09", "08af", "1", None),
    ("audio shadow update 2", "09", "09a0", "1", None),
    ("tone table arm", "26", "ffff", "0", None),
    ("900 Hz oscillator start", "21", "0e10", "0", None),
    ("answer-tone amplitude", "25", "0041", "0", None),
    ("tone route start", "29", "fff7", "0", None),
    ("start commit", "2f", "0000", "1", None),
    ("tone table disarm", "1c", "ffff", "0", None),
    ("tone table clear", "26", "0000", "0", None),
    ("oscillator stop", "21", "0000", "0", None),
    ("tone route stop", "29", "fff6", "0", None),
    ("stop commit", "2f", "0000", "1", None),
)

SHARED_WRITE_SEQUENCE = (
    ("answer control word", "0a8", "860b"),
    ("oscillator word", "0ae", "0e10"),
    ("amplitude word", "0b6", "0041"),
    ("oscillator clear", "0ae", "0000"),
)


def _ordered(events, expected, start: int, fields: tuple[str, ...]):
    cursor = start
    found = []
    for item in expected:
        label, *values = item
        for index in range(cursor, len(events)):
            if all(values[field_index] is None
                   or events[index][field] == values[field_index]
                   for field_index, field in enumerate(fields)):
                found.append(events[index])
                cursor = index + 1
                break
        else:
            raise ValueError(
                f"missing or out-of-order answered audio boundary: {label}")
    return found, cursor


def verify(text: str) -> dict[str, float | int]:
    connect = CONNECT_RE.search(text)
    if not connect:
        raise ValueError("missing organic CC Connect")
    connect_time = float(connect.group("time"))

    connect_ack = CONNECT_ACK_RE.search(text, connect.end())
    if not connect_ack:
        raise ValueError("missing network Connect Acknowledge")

    controls = [
        {
            **match.groupdict(),
            "time": float(match.group("time")),
            "position": match.start(),
        }
        for match in CONTROL_RE.finditer(text)
    ]
    control_start = next(
        (index for index, event in enumerate(controls)
         if event["position"] > connect.start()),
        len(controls),
    )
    matched_controls, control_end = _ordered(
        controls, CONTROL_SEQUENCE, control_start,
        ("command", "value", "commit", "task"))

    first_control = matched_controls[0]
    if not connect_time <= first_control["time"] <= float(
            connect_ack.group("time")):
        raise ValueError(
            "answer-side command 08 did not fall between Connect and Connect Ack")

    tone_start = matched_controls[4]["time"]
    tone_stop = matched_controls[10]["time"]
    tone_duration = tone_stop - tone_start
    if not 0.10 <= tone_duration <= 0.15:
        raise ValueError(
            f"answer-tone duration outside recovered bound: "
            f"{tone_duration * 1000:.3f} ms")

    trailing = controls[control_end:]
    if trailing:
        raise ValueError(
            "unexpected continuing shared-control traffic after answer-tone stop: "
            f"command {trailing[0]['command']}")

    writes = [
        {
            **match.groupdict(),
            "time": float(match.group("time")),
            "position": match.start(),
        }
        for match in WRITE_RE.finditer(text)
    ]
    write_start = next(
        (index for index, event in enumerate(writes)
         if event["position"] > connect.start()),
        len(writes),
    )
    _ordered(
        writes, SHARED_WRITE_SEQUENCE, write_start, ("offset", "data"))

    return {
        "shared_control_commands": len(matched_controls),
        "tone_frequency_hz": int("0e10", 16) >> 2,
        "tone_duration_ms": tone_duration * 1000,
        "trailing_shared_control_commands": len(trailing),
    }


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit(
            "usage: radio_answered_audio_boundary_trace_check.py "
            "MAME_ERROR_LOG")
    try:
        result = verify(pathlib.Path(sys.argv[1]).read_text(errors="replace"))
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(
        "OK - physical Answer exposed lower DSP command 08/060b and a "
        f"{result['tone_frequency_hz']} Hz, "
        f"{result['tone_duration_ms']:.1f} ms acknowledgement-tone burst; "
        "no continuing MCU shared-control traffic followed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

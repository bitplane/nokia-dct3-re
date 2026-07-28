"""Shared deterministic radio save/load replay verification."""

import re


ROUNDTRIP_RE = re.compile(
    r"state_roundtrip: result=(\w+) .*?requested_at=([0-9.]+) t=([0-9.]+)")
MARKER_RE = re.compile(
    r"state_replay: phase=(reference|restored) event=(begin|end) t=([0-9.]+)")


def _records(
        lines: list[str], begin: int, end: int,
        tokens: tuple[str, ...]) -> list[str]:
    result = []
    for line in lines[begin + 1:end]:
        for token in tokens:
            if token in line:
                result.append(line[line.index(token):])
                break
    return result


def verify_roundtrip(
        text: str, tokens: tuple[str, ...], lifecycle: str) -> None:
    roundtrip = ROUNDTRIP_RE.search(text)
    if not roundtrip or roundtrip.group(1) != "pass":
        raise ValueError("missing successful emulator save/load round trip")
    requested = float(roundtrip.group(2))
    restored = float(roundtrip.group(3))
    if restored < requested - 0.000_001 or restored > requested + 0.012:
        raise ValueError(
            f"restore did not resume the saved timeline: {requested} -> {restored}")

    lines = text.splitlines()
    markers = [
        (match.group(1), match.group(2), index)
        for index, line in enumerate(lines)
        if (match := MARKER_RE.search(line))
    ]
    if [(phase, event) for phase, event, _ in markers] != [
            ("reference", "begin"), ("reference", "end"),
            ("restored", "begin"), ("restored", "end")]:
        raise ValueError("missing or misordered deterministic replay markers")

    reference = _records(lines, markers[0][2], markers[1][2], tokens)
    replayed = _records(lines, markers[2][2], markers[3][2], tokens)
    if not reference:
        raise ValueError(f"{lifecycle} replay interval contained no radio records")
    if replayed != reference:
        raise ValueError(
            f"restored {lifecycle} trace diverged from the reference interval")

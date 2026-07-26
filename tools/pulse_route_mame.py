#!/usr/bin/env python3
"""Keep MAME's PulseAudio streams on isolated acceptance-test endpoints."""

import argparse
import json
import subprocess
import time


def is_mame_stream(stream: dict) -> bool:
    properties = stream.get("properties", {})
    binary = properties.get("application.process.binary", "")
    application = properties.get("application.name", "")
    return binary == "mame" or application.lower() == "mame"


def list_streams(kind: str) -> list[dict]:
    result = subprocess.run(
        ["pactl", "--format=json", "list", kind],
        check=True,
        capture_output=True,
        text=True,
    )
    return json.loads(result.stdout)


def move_stream(kind: str, index: int, target: str) -> None:
    command = (
        "move-source-output" if kind == "source-outputs"
        else "move-sink-input"
    )
    subprocess.run(["pactl", command, str(index), target], check=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True)
    parser.add_argument("--sink", required=True)
    args = parser.parse_args()
    routed: set[tuple[str, int]] = set()

    while True:
        for kind, target, label in (
            ("source-outputs", args.source, "source-output"),
            ("sink-inputs", args.sink, "sink-input"),
        ):
            for stream in list_streams(kind):
                index = int(stream["index"])
                key = (kind, index)
                if key in routed or not is_mame_stream(stream):
                    continue
                move_stream(kind, index, target)
                routed.add(key)
                print(
                    f"pulse_route: {label} {index} -> {target}",
                    flush=True,
                )
        time.sleep(0.1)


if __name__ == "__main__":
    raise SystemExit(main())

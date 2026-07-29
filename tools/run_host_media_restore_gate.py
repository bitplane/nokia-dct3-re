#!/usr/bin/env python3
"""Echo GSM-FR across save/load using the republished media cursors."""

import argparse
import asyncio
import json
import pathlib
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from tools.run_host_call_adapter_gate import connect


async def run(args):
    process = await asyncio.create_subprocess_exec(*args.command, cwd=args.cwd)
    try:
        websocket = await connect(args.port, process)
        async with websocket:
            initial_epoch = None
            current_epoch = None
            downlink_sequence = None
            restored_frames = 0
            terminated = False
            last_time_us = 0
            while not terminated:
                message = json.loads(
                    await asyncio.wait_for(websocket.recv(), 45)
                )
                kind = message.get("type")
                if kind == "outgoing_call":
                    if initial_epoch is None:
                        initial_epoch = message["epoch"]
                        current_epoch = initial_epoch
                        await websocket.send(json.dumps({
                            "type": "outgoing_call_decision",
                            "epoch": initial_epoch,
                            "request_id": message["request_id"],
                            "decision": "connect",
                        }))
                    continue
                if kind == "outgoing_call_state":
                    if message.get("phase") != "connected":
                        continue
                    previous_epoch = current_epoch
                    current_epoch = message["epoch"]
                    if current_epoch != previous_epoch:
                        last_time_us = 0
                    downlink_sequence = message.get(
                        "media_downlink_sequence"
                    )
                    if downlink_sequence is None:
                        raise RuntimeError(
                            "connected snapshot omitted media cursor"
                        )
                    if current_epoch != initial_epoch:
                        await websocket.send(json.dumps({
                            "type": "outgoing_call_media_downlink",
                            "epoch": initial_epoch,
                            "request_id": 1,
                            "sequence": downlink_sequence,
                            "source_time_us": 0,
                            "frame": "00" * 33,
                        }))
                    continue
                if kind != "outgoing_call_media_uplink":
                    continue
                if downlink_sequence is None:
                    raise RuntimeError("media preceded connected snapshot")
                if message.get("epoch") != current_epoch:
                    continue
                frame = message.get("frame")
                time_us = message.get("emulation_time_us")
                if (
                    message.get("good")
                    and isinstance(frame, str)
                    and len(frame) == 66
                ):
                    if not isinstance(time_us, int) or time_us < last_time_us:
                        raise RuntimeError(
                            "uplink media timestamp was absent or regressed"
                        )
                    await websocket.send(json.dumps({
                        "type": "outgoing_call_media_downlink",
                        "epoch": current_epoch,
                        "request_id": 1,
                        "sequence": downlink_sequence,
                        "source_time_us": time_us,
                        "frame": frame,
                    }))
                    last_time_us = time_us
                    downlink_sequence += 1
                    if current_epoch != initial_epoch:
                        restored_frames += 1
                if restored_frames >= args.restored_frames:
                    await websocket.send(json.dumps({
                        "type": "outgoing_call_terminate",
                        "epoch": current_epoch,
                        "request_id": 1,
                        "cause": 16,
                    }))
                    terminated = True
            await asyncio.sleep(0.5)

        result = await asyncio.wait_for(process.wait(), 90)
        if result:
            raise RuntimeError(f"MAME exited with status {result}")
    finally:
        if process.returncode is None:
            process.terminate()
            try:
                await asyncio.wait_for(process.wait(), 5)
            except asyncio.TimeoutError:
                process.kill()
                await process.wait()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, required=True)
    parser.add_argument("--cwd")
    parser.add_argument("--restored-frames", type=int, default=80)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.command[:1] == ["--"]:
        args.command = args.command[1:]
    if not args.command:
        parser.error("a MAME command is required after --")
    try:
        asyncio.run(run(args))
    except (RuntimeError, asyncio.TimeoutError) as error:
        print(f"FAIL - {error}")
        return 1
    print("OK - media resumed from the restored emulator-owned sequence cursor")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

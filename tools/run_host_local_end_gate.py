#!/usr/bin/env python3
"""Submit stale host events only after firmware has physically ended a call."""

import argparse
import asyncio
import json
import pathlib
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from tools.run_host_call_adapter_gate import connect


async def receive_type(websocket, kind: str, timeout: float = 45):
    while True:
        message = json.loads(await asyncio.wait_for(websocket.recv(), timeout))
        if message.get("type") == kind:
            return message


async def run(args):
    process = await asyncio.create_subprocess_exec(*args.command, cwd=args.cwd)
    try:
        websocket = await connect(args.port, process)
        async with websocket:
            request = await receive_type(websocket, "outgoing_call")
            epoch = request["epoch"]
            await websocket.send(json.dumps({
                "type": "outgoing_call_decision",
                "epoch": epoch,
                "request_id": request["request_id"],
                "decision": "connect",
            }))
            state = await receive_type(websocket, "outgoing_call_state")
            if state.get("phase") != "connected":
                raise RuntimeError(f"unexpected call state {state!r}")

            # The physical input script presses End five seconds after Send.
            # Realtime throttling makes this delay a harness bound only; the
            # acceptance trace still identifies firmware Disconnect/RR release.
            await asyncio.sleep(8)
            stale_identity = {
                "epoch": epoch,
                "request_id": request["request_id"],
            }
            await websocket.send(json.dumps({
                "type": "outgoing_call_terminate",
                **stale_identity,
                "cause": 16,
            }))
            await websocket.send(json.dumps({
                "type": "outgoing_call_media_downlink",
                **stale_identity,
                "sequence": 0,
                "source_time_us": 0,
                "frame": "00" * 33,
            }))
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
    print("OK - stale host events were submitted after physical local End")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

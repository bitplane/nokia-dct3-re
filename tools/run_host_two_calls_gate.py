#!/usr/bin/env python3
"""Drive two sequential host-decided outgoing calls."""

import argparse
import asyncio
import json
import pathlib
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from tools.run_host_call_adapter_gate import connect


async def next_request(websocket):
    while True:
        message = json.loads(await asyncio.wait_for(websocket.recv(), 30))
        if message.get("type") == "outgoing_call":
            return message


async def run(args):
    process = await asyncio.create_subprocess_exec(*args.command, cwd=args.cwd)
    try:
        websocket = await connect(args.port, process)
        async with websocket:
            first = await next_request(websocket)
            await websocket.send(json.dumps({
                "type": "outgoing_call_decision",
                "epoch": first["epoch"],
                "request_id": first["request_id"],
                "decision": "busy",
            }))
            second = await next_request(websocket)
            if second["request_id"] != first["request_id"] + 1:
                raise RuntimeError("second call did not receive a new request ID")
            await websocket.send(json.dumps({
                "type": "outgoing_call_decision",
                "epoch": second["epoch"],
                "request_id": first["request_id"],
                "decision": "connect",
            }))
            await websocket.send(json.dumps({
                "type": "outgoing_call_decision",
                "epoch": second["epoch"],
                "request_id": second["request_id"],
                "decision": "busy",
            }))
            await asyncio.sleep(0.5)
        result = await asyncio.wait_for(process.wait(), 90)
        if result:
            raise RuntimeError(f"MAME exited with status {result}")
    finally:
        if process.returncode is None:
            process.terminate()
            await process.wait()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, required=True)
    parser.add_argument("--cwd")
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.command[:1] == ["--"]:
        args.command = args.command[1:]
    try:
        asyncio.run(run(args))
    except (RuntimeError, asyncio.TimeoutError) as error:
        print(f"FAIL - {error}")
        return 1
    print("OK - two sequential calls used distinct correlated request IDs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

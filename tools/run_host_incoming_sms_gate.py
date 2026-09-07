#!/usr/bin/env python3
"""Run MAME and deliver one external SMS through the host adapter."""

import argparse
import asyncio
import json

try:
    from tools.run_host_call_adapter_gate import connect
except ModuleNotFoundError:
    from run_host_call_adapter_gate import connect


async def run(args: argparse.Namespace) -> None:
    process = await asyncio.create_subprocess_exec(*args.command, cwd=args.cwd)
    try:
        websocket = await connect(args.port, process)
        async with websocket:
            epoch = None
            while epoch is None:
                event = json.loads(await asyncio.wait_for(websocket.recv(), 30))
                if event.get("type") == "call_adapter_ready":
                    epoch = event.get("epoch")
            while True:
                event = json.loads(await asyncio.wait_for(websocket.recv(), 30))
                if (event.get("type") == "network_state" and
                        event.get("epoch") == epoch and
                        event.get("registered") is True):
                    break
            await websocket.send(json.dumps({
                "type": "incoming_sms",
                "epoch": epoch,
                "request_id": 1,
                "sender": "5551234",
                "alphabet": "gsm7",
                "user_data_length": 5,
                "user_data": "e8329bfd06",
            }))
            phases = []
            while "delivered" not in phases:
                event = json.loads(await asyncio.wait_for(websocket.recv(), 30))
                if event.get("type") == "incoming_sms_state":
                    if event.get("request_id") != 1 or event.get("epoch") != epoch:
                        raise RuntimeError(f"uncorrelated SMS state {event!r}")
                    phases.append(event.get("phase"))
            if phases != ["queued", "delivered"]:
                raise RuntimeError(f"unexpected incoming SMS phases {phases!r}")
        result = await asyncio.wait_for(process.wait(), 90)
        if result:
            raise RuntimeError(f"MAME exited with status {result}")
    finally:
        if process.returncode is None:
            process.terminate()
            await process.wait()


def main() -> int:
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
    print("OK - host SMS crossed paging, SAPI 3 and handset storage")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

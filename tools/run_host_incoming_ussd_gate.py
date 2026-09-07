#!/usr/bin/env python3
"""Run MAME and deliver one host-originated USSD notification."""

import argparse
import asyncio
import json

try:
    from tools.run_host_call_adapter_gate import connect
    from tools.run_host_ussd_gate import pack_gsm7
except ModuleNotFoundError:
    from run_host_call_adapter_gate import connect
    from run_host_ussd_gate import pack_gsm7


async def run(args: argparse.Namespace) -> None:
    process = await asyncio.create_subprocess_exec(*args.command, cwd=args.cwd)
    try:
        websocket = await connect(args.port, process)
        async with websocket:
            epoch = None
            registered = False
            while not registered:
                event = json.loads(await asyncio.wait_for(websocket.recv(), 40))
                if event.get("type") == "call_adapter_ready":
                    epoch = event.get("epoch")
                elif (event.get("type") == "network_state" and
                        event.get("epoch") == epoch):
                    registered = event.get("registered") is True
            await websocket.send(json.dumps({
                "type": "incoming_ussd",
                "epoch": epoch,
                "request_id": 1,
                "dcs": 0x0f,
                "data": pack_gsm7("Host notice"),
            }))
            phases = []
            while "delivered" not in phases:
                event = json.loads(await asyncio.wait_for(websocket.recv(), 40))
                if event.get("type") == "incoming_ussd_state":
                    if event.get("epoch") != epoch or event.get("request_id") != 1:
                        raise RuntimeError(f"uncorrelated incoming USSD state {event!r}")
                    phases.append(event.get("phase"))
            if phases != ["queued", "delivered"]:
                raise RuntimeError(f"unexpected incoming USSD phases {phases!r}")
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
    print("OK - host USSD notification crossed paging and firmware acknowledgement")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

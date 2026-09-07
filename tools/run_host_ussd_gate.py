#!/usr/bin/env python3
"""Run MAME and answer one firmware-originated USSD request from the host."""

import argparse
import asyncio
import json

try:
    from tools.run_host_call_adapter_gate import connect
except ModuleNotFoundError:
    from run_host_call_adapter_gate import connect


def pack_gsm7(text: str) -> str:
    accumulator = 0
    bits = 0
    packed = bytearray()
    for value in text.encode("ascii"):
        accumulator |= value << bits
        bits += 7
        while bits >= 8:
            packed.append(accumulator & 0xff)
            accumulator >>= 8
            bits -= 8
    if bits:
        packed.append(accumulator & 0xff)
    return packed.hex()


async def run(args: argparse.Namespace) -> None:
    process = await asyncio.create_subprocess_exec(*args.command, cwd=args.cwd)
    try:
        websocket = await connect(args.port, process)
        async with websocket:
            epoch = None
            accepted = False
            initial_request_epoch = None
            while True:
                event = json.loads(await asyncio.wait_for(websocket.recv(), 45))
                if event.get("type") == "call_adapter_ready":
                    epoch = event.get("epoch")
                elif event.get("type") == "outgoing_ussd":
                    if event.get("epoch") != epoch:
                        raise RuntimeError(f"uncorrelated USSD request {event!r}")
                    if event.get("dcs") != 0x0f or event.get("data") != "aa986c3602":
                        raise RuntimeError(f"unexpected USSD request {event!r}")
                    if initial_request_epoch is None:
                        initial_request_epoch = epoch
                    if (args.require_restore and epoch == initial_request_epoch):
                        continue
                    response = {
                        "type": "outgoing_ussd_response",
                        "epoch": epoch,
                        "request_id": event.get("request_id"),
                        "outcome": "success",
                        "dcs": 0x0f,
                        "data": pack_gsm7("Host network"),
                    }
                    stale = dict(response)
                    stale["request_id"] += 1
                    await websocket.send(json.dumps(stale))
                    await websocket.send(json.dumps(response))
                elif event.get("type") == "outgoing_ussd_state":
                    if event.get("epoch") != epoch:
                        raise RuntimeError(f"uncorrelated USSD state {event!r}")
                    if event.get("phase") == "accepted":
                        accepted = True
                    elif event.get("phase") == "ended":
                        if not accepted:
                            raise RuntimeError("USSD ended before host response acceptance")
                        break
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
    parser.add_argument("--require-restore", action="store_true")
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.command[:1] == ["--"]:
        args.command = args.command[1:]
    try:
        asyncio.run(run(args))
    except (RuntimeError, asyncio.TimeoutError) as error:
        print(f"FAIL - {error}")
        return 1
    print("OK - host answered one correlated firmware-originated USSD request")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

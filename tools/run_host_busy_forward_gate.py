#!/usr/bin/env python3
"""Register CFB organically, connect one call, then divert an incoming call."""

import argparse
import asyncio
import json
import pathlib
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from tools.run_host_call_adapter_gate import connect


REGISTERED = "gsm_ss: request=register transaction=1b invoke=1 service=29"


async def wait_for_registration(path, process):
    for _ in range(8000):
        if path.exists() and REGISTERED in path.read_text(errors="replace"):
            return
        if process.returncode is not None:
            raise RuntimeError("MAME exited before busy forwarding registered")
        await asyncio.sleep(0.005)
    raise RuntimeError("timed out waiting for busy forwarding registration")


async def next_message(websocket, kind, timeout=45):
    while True:
        message = json.loads(await asyncio.wait_for(websocket.recv(), timeout))
        if message.get("type") == kind:
            return message


async def run(args):
    process = await asyncio.create_subprocess_exec(*args.command, cwd=args.cwd)
    try:
        websocket = await connect(args.port, process)
        async with websocket:
            ready = await next_message(websocket, "call_adapter_ready")
            epoch = ready.get("epoch")
            await wait_for_registration(pathlib.Path(args.cwd) / "error.log", process)
            outgoing = await next_message(websocket, "outgoing_call")
            if outgoing.get("epoch") != epoch or outgoing.get("request_id") != 1:
                raise RuntimeError(f"unexpected outgoing request {outgoing!r}")
            await websocket.send(json.dumps({
                "type": "outgoing_call_decision",
                "epoch": epoch,
                "request_id": 1,
                "decision": "connect",
            }))
            while True:
                state = await next_message(websocket, "outgoing_call_state")
                if state.get("phase") == "connected":
                    break
            await websocket.send(json.dumps({
                "type": "incoming_call",
                "epoch": epoch,
                "request_id": 2,
                "caller": args.caller,
            }))
            phases = []
            while phases[-1:] != ["forwarded"]:
                state = await next_message(websocket, "incoming_call_state")
                if state.get("request_id") != 2 or state.get("epoch") != epoch:
                    continue
                phase = state.get("phase")
                if not phases or phase != phases[-1]:
                    phases.append(phase)
                if phase == "forwarded" and (
                        state.get("forwarding_reason") != "busy" or
                        state.get("forwarding_destination") != "5551234"):
                    raise RuntimeError(f"busy forwarding metadata mismatch {state!r}")
            if phases != ["queued", "forwarded"]:
                raise RuntimeError(f"busy call reached handset phases {phases!r}")
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
    parser.add_argument("--cwd", required=True)
    parser.add_argument("--caller", default="447700900123")
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.command[:1] == ["--"]:
        args.command = args.command[1:]
    if not args.command:
        parser.error("a MAME command is required after --")
    try:
        asyncio.run(run(args))
    except (RuntimeError, ValueError, asyncio.TimeoutError) as error:
        print(f"FAIL - {error}")
        return 1
    print("OK - active speech CFB diverted a second call before paging")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

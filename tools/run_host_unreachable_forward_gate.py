#!/usr/bin/env python3
"""Register CFNRc organically, wait for cell loss, then submit a host call."""

import argparse
import asyncio
import json
import pathlib
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from tools.run_host_call_adapter_gate import connect


MARKERS = (
    "gsm_ss: request=register transaction=1b invoke=1 service=2b",
    "DOWNLINK_SIGNALLING_FAIL arfcn=1",
)


async def wait_for_markers(path, process):
    for _ in range(12000):
        text = path.read_text(errors="replace") if path.exists() else ""
        if all(marker in text for marker in MARKERS):
            return
        if process.returncode is not None:
            raise RuntimeError("MAME exited before registration and cell loss")
        await asyncio.sleep(0.005)
    raise RuntimeError("timed out waiting for registration and cell loss")


async def run(args):
    process = await asyncio.create_subprocess_exec(*args.command, cwd=args.cwd)
    try:
        websocket = await connect(args.port, process)
        async with websocket:
            ready = json.loads(await asyncio.wait_for(websocket.recv(), 30))
            epoch = ready.get("epoch")
            await wait_for_markers(pathlib.Path(args.cwd) / "error.log", process)
            await websocket.send(json.dumps({
                "type": "incoming_call",
                "epoch": epoch,
                "request_id": 1,
                "caller": args.caller,
            }))
            phases = []
            while phases[-1:] != ["forwarded"]:
                message = json.loads(await asyncio.wait_for(websocket.recv(), 30))
                if (message.get("type") != "incoming_call_state" or
                        message.get("request_id") != 1 or
                        message.get("epoch") != epoch):
                    continue
                phase = message.get("phase")
                if not phases or phase != phases[-1]:
                    phases.append(phase)
                if phase == "forwarded" and (
                        message.get("forwarding_reason") != "not-reachable" or
                        message.get("forwarding_destination") != "5551234"):
                    raise RuntimeError(
                        f"not-reachable forwarding metadata mismatch {message!r}")
            if phases != ["queued", "forwarded"]:
                raise RuntimeError(f"unreachable call reached handset {phases!r}")
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
    print("OK - active speech CFNRc diverted after organic cell loss")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

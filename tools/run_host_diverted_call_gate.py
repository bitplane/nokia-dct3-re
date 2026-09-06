#!/usr/bin/env python3
"""Register call forwarding organically, then originate a host-side call."""

import argparse
import asyncio
import json
import pathlib
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

REGISTERED = "gsm_ss: request=register transaction=1b invoke=1 service=21"


def validate_phases(phases):
    if phases != ["queued", "forwarded"]:
        raise ValueError(f"unexpected diverted-call phases {phases!r}")


async def wait_for_registration(path, process):
    for _ in range(6000):
        if path.exists() and REGISTERED in path.read_text(errors="replace"):
            return
        if process.returncode is not None:
            raise RuntimeError("MAME exited before call forwarding registered")
        await asyncio.sleep(0.005)
    raise RuntimeError("timed out waiting for organic forwarding registration")


async def run(args):
    from tools.run_host_call_adapter_gate import connect

    process = await asyncio.create_subprocess_exec(*args.command, cwd=args.cwd)
    try:
        websocket = await connect(args.port, process)
        async with websocket:
            ready = json.loads(await asyncio.wait_for(websocket.recv(), 30))
            if ready.get("type") != "call_adapter_ready":
                raise RuntimeError(f"expected adapter ready, got {ready!r}")
            epoch = ready.get("epoch")
            if not isinstance(epoch, int):
                raise RuntimeError("adapter ready omitted transport epoch")
            await wait_for_registration(pathlib.Path(args.cwd) / "error.log", process)
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
                if phase in ("paging", "alerting", "connected", "ended"):
                    raise RuntimeError(f"diverted call reached handset phase {phase}")
            validate_phases(phases)

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
    print("OK - active unconditional forwarding suppressed handset paging")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

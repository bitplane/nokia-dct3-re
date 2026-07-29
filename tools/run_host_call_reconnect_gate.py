#!/usr/bin/env python3
"""Exercise reconnect and save-state epoch resynchronization."""

import argparse
import asyncio
import json
import pathlib
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from tools.run_host_call_adapter_gate import connect


async def receive_type(websocket, kind: str, timeout: float = 20):
    while True:
        message = json.loads(await asyncio.wait_for(websocket.recv(), timeout))
        if message.get("type") == kind:
            return message


async def run(args):
    process = await asyncio.create_subprocess_exec(*args.command, cwd=args.cwd)
    try:
        first = await connect(args.port, process)
        request = await receive_type(first, "outgoing_call")
        epoch = request["epoch"]
        await first.close()

        before_decision = await connect(args.port, process)
        repeated = await receive_type(before_decision, "outgoing_call")
        if repeated != request:
            raise RuntimeError("pre-decision reconnect did not repeat call snapshot")
        await before_decision.send(json.dumps({
            "type": "outgoing_call_decision",
            "epoch": epoch,
            "request_id": 1,
            "decision": "no_answer" if args.phase == "alerting" else "connect",
        }))
        state = await receive_type(before_decision, "outgoing_call_state")
        if (state["epoch"], state["request_id"], state["phase"]) != (
                epoch, 1, args.phase):
            raise RuntimeError(f"unexpected {args.phase} snapshot {state!r}")
        await before_decision.close()

        during_traffic = await connect(args.port, process)
        repeated = await receive_type(during_traffic, "outgoing_call")
        repeated_state = await receive_type(
            during_traffic, "outgoing_call_state")
        if repeated["epoch"] != epoch or repeated_state["epoch"] != epoch:
            raise RuntimeError(f"{args.phase} reconnect changed transport epoch")
        if repeated_state["phase"] != args.phase:
            raise RuntimeError(f"reconnect changed {args.phase} call phase")

        if args.phase == "alerting":
            await during_traffic.send(json.dumps({
                "type": "outgoing_call_terminate",
                "epoch": epoch,
                "request_id": 1,
                "cause": 16,
            }))
            await asyncio.sleep(0.5)
            await during_traffic.close()
            result = await asyncio.wait_for(process.wait(), 90)
            if result:
                raise RuntimeError(f"MAME exited with status {result}")
            return

        restored_request = await receive_type(during_traffic, "outgoing_call")
        restored_epoch = restored_request["epoch"]
        if restored_epoch != epoch + 1:
            raise RuntimeError(
                f"restore epoch was {restored_epoch}, expected {epoch + 1}")
        restored_state = await receive_type(
            during_traffic, "outgoing_call_state")
        if (restored_state["epoch"], restored_state["phase"]) != (
                restored_epoch, "connected"):
            raise RuntimeError("restore did not republish connected state")

        await during_traffic.send(json.dumps({
            "type": "outgoing_call_terminate",
            "epoch": epoch,
            "request_id": 1,
            "cause": 16,
        }))
        await during_traffic.send(json.dumps({
            "type": "outgoing_call_terminate",
            "epoch": restored_epoch,
            "request_id": 1,
            "cause": 16,
        }))
        await asyncio.sleep(0.5)
        await during_traffic.close()
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
    parser.add_argument(
        "--phase", choices=("connected", "alerting"), default="connected"
    )
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
    print("OK - reconnect and restore republished an epoch-correlated call snapshot")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

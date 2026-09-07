#!/usr/bin/env python3
"""Run MAME and exercise the Nokia host SMS decision boundary."""

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
            request = None
            while request is None:
                event = json.loads(await asyncio.wait_for(websocket.recv(), 60))
                if event.get("type") == "outgoing_sms":
                    request = event
            expected = {
                "type": "outgoing_sms",
                "request_id": 1,
                "epoch": request.get("epoch"),
                "recipient": "5551234",
                "alphabet": "gsm7",
                "user_data_length": 2,
                "user_data": "c824",
            }
            if request != expected:
                raise RuntimeError(
                    f"unexpected outgoing SMS {request!r}, expected {expected!r}")
            epoch = request["epoch"]
            await websocket.send(json.dumps({
                "type": "outgoing_sms_decision",
                "epoch": epoch,
                "request_id": 2,
                "decision": "rp_error",
            }))
            decision = {
                "type": "outgoing_sms_decision",
                "epoch": epoch,
                "request_id": 1,
                "decision": args.decision,
            }
            await websocket.send(json.dumps(decision))
            await websocket.send(json.dumps(decision))
            ended = False
            while not ended:
                event = json.loads(await asyncio.wait_for(websocket.recv(), 30))
                ended = (
                    event.get("type") == "outgoing_sms_state"
                    and event.get("request_id") == 1
                    and event.get("epoch") == epoch
                    and event.get("phase") == "ended"
                )
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


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, required=True)
    parser.add_argument("--cwd")
    parser.add_argument(
        "--decision", choices=("accept", "rp_error", "rp_silence"),
        default="accept")
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.command[:1] == ["--"]:
        args.command = args.command[1:]
    if not args.command:
        parser.error("a MAME launch command is required after --")
    try:
        asyncio.run(run(args))
    except (RuntimeError, asyncio.TimeoutError) as error:
        print(f"FAIL - {error}")
        return 1
    print("OK - host received and decided one correlated outgoing SMS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

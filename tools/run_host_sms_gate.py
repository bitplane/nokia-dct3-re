#!/usr/bin/env python3
"""Run MAME and exercise the Nokia host SMS decision boundary."""

import argparse
import asyncio
import json
import math
from websockets.exceptions import ConnectionClosed

try:
    from tools.run_host_call_adapter_gate import connect
except ModuleNotFoundError:
    from run_host_call_adapter_gate import connect


def positive_seconds(value):
    seconds = float(value)
    if not math.isfinite(seconds) or seconds <= 0:
        raise argparse.ArgumentTypeError('completion timeout must be finite and positive')
    return seconds


async def wait_for_sms_end(websocket, epoch, timeout):
    # One wall-clock budget: unrelated notifications cannot extend the wait.
    async with asyncio.timeout(timeout):
        while True:
            event = json.loads(await websocket.recv())
            if (event.get('type') == 'outgoing_sms_state' and
                    event.get('request_id') == 1 and event.get('epoch') == epoch and
                    event.get('phase') == 'ended'):
                return


async def run(args: argparse.Namespace) -> None:
    process = await asyncio.create_subprocess_exec(*args.command, cwd=args.cwd)
    try:
        websocket = await connect(args.port, process)
        async with websocket:
            request = None
            current_epoch = None
            initial_request_epoch = None
            while request is None:
                event = json.loads(await asyncio.wait_for(websocket.recv(), 60))
                if event.get("type") == "call_adapter_ready":
                    current_epoch = event.get("epoch")
                    continue
                if event.get("type") != "outgoing_sms":
                    continue
                if event.get("epoch") != current_epoch:
                    raise RuntimeError(f"uncorrelated outgoing SMS {event!r}")
                if initial_request_epoch is None:
                    initial_request_epoch = current_epoch
                    if args.require_restore:
                        continue
                if args.require_restore and current_epoch == initial_request_epoch:
                    continue
                request = event
            expected = {
                "type": "outgoing_sms",
                "request_id": 1,
                "epoch": request.get("epoch"),
                "recipient": "5551234",
                "alphabet": "gsm7",
                "user_data_length": args.user_data_length,
                "user_data": args.user_data,
            }
            if request != expected:
                raise RuntimeError(
                    f"unexpected outgoing SMS {request!r}, expected {expected!r}")
            epoch = request["epoch"]
            if args.require_restore:
                await websocket.send(json.dumps({
                    "type": "outgoing_sms_decision",
                    "epoch": initial_request_epoch,
                    "request_id": 1,
                    "decision": "rp_error",
                }))
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
            await wait_for_sms_end(websocket, epoch, args.completion_timeout)
        result = await asyncio.wait_for(process.wait(), 90)
        if result:
            raise RuntimeError(f"MAME exited with status {result}")
    except ConnectionClosed as error:
        raise RuntimeError('host SMS connection closed before lifecycle completion') from error
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
    parser.add_argument("--user-data", default="c824", help="expected packed GSM7 hex bytes")
    parser.add_argument("--user-data-length", type=int, default=2,
                        help="expected number of GSM7 septets")
    parser.add_argument(
        "--decision", choices=("accept", "rp_error", "rp_silence"),
        default="accept")
    parser.add_argument("--require-restore", action="store_true")
    parser.add_argument('--completion-timeout', type=positive_seconds, default=180,
                        help='total wall-clock seconds after the host decision (default: 180)')
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

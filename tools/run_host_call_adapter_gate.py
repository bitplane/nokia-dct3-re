#!/usr/bin/env python3
"""Run MAME and exercise the Nokia outgoing-call WebSocket adapter."""

import argparse
import asyncio
import json

import websockets

async def connect(port: int, process: asyncio.subprocess.Process):
    for _ in range(300):
        if process.returncode is not None:
            raise RuntimeError(
                f"MAME exited before the call adapter opened ({process.returncode})"
            )
        try:
            return await websockets.connect(
                f"ws://127.0.0.1:{port}/nokia/dct3/calls"
            )
        except (OSError, websockets.exceptions.InvalidHandshake):
            await asyncio.sleep(0.1)
    raise RuntimeError("call adapter endpoint did not open")


async def run(args: argparse.Namespace) -> None:
    process = await asyncio.create_subprocess_exec(
        *args.command, cwd=args.cwd
    )
    try:
        websocket = await connect(args.port, process)
        async with websocket:
            if args.hostile_pre_setup:
                # No request has been published, so none of these events can
                # legitimately own emulator call state.  A burst larger than
                # the callback queue also exercises bounded overload.
                await asyncio.sleep(0.1)
                await websocket.send("{")
                await websocket.send(json.dumps({
                    "type": "unsupported",
                    "epoch": 1,
                    "request_id": 1,
                }))
                early_termination = json.dumps({
                    "type": "outgoing_call_terminate",
                    "epoch": 1,
                    "request_id": 1,
                    "cause": 16,
                })
                for _ in range(64):
                    await websocket.send(early_termination)
                await asyncio.sleep(0.5)
            event = json.loads(await asyncio.wait_for(websocket.recv(), 45))
            expected = {
                "type": "outgoing_call",
                "request_id": 1,
                "epoch": event.get("epoch"),
                "digits": args.number,
            }
            if event != expected:
                raise RuntimeError(
                    f"unexpected outgoing request {event!r}, expected {expected!r}"
                )
            epoch = event["epoch"]

            # These exercise parser and correlation failures without changing
            # emulated state. Only the final matching decision may be accepted.
            await websocket.send("{")
            await websocket.send(json.dumps({
                "type": "outgoing_call_decision",
                "epoch": epoch,
                "request_id": 2,
                "decision": "busy",
            }))
            await websocket.send(json.dumps({
                "type": "outgoing_call_decision",
                "epoch": epoch,
                "request_id": 1,
                "decision": "service_reject",
            }))
            decision = {
                "type": "outgoing_call_decision",
                "epoch": epoch,
                "request_id": 1,
                "decision": args.decision,
            }
            await websocket.send(json.dumps(decision))
            await websocket.send(json.dumps(decision))
            if args.media_frames:
                connected = False
                returned = 0
                last_frame = None
                last_time_us = 0
                while returned < args.media_frames:
                    message = json.loads(
                        await asyncio.wait_for(websocket.recv(), 20))
                    if message.get("type") == "outgoing_call_state":
                        if (message.get("request_id"), message.get("epoch"),
                                message.get("phase")) == (
                                1, epoch, "connected"):
                            connected = True
                        continue
                    if message.get("type") != "outgoing_call_media_uplink":
                        continue
                    if (not connected or message.get("request_id") != 1 or
                            message.get("epoch") != epoch):
                        raise RuntimeError(
                            "uplink media arrived outside connected call state")
                    frame = message.get("frame")
                    time_us = message.get("emulation_time_us")
                    if (not message.get("good") or
                            not isinstance(frame, str) or len(frame) != 66):
                        continue
                    if (
                        not isinstance(time_us, int)
                        or time_us < last_time_us
                    ):
                        raise RuntimeError(
                            "uplink media timestamp was absent or regressed"
                        )
                    await websocket.send(json.dumps({
                        "type": "outgoing_call_media_downlink",
                        "epoch": epoch,
                        "request_id": 1,
                        "sequence": returned,
                        "source_time_us": time_us,
                        "frame": frame,
                    }))
                    last_frame = frame
                    last_time_us = time_us
                    returned += 1
                await asyncio.sleep(0.1)
                await websocket.send(json.dumps({
                    "type": "outgoing_call_terminate",
                    "epoch": epoch,
                    "request_id": 1,
                    "cause": 16,
                }))
                await asyncio.sleep(0.2)
                await websocket.send(json.dumps({
                    "type": "outgoing_call_media_downlink",
                    "epoch": epoch,
                    "request_id": 1,
                    "sequence": returned,
                    "source_time_us": last_time_us,
                    "frame": last_frame,
                }))
            elif args.terminate:
                await websocket.send(json.dumps({
                    "type": "outgoing_call_terminate",
                    "epoch": epoch,
                    "request_id": 2,
                    "cause": 16,
                }))
                termination = {
                    "type": "outgoing_call_terminate",
                    "epoch": epoch,
                    "request_id": 1,
                    "cause": 16,
                }
                await websocket.send(json.dumps(termination))
                await websocket.send(json.dumps(termination))
            await asyncio.sleep(0.5)

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
    parser.add_argument("--number", default="5551234")
    parser.add_argument("--cwd", type=str)
    parser.add_argument(
        "--decision", choices=("connect", "busy", "no_answer"), default="busy"
    )
    parser.add_argument("--terminate", action="store_true")
    parser.add_argument("--media-frames", type=int, default=0)
    parser.add_argument("--hostile-pre-setup", action="store_true")
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
    print("OK - host adapter published request and accepted one correlated decision")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Run MAME and exercise one externally originated incoming call."""

import argparse
import asyncio
import json
import pathlib
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from tools.run_host_call_adapter_gate import connect


async def run(args):
    process = await asyncio.create_subprocess_exec(*args.command, cwd=args.cwd)
    try:
        websocket = await connect(args.port, process)
        async with websocket:
            epoch = None
            initial_epoch = None
            request_sent = False
            phases = []
            downlink_sequence = None
            returned_frames = 0
            restored = False
            termination_sent = False
            while phases[-1:] != ["ended"]:
                message = json.loads(
                    await asyncio.wait_for(websocket.recv(), 60)
                )
                kind = message.get("type")
                if kind == "call_adapter_ready":
                    if message.get("protocol_version") != 1:
                        raise RuntimeError("unsupported call adapter protocol")
                    new_epoch = message.get("epoch")
                    if not isinstance(new_epoch, int):
                        raise RuntimeError("ready event omitted transport epoch")
                    if initial_epoch is None:
                        initial_epoch = new_epoch
                    elif new_epoch != epoch:
                        restored = True
                    epoch = new_epoch
                    if not request_sent:
                        await websocket.send(json.dumps({
                            "type": "incoming_call",
                            "epoch": epoch,
                            "request_id": 1,
                            "caller": args.caller,
                        }))
                        request_sent = True
                    continue
                if kind == "incoming_call_state":
                    if message.get("request_id") != 1:
                        raise RuntimeError(f"wrong incoming identity {message!r}")
                    if message.get("epoch") != epoch:
                        raise RuntimeError(f"stale incoming state {message!r}")
                    phase = message.get("phase")
                    if not phases or phase != phases[-1]:
                        phases.append(phase)
                    if phase == "connected":
                        downlink_sequence = message.get(
                            "media_downlink_sequence")
                        if not isinstance(downlink_sequence, int):
                            raise RuntimeError(
                                "connected state omitted downlink cursor")
                    continue
                if kind != "incoming_call_media_uplink":
                    continue
                if message.get("epoch") != epoch or downlink_sequence is None:
                    continue
                frame = message.get("frame")
                time_us = message.get("emulation_time_us")
                if (message.get("good") is True and
                        isinstance(frame, str) and len(frame) == 66 and
                        isinstance(time_us, int)):
                    await websocket.send(json.dumps({
                        "type": "incoming_call_media_downlink",
                        "epoch": epoch,
                        "request_id": 1,
                        "sequence": downlink_sequence,
                        "source_time_us": time_us,
                        "frame": frame,
                    }))
                    downlink_sequence += 1
                    returned_frames += 1
                restore_ready = not args.require_restore or restored
                if (restore_ready and returned_frames >= args.frames and
                        not termination_sent):
                    await websocket.send(json.dumps({
                        "type": "incoming_call_terminate",
                        "epoch": epoch,
                        "request_id": 1,
                        "cause": 16,
                    }))
                    termination_sent = True

            required = ["queued", "paging", "alerting", "connected", "ended"]
            if any(phase not in phases for phase in required):
                raise RuntimeError(f"incomplete incoming lifecycle {phases!r}")
            if args.require_restore and not restored:
                raise RuntimeError("call completed without a save/load epoch change")

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
    parser.add_argument("--caller", default="447700900123")
    parser.add_argument("--frames", type=int, default=40)
    parser.add_argument("--require-restore", action="store_true")
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
    print("OK - external incoming call crossed paging, physical answer, media and release")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

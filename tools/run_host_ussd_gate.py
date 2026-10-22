#!/usr/bin/env python3
"""Run MAME and answer one firmware-originated USSD request from the host."""

import argparse
import asyncio
import json

try:
    from tools.run_host_call_adapter_gate import connect
    from tools.gsm7_text import encode_text
except ModuleNotFoundError:
    from run_host_call_adapter_gate import connect
    from gsm7_text import encode_text


def pack_gsm7(text: str) -> str:
    encoded = encode_text(text, ussd=True)
    if encoded is None:
        raise ValueError('text is outside the GSM default/extension alphabet')
    return encoded[0]


async def run(args: argparse.Namespace) -> None:
    process = await asyncio.create_subprocess_exec(*args.command, cwd=args.cwd,
        stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.STDOUT)
    console=asyncio.create_task(process.stdout.read())
    try:
        websocket = await connect(args.port, process)
        async with websocket:
            epoch = None
            accepted = False
            initial_request_epoch = None
            request_id = None
            responded = False
            seen_epochs = set()
            while True:
                event = json.loads(await asyncio.wait_for(websocket.recv(), 45))
                if event.get("type") == "call_adapter_ready":
                    new_epoch=event.get("epoch")
                    if type(new_epoch) is not int or new_epoch<1:
                        raise RuntimeError('invalid USSD transport epoch')
                    if epoch is not None and new_epoch!=epoch:
                        if not args.require_restore or new_epoch!=epoch+1 or initial_request_epoch is None:
                            raise RuntimeError('unexpected USSD transport restoration')
                        accepted=False
                        responded=False
                    epoch = new_epoch
                elif event.get("type") == "outgoing_ussd":
                    if event.get("epoch") != epoch:
                        raise RuntimeError(f"uncorrelated USSD request {event!r}")
                    if event.get("dcs") != 0x0f or event.get("data") != "aa986c3602":
                        raise RuntimeError(f"unexpected USSD request {event!r}")
                    identity=event.get('request_id')
                    if type(identity) is not int or identity<1 or (request_id is not None and identity!=request_id):
                        raise RuntimeError('USSD request identity changed or is invalid')
                    if epoch in seen_epochs:
                        raise RuntimeError('duplicate USSD request in one epoch')
                    seen_epochs.add(epoch)
                    request_id=identity
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
                    if args.require_restore:
                        previous=dict(response,epoch=initial_request_epoch)
                        await websocket.send(json.dumps(previous))
                    await websocket.send(json.dumps(stale))
                    await websocket.send(json.dumps(response))
                    responded=True
                elif event.get("type") == "outgoing_ussd_state":
                    if event.get("epoch") != epoch or event.get('request_id')!=request_id or not responded:
                        raise RuntimeError(f"uncorrelated USSD state {event!r}")
                    if event.get("phase") == "accepted":
                        if accepted:raise RuntimeError('duplicate USSD acceptance')
                        accepted = True
                    elif event.get("phase") == "ended":
                        if not accepted:
                            raise RuntimeError("USSD ended before host response acceptance")
                        break
                    else:
                        raise RuntimeError(f"unexpected USSD phase {event!r}")
        result = await asyncio.wait_for(process.wait(), 90)
        if result:
            raise RuntimeError(f"MAME exited with status {result}")
        output=(await console).decode(errors='replace')
        print(output,end='')
        try:
            from tools.run_sip_handset_gate import verify_handset_health
        except ModuleNotFoundError:
            from run_sip_handset_gate import verify_handset_health
        verify_handset_health(output)
    finally:
        if process.returncode is None:
            process.terminate()
            await process.wait()
        await console


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

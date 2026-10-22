#!/usr/bin/env python3
"""Run MAME and deliver one external SMS through the host adapter."""

import argparse
import asyncio
import json
import sys
from websockets.exceptions import ConnectionClosed

try:
    from tools.run_host_call_adapter_gate import connect
    from tools.dct3_call_bridge import LoopbackProtocol
except ModuleNotFoundError:
    from run_host_call_adapter_gate import connect
    from dct3_call_bridge import LoopbackProtocol


async def run(args: argparse.Namespace) -> None:
    process = await asyncio.create_subprocess_exec(*args.command, cwd=args.cwd,
        stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.STDOUT)
    console=asyncio.create_task(process.stdout.read())
    try:
        websocket = await connect(args.port, process)
        async with websocket:
            epoch = None
            while epoch is None:
                event = json.loads(await asyncio.wait_for(websocket.recv(), 30))
                if event.get("type") == "call_adapter_ready":
                    epoch = event.get("epoch")
            while True:
                event = json.loads(await asyncio.wait_for(websocket.recv(), 30))
                if (event.get("type") == "network_state" and
                        event.get("epoch") == epoch and
                        event.get("registered") is True):
                    break
            protocol = LoopbackProtocol()
            protocol.epoch = epoch
            request = protocol.incoming_sms_request('5551234', getattr(args, 'text', 'hello'))
            if request is None:
                raise RuntimeError('SMS text must fit 1..160 GSM default/extension septets')
            await websocket.send(json.dumps(request))
            phases = []
            initial_epoch = epoch
            restored = False
            while "delivered" not in phases or (args.require_restore and not restored):
                event = json.loads(await asyncio.wait_for(websocket.recv(), 30))
                if event.get("type") == "call_adapter_ready":
                    new_epoch = event.get("epoch")
                    if (args.require_restore and isinstance(new_epoch, int) and
                            new_epoch != initial_epoch):
                        if restored or new_epoch <= initial_epoch:
                            raise RuntimeError(f"unexpected SMS restore epoch {new_epoch!r}")
                        # A restore fixture may save before delivery. The
                        # reference must be a valid prefix, not necessarily
                        # complete; the restored epoch must finish delivery.
                        epoch = new_epoch
                        phases = []
                        restored = True
                    continue
                if event.get("type") == "incoming_sms_state":
                    if event.get("request_id") != 1 or event.get("epoch") != epoch:
                        raise RuntimeError(f"uncorrelated SMS state {event!r}")
                    phases.append(event.get("phase"))
                    if phases != ["queued", "delivered"][:len(phases)]:
                        raise RuntimeError(f"unexpected incoming SMS phases {phases!r}")
            if phases != ["queued", "delivered"]:
                raise RuntimeError(f"unexpected incoming SMS phases {phases!r}")
            if args.require_restore and not restored:
                raise RuntimeError("incoming SMS completed without an epoch change")
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
    except ConnectionClosed as error:
        raise RuntimeError(
            "host SMS connection closed before lifecycle completion"
        ) from error
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
    parser.add_argument("--text", default="hello", help="GSM default/extension alphabet text")
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.command[:1] == ["--"]:
        args.command = args.command[1:]
    try:
        asyncio.run(run(args))
    except (RuntimeError, asyncio.TimeoutError) as error:
        print(f"FAIL - {error}")
        return 1
    print("OK - host SMS crossed paging, SAPI 3 and handset storage")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

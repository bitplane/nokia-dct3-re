#!/usr/bin/env python3
"""Run MAME and deliver one host-originated USSD notification."""

import argparse
import asyncio
import json

try:
    from tools.run_host_call_adapter_gate import connect
    from tools.run_host_ussd_gate import pack_gsm7
except ModuleNotFoundError:
    from run_host_call_adapter_gate import connect
    from run_host_ussd_gate import pack_gsm7


async def read_console(stream, ready, marker):
    chunks=[]
    while line:=await stream.readline():
        chunks.append(line)
        if marker and marker in line.decode(errors='replace'):
            ready.set()
    return b''.join(chunks).decode(errors='replace')


async def run(args: argparse.Namespace) -> None:
    process = await asyncio.create_subprocess_exec(*args.command, cwd=args.cwd,
        stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.STDOUT)
    ready=asyncio.Event()
    marker=getattr(args,'ready_marker',None)
    console=asyncio.create_task(read_console(process.stdout,ready,marker))
    try:
        websocket = await connect(args.port, process)
        async with websocket:
            epoch = None
            registered = False
            while not registered:
                event = json.loads(await asyncio.wait_for(websocket.recv(), 40))
                if event.get("type") == "call_adapter_ready":
                    epoch = event.get("epoch")
                elif (event.get("type") == "network_state" and
                        event.get("epoch") == epoch):
                    registered = event.get("registered") is True
            if marker:
                await asyncio.wait_for(ready.wait(),40)
            await websocket.send(json.dumps({
                "type": "incoming_ussd",
                "epoch": epoch,
                "request_id": 1,
                "dcs": 0x0f,
                "data": pack_gsm7(args.text),
            }))
            phases = []
            initial_epoch = epoch
            restored = False
            while "delivered" not in phases or (args.require_restore and not restored):
                event = json.loads(await asyncio.wait_for(websocket.recv(), 40))
                if event.get("type") == "call_adapter_ready":
                    new_epoch = event.get("epoch")
                    if (args.require_restore and isinstance(new_epoch, int) and
                            new_epoch != initial_epoch):
                        if restored or new_epoch <= initial_epoch:
                            raise RuntimeError(f"unexpected USSD restore epoch {new_epoch!r}")
                        epoch = new_epoch
                        phases = []
                        restored = True
                    continue
                if event.get("type") == "incoming_ussd_state":
                    if event.get("epoch") != epoch or event.get("request_id") != 1:
                        raise RuntimeError(f"uncorrelated incoming USSD state {event!r}")
                    phases.append(event.get("phase"))
                    if phases != ["queued", "delivered"][:len(phases)]:
                        raise RuntimeError(f"unexpected incoming USSD phases {phases!r}")
            if phases != ["queued", "delivered"]:
                raise RuntimeError(f"unexpected incoming USSD phases {phases!r}")
            if args.require_restore and not restored:
                raise RuntimeError("incoming USSD completed without an epoch change")
        result = await asyncio.wait_for(process.wait(), 90)
        if result:
            raise RuntimeError(f"MAME exited with status {result}")
        output=await console
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
    parser.add_argument("--ready-marker", help="wait for this fixture console marker before admission")
    parser.add_argument("--text", default="Host notice")
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if not args.text:
        parser.error('--text must not be empty')
    try:
        encoded = pack_gsm7(args.text)
    except ValueError as error:
        parser.error(str(error))
    if len(encoded) > 320:
        parser.error('--text must fit in 160 packed octets')
    if args.command[:1] == ["--"]:
        args.command = args.command[1:]
    try:
        asyncio.run(run(args))
    except (RuntimeError, asyncio.TimeoutError) as error:
        print(f"FAIL - {error}")
        return 1
    print("OK - host USSD notification crossed paging and firmware acknowledgement")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

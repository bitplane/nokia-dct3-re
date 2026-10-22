#!/usr/bin/env python3
"""Run MAME and verify one host-originated USSD notification or request."""

import argparse
import asyncio
import json
from websockets.exceptions import ConnectionClosed

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
            request_mode = getattr(args, 'request', False)
            empty_reply = getattr(args, 'empty_reply', False)
            admission = {
                "type": "incoming_ussd",
                "epoch": epoch,
                "request_id": 1,
                "dcs": 0x0f,
                "data": pack_gsm7(args.text),
            }
            if request_mode:
                admission['operation'] = 'request'
            await websocket.send(json.dumps(admission))
            phases = []
            initial_epoch = epoch
            restored = False
            terminal = 'answered' if request_mode else 'delivered'
            response_seen = False
            while terminal not in phases or (args.require_restore and not restored):
                event = json.loads(await asyncio.wait_for(websocket.recv(), 40))
                if event.get("type") == "call_adapter_ready":
                    new_epoch = event.get("epoch")
                    if (args.require_restore and isinstance(new_epoch, int) and
                            new_epoch != initial_epoch):
                        if restored or new_epoch <= initial_epoch:
                            raise RuntimeError(f"unexpected USSD restore epoch {new_epoch!r}")
                        epoch = new_epoch
                        phases = []
                        response_seen = False
                        restored = True
                    continue
                if event.get('type') == 'incoming_ussd_response':
                    expected = {'type': 'incoming_ussd_response', 'epoch': epoch,
                                'request_id': 1, 'dcs': 0 if empty_reply else 0x0f,
                                'data': '' if empty_reply else pack_gsm7(getattr(args, 'expected_reply', '2')),
                                'empty': empty_reply}
                    if not request_mode or response_seen or phases != ['queued'] or event != expected:
                        raise RuntimeError(f'unexpected incoming USSD reply {event!r}')
                    response_seen = True
                    print('host_ussd_reply: '+json.dumps(event, sort_keys=True), flush=True)
                if event.get("type") == "incoming_ussd_state":
                    if event.get("epoch") != epoch or event.get("request_id") != 1:
                        raise RuntimeError(f"uncorrelated incoming USSD state {event!r}")
                    phases.append(event.get("phase"))
                    if phases != ["queued", terminal][:len(phases)]:
                        raise RuntimeError(f"unexpected incoming USSD phases {phases!r}")
                    if request_mode and event.get('phase') == terminal and not response_seen:
                        raise RuntimeError('USSD answered without a correlated host reply')
            if phases != ["queued", terminal]:
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
    parser.add_argument('--request', action='store_true', help='request a physical reply instead of notification')
    parser.add_argument('--empty-reply', action='store_true', help='require empty ReturnResult after physical editor dismissal')
    parser.add_argument('--expected-reply', default='2', help='exact physical reply text required in request mode')
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.empty_reply and not args.request:
        parser.error('--empty-reply requires --request')
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
    except (RuntimeError, asyncio.TimeoutError, ConnectionClosed) as error:
        print(f"FAIL - {error}")
        return 1
    print("OK - host USSD request/reply completed" if args.request else
          "OK - host USSD notification crossed paging and firmware acknowledgement")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

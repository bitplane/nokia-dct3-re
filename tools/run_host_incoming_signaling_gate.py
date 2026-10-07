"""Queue a host call after an emulation-produced readiness artifact."""
import argparse
import asyncio
import json
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.run_host_call_adapter_gate import connect


async def run(args):
    if args.ready_file.exists():
        raise RuntimeError('readiness artifact already exists; use a fresh run')
    process = await asyncio.create_subprocess_exec(*args.command, cwd=args.cwd)
    try:
        async with await connect(args.port, process) as socket:
            while True:
                message = json.loads(await asyncio.wait_for(socket.recv(), 30))
                if message.get('type') == 'call_adapter_ready':
                    epoch = message['epoch']
                    break
            async def wait_ready():
                while not args.ready_file.exists():
                    if process.returncode is not None:
                        raise RuntimeError('MAME ended before readiness artifact')
                    await asyncio.sleep(0.01)
            await asyncio.wait_for(wait_ready(), 60)
            await socket.send(json.dumps({'type': 'incoming_call', 'epoch': epoch,
                'request_id': 1, 'caller': args.caller}))
            phases = []
            while phases[-1:] != ['ended']:
                message = json.loads(await asyncio.wait_for(socket.recv(), 30))
                if message.get('type') == 'incoming_call_state':
                    if message.get('epoch') != epoch or message.get('request_id') != 1:
                        raise RuntimeError('incoming identity/epoch mismatch')
                    if phases[-1:] != [message['phase']]:
                        phases.append(message['phase'])
            if phases != ['queued', 'paging', 'alerting', 'connected', 'ended']:
                raise RuntimeError(f'incomplete incoming lifecycle: {phases}')
        if await asyncio.wait_for(process.wait(), 90):
            raise RuntimeError('MAME exited unsuccessfully')
    finally:
        if process.returncode is None:
            process.terminate()
            try:
                await asyncio.wait_for(process.wait(), 5)
            except asyncio.TimeoutError:
                process.kill()
                await process.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, required=True)
    parser.add_argument('--ready-file', type=Path, required=True)
    parser.add_argument('--cwd')
    parser.add_argument('--caller', default='447700900123')
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.command[:1] == ['--']:
        args.command = args.command[1:]
    if not args.command:
        parser.error('MAME command required after --')
    args.ready_file = args.ready_file.resolve()
    try:
        asyncio.run(run(args))
    except (RuntimeError, asyncio.TimeoutError, OSError) as error:
        parser.exit(1, f'host incoming signaling FAIL: {error}\n')
    print('host incoming queued/paging/alerting/connected/ended PASS; speech unproved')


if __name__ == '__main__':
    main()

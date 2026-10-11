"""Queue a host call after an emulation-produced readiness artifact."""
import argparse
import asyncio
import json
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.run_host_call_adapter_gate import connect


def verify_phases(phases):
    expected = ['queued', 'paging', 'alerting', 'connected']
    if phases not in (expected + ['ended'], expected + ['media_closed', 'ended']):
        raise RuntimeError(f'incomplete incoming lifecycle: {phases}')


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
            restored = False
            while phases[-1:] != ['ended']:
                message = json.loads(await asyncio.wait_for(socket.recv(), 30))
                if message.get('type') == 'incoming_call_state':
                    if message.get('request_id') != 1:
                        raise RuntimeError('incoming identity/epoch mismatch')
                    if args.observe_restore and message.get('epoch') == epoch + 1 and not restored:
                        if phases != ['queued', 'paging', 'alerting', 'connected'] or message['phase'] != 'connected':
                            raise RuntimeError('incoming restore omitted connected predecessor')
                        restored = True
                        await socket.send(json.dumps({'type': 'incoming_call_terminate',
                            'epoch': epoch, 'request_id': 1, 'cause': 16}))
                        epoch += 1
                    if message.get('epoch') != epoch:
                        raise RuntimeError('incoming identity/epoch mismatch')
                    if phases[-1:] != [message['phase']]:
                        phases.append(message['phase'])
            verify_phases(phases)
            if args.observe_restore and not restored:
                raise RuntimeError('incoming call ended without restoration')
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
    parser.add_argument('--observe-restore', action='store_true')
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

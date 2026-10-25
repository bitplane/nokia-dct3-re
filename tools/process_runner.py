"""Own and reap a complete emulator/runner process group, including interrupts."""
import json
import os
from pathlib import Path
import signal
import subprocess


def run_owned_process(command, cwd, output, timeout=None, status_path=None):
    def terminate(signum, frame):
        raise SystemExit(128 + signum)

    previous = signal.signal(signal.SIGTERM, terminate)
    process = None
    interrupted = False
    try:
        process = subprocess.Popen(command, cwd=cwd, stdout=output,
                                   stderr=subprocess.STDOUT, start_new_session=True)
        try:
            return process.wait(timeout=timeout)
        except BaseException:
            interrupted = True
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            process.wait()
            raise
    finally:
        signal.signal(signal.SIGTERM, previous)
        if status_path is not None:
            Path(status_path).write_text(json.dumps({
                'returncode': process.returncode if process is not None else None,
                'command': command, 'interrupted': interrupted,
            }, indent=2) + '\n')


def run_emulator(command, run, output, timeout=None):
    return run_owned_process(command, run, output, timeout,
                             Path(run) / 'exit_status.json')


def run_stage(command, cwd, stream, timeout=180):
    status = run_owned_process(command, cwd, stream, timeout)
    if status:
        raise subprocess.CalledProcessError(status, command)

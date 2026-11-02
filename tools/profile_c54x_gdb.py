"""Coarse host-cost sampling, run inside GDB with an optimized symbol build.

Usage: DCT3_PROFILE_JSON=/absolute/report.json gdb -batch \
  -x tools/profile_c54x_gdb.py --args /absolute/mame tms54rom4 ...

This interrupts the whole fixture, including replay and validation. It is not
a throughput benchmark or an emulated-cycle profile. Do not enable -verbose.
Only the C54x translation unit needs -g; retain the normal optimization flags.
"""
import collections
import json
import os
import random
import signal
import threading

import gdb


output = os.environ['DCT3_PROFILE_JSON']
gdb.execute('set pagination off')
gdb.execute('set confirm off')
gdb.execute('set debuginfod enabled off')
gdb.execute('handle SIGINT stop nopass')
gdb.execute('start', to_string=True)
for breakpoint in gdb.breakpoints() or []:
    breakpoint.delete()
pid = gdb.selected_inferior().pid
running = threading.Event()
finished = threading.Event()


def interrupt():
    rng = random.Random(54)
    while not finished.is_set():
        if not running.wait(.1):
            continue
        # Jitter avoids a fixed interval locking onto a periodic fixture phase.
        if finished.wait(rng.uniform(.010, .030)):
            return
        running.clear()
        try:
            os.kill(pid, signal.SIGINT)
        except ProcessLookupError:
            return


threading.Thread(target=interrupt, daemon=True).start()
exclusive = collections.Counter()
inclusive = collections.Counter()
locations = collections.Counter()
pipeline_locations = collections.Counter()
samples = 0
complete = False
try:
    while gdb.selected_inferior().pid:
        running.set()
        gdb.execute('continue', to_string=True)
        if not gdb.selected_inferior().pid:
            complete = True
            break
        threads = gdb.selected_inferior().threads()
        next((thread for thread in threads if thread.num == 1), threads[0]).switch()
        frame = gdb.newest_frame()
        names = []
        while frame and len(names) < 20:
            names.append(frame.name() or hex(frame.pc()))
            source = frame.find_sal()
            if frame.name() == 'tms320c54x_device::pipeline_step':
                pipeline_locations[str(source.line)] += 1
            if len(names) == 1 and source.symtab:
                locations[source.symtab.filename + ':' + str(source.line)] += 1
            frame = frame.older()
        if names:
            samples += 1
            exclusive[names[0]] += 1
            inclusive.update(set(names))
finally:
    finished.set()
    report = dict(scope='whole fixture, including replay; interrupted main-thread samples',
                  binary=gdb.current_progspace().filename, gdb_version=gdb.VERSION,
                  interval_seconds=[.010, .030],
                  complete=complete, samples=samples,
                  exclusive=exclusive.most_common(), inclusive=inclusive.most_common(),
                  locations=locations.most_common(), pipeline_locations=pipeline_locations.most_common())
    with open(output, 'w') as stream:
        json.dump(report, stream, indent=2)

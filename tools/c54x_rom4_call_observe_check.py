"""Validate observation completeness, never connected-call success."""
import re


def inspect(text, console, machine):
    if re.search(r'Lua.*(?:error|Error)|stack traceback|FATALERROR', console + '\n' + text):
        raise ValueError('observer or emulator error')
    ends = re.findall(r'rom4_call_end: machine=(\w+) illegal=(\d+) mode=([0-9a-f]{4}) flags=([0-9a-f]{4})', text)
    if len(ends) != 1 or ends[0][0] != machine or ends[0][1] != '0':
        raise ValueError('missing, duplicate, wrong-machine or faulted DSP completion')
    codecs = re.findall(r'rom4_call_codec: encoder=(\d+) decoder=(\d+)', text)
    if len(codecs) != 1:
        raise ValueError('missing or duplicate codec observation')
    counts = {}
    for kind in ('host_write', 'payload_read', 'cobba_write'):
        summaries = re.findall(rf'rom4_call_count: kind={kind} count=(\d+) cap=(\d+)', text)
        if len(summaries) != 1:
            raise ValueError(f'missing or duplicate {kind} summary')
        total, cap = map(int, summaries[0])
        events = re.findall(rf'rom4_call_bus: kind={kind} address=([0-9a-f]{{8}}) value=([0-9a-f]{{8}}) mask=([0-9a-f]{{8}}) t=([0-9.]+)', text)
        if total > cap or len(events) != total:
            raise ValueError(f'truncated or inconsistent {kind} observation')
        if not events:
            raise ValueError(f'no {kind} positive control')
        previous = 7.0
        for _, _, mask, when in events:
            time = float(when)
            if time < previous or not int(mask, 16):
                raise ValueError(f'invalid {kind} mask/time')
            previous = time
        counts[kind] = total
    return dict(observation_complete=True, connected_call_claim=False,
                native_speech_claim=False, machine=machine,
                mode=ends[0][2], flags=ends[0][3], counts=counts,
                codec_entries=dict(zip(('encoder', 'decoder'), map(int, codecs[0]))))

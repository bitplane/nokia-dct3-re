"""Check whole-session decoder returns; ordinal PCM comparison is diagnostic."""
import argparse
from array import array
import hashlib
import json
import math
from pathlib import Path
import re
import sys

if __package__:
    from .mu4_context_contract import contract
    from .mu4_coefficient_check import verify as verify_arithmetic
    from .mu4_playback_capture_check import verify as verify_capture
    from .mu4_playback_frame_check import rows
else:
    from mu4_context_contract import contract
    from mu4_coefficient_check import verify as verify_arithmetic
    from mu4_playback_capture_check import verify as verify_capture
    from mu4_playback_frame_check import rows

FILE = 'mu4_decoded_returns.le16'
END = re.compile(
    r'mu4_decoded_return_capture: calls=(\d+) returns=(\d+) blocks=(\d+) words=(\d+) bytes=(\d+) '
    r'fnv1a=([0-9a-f]{8}) bad=0 pending=0 capped=0 encoding=le16 file=mu4_decoded_returns.le16 '
    r'retirement=unclaimed save_restore=unsupported observer_writes=0 audio_acceptance=0$')


def signed_words(data):
    if len(data)%2:
        raise ValueError('partial PCM word')
    values = array('h')
    values.frombytes(data)
    if sys.byteorder != 'little':
        values.byteswap()
    return values


def check_session(text, data, reference):
    terminals = [line for line in text.splitlines() if 'mu4_decoded_return_capture:' in line]
    if len(terminals) != 1 or not (end := END.search(terminals[0])):
        raise ValueError('missing, incomplete or malformed decoder session terminal')
    calls,returned,blocks,words,extent = map(int,end.group(1,2,3,4,5))
    if not 0 < calls < 512 or returned != calls or not blocks or words != 2048*blocks or len(data) != extent or extent != 2*words:
        raise ValueError('decoder session counts or PCM extent mismatch')
    fingerprint = 2166136261
    for byte in data:
        fingerprint = ((fingerprint ^ byte)*16777619) & 0xffffffff
    if f'{fingerprint:08x}' != end[6]:
        raise ValueError('decoder return hash mismatch')
    events = rows(text,'mu4_decoder_call')
    outputs = rows(text,'mu4_decoder_block')
    if len(events) != 2*calls or len(outputs) != blocks:
        raise ValueError('decoder call/block coverage mismatch')
    successes,failures = [],[]
    previous = -1.0
    for index in range(calls):
        entry,result = events[2*index:2*index+2]
        if (entry.get('phase'),result.get('phase')) != ('entry','return'):
            raise ValueError('decoder call phase order changed')
        if any(int(row['call']) != index+1 or row.get('identity') != 'Execute_entry' or row.get('context_only') != '1'
               for row in (entry,result)):
            raise ValueError('decoder call identity changed')
        start,stop = float(entry['t']),float(result['t'])
        if not previous < start < stop:
            raise ValueError('decoder call timestamps overlap')
        previous = stop
        code = int(result['result'],16)
        if not 0 <= code < 1<<40:
            raise ValueError('decoder result outside accumulator domain')
        # The caller stores/tests the returned low word; retain upper bits as context.
        status = code & 0xffff
        if status:
            failures.append({'call':index+1,'result':result['result'],'status_word':f'{status:04x}','time':stop})
        else:
            successes.append((index+1,stop))
    if len(successes) != blocks:
        raise ValueError('successful decoder calls differ from converted blocks')
    for frame,(output,(call,returned_at)) in enumerate(zip(outputs,successes)):
        if (int(output['call']),int(output['frame']),int(output['words']),output.get('bad')) != (call,frame,2048*(frame+1),'0'):
            raise ValueError('decoder block identity or cumulative sample count changed')
        if output.get('identity') != 'Execute_entry' or output.get('retirement') != 'unclaimed':
            raise ValueError('decoder block observation scope changed')
        when = float(output['t'])
        if not returned_at < when or (call < calls and when >= float(events[2*call]['t'])):
            raise ValueError('decoder output not between return and next call')
    actual,wanted = signed_words(data),signed_words(reference)
    if not wanted or len(wanted)%2048:
        raise ValueError('reference lacks complete stereo sample frames')
    prefix = failures[0]['call']-1 if failures else blocks
    shared = min(len(actual)//2048,len(wanted)//2048,prefix)
    metrics = []
    squared_errors = 0
    for frame in range(shared):
        begin = frame*2048
        errors = [actual[i]-wanted[i] for i in range(begin,begin+2048)]
        squared_errors += sum(error*error for error in errors)
        metrics.append({'frame':frame,'mismatched_samples':sum(error != 0 for error in errors),
            'max_absolute_error':max(map(abs,errors)),
            'rms_error':math.sqrt(sum(error*error for error in errors)/2048)})
    return {'calls':calls,'converted_blocks':blocks,'failed_calls':failures,
        'native_return_sha256':hashlib.sha256(data).hexdigest(),
        'reference_sha256':hashlib.sha256(reference).hexdigest(),
        'compared_blocks':shared,'native_blocks_not_compared':blocks-shared,
        'reference_blocks_not_compared':len(wanted)//2048-shared,'frame_metrics':metrics,
        'comparison_stops_at_first_failure':bool(failures),
        'reference_comparison_diagnostic':{'samples':shared*2048,
            'mismatched_samples':sum(frame['mismatched_samples'] for frame in metrics),
            'max_absolute_error':max((frame['max_absolute_error'] for frame in metrics),default=None),
            'rms_error':math.sqrt(squared_errors/(shared*2048)) if shared else None},
        'fitted_alignment':False,'fitted_gain':False,'native_audio_validated':False,
        'scope':'Chronological converter-return words, not retired stores, complete packet binding, codec conformance, analog output or speech'}


def verify(directory, source, reference):
    contract(source)
    text = (directory/'error.log').read_text()
    verify_capture(text,directory,require_framing=True,require_tx_times=True)
    arithmetic = verify_arithmetic(directory,source)
    report = check_session(text,(directory/FILE).read_bytes(),reference)
    report['bounded_eight_call_arithmetic_validated'] = arithmetic['coefficient_publication_validated']
    report['source_a00_sha256'] = hashlib.sha256(source).hexdigest()
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--source-a00',type=Path,required=True)
    parser.add_argument('--reference-pcm',type=Path,required=True)
    args = parser.parse_args()
    try:
        report = verify(args.directory,args.source_a00.read_bytes(),args.reference_pcm.read_bytes())
    except (OSError,KeyError,ValueError) as error:
        print(f'MU4 decoder session: {error}',file=sys.stderr)
        return 1
    print(json.dumps(report,indent=2,sort_keys=True))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

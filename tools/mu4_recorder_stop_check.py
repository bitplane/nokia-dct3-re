"""Check changed-write stop ordering, not retained instructions or file draining."""
import argparse
import json
import math
from pathlib import Path
import sys

if __package__:
    from .mu4_playback_frame_check import rows
    from .mu4_context_contract import contract
else:
    from mu4_playback_frame_check import rows
    from mu4_context_contract import contract


def check(text):
    scope = dict(first='0.8',last='15',cap='32768',boundary='changed_data_write',
                 identical_full_writes='deduplicated',context_pc='not_owner',
                 observer_writes='0',audio_acceptance='0')
    events, ends = rows(text,'mu4_recorder_stop_write'), rows(text,'mu4_recorder_stop_terminal')
    if rows(text,'mu4_recorder_stop_scope') != [scope] or len(ends) != 1 or not events:
        raise ValueError('missing or changed recorder stop observation scope')
    if 'LUA ERROR' in text or '\0' in text:
        raise ValueError('failed or corrupt recorder observation')
    marker = 'mu4_native_recorder_control_entry: pc=0285dc pin_request=1'
    if text.count(marker) != 1:
        raise ValueError('missing or duplicate original control entry')
    control = text.index(marker)
    control_events = len(rows(text[:control],'mu4_recorder_stop_write'))
    overlay = 'mu4_native_recorder_overlay: PASS marker=aa88 records=4622 words=73766 checked_ram_words=73762 excluded_register_words=4 before_execution=1'
    if text.count(overlay) != 1 or text.index(overlay) >= control:
        raise ValueError('missing original recorder verification before stop')
    end = ends[0]
    if text.index('mu4_recorder_stop_terminal:') < text.rindex('mu4_recorder_stop_write:'):
        raise ValueError('stop terminal precedes observed writes')
    try:
        stop = float(end['t'])
        if (not len(events) == int(end['events']) < 32768 or int(end['bus_writes']) < len(events)
                or end['closed'] != '1' or end['capped'] != '0' or end['illegal'] != '0'
                or end['observer_writes'] != '0' or end['audio_acceptance'] != '0'
                or not math.isfinite(stop) or stop < 15):
            raise ValueError('incomplete, capped or faulted stop observation')
        state, witnesses = {}, {}
        previous_time, previous_bus = 0.8, 0
        for ordinal, event in enumerate(events,1):
            address, value, pc = (int(event[key],16) for key in ('address','value','context_pc'))
            when, bus = float(event['t']), int(event['bus_ordinal'])
            allowed = (0x80 <= address <= 0x8f or 0x5eb <= address <= 0x5f3 or
                       0xfd85 <= address <= 0xfd88 or address in (0x373e,0x3762,0x3763))
            if (int(event['ordinal']) != ordinal or not previous_time <= when <= 15
                    or not math.isfinite(when) or bus <= previous_bus or not allowed
                    or not 0 <= value <= 0xffff or not 0 <= pc <= 0x7fffff
                    or event['mask'] != 'ffff' or event['observer_writes'] != '0'):
                raise ValueError('changed stop-write domain or order')
            old = state.get(address)
            state[address] = value
            previous_time, previous_bus = when,bus
            observed = dict(ordinal=ordinal,t=when,context_pc=f'{pc:06x}')
            after_control = ordinal > control_events
            if not witnesses and after_control and address == 0x80 and value == 3:
                if [state.get(at) for at in range(0x80,0x90)] != [3,0,0,1]+[0]*12:
                    raise ValueError('stop request is not the source-selected zero-extended record')
                witnesses['request'] = observed
            if not witnesses:
                continue
            if address == 0x5eb and value == 1 and 'flag' not in witnesses:
                witnesses['flag'] = observed
            if address == 0x5f3 and value == 1 and 'flag' in witnesses:
                witnesses.setdefault('deferred_flag',observed)
            if address == 0x80 and value == 0x8003 and 'ack' not in witnesses:
                witnesses['ack'] = observed
            if address in (0x5ed,0x5ee) and value == 0 and 'flag' in witnesses and 'reset' not in witnesses:
                if state.get(0x5f0) != 0:
                    raise ValueError('buffer reset lacks its observed zero guard')
                key = f'reset_{address:04x}'
                witnesses.setdefault(key,dict(observed,previous=old))
            if address == 0x5eb and value == 0 and 'flag' in witnesses:
                witnesses.setdefault('reset',observed)
            if address == 0xfd85 and value == 0x8009:
                witnesses.setdefault('completion',observed)
            if address == 0x373e and value == 2 and 'completion' in witnesses:
                if state.get(0xfd86) != 1:
                    raise ValueError('completion status differs from the selected success consumer')
                witnesses.setdefault('resident',observed)
        names = ('request','flag','deferred_flag','ack','reset_05ed','reset_05ee','reset','completion','resident')
        if any(name not in witnesses for name in names):
            raise ValueError('incomplete source-selected stop/reset/completion chain')
        if [witnesses[name]['ordinal'] for name in names] != sorted(witnesses[name]['ordinal'] for name in names):
            raise ValueError('stop request, reset and completion ordering differs')
        if int(end['bus_writes']) < previous_bus:
            raise ValueError('stop terminal bus count precedes observed writes')
    except (KeyError,TypeError,OverflowError) as error:
        raise ValueError('incomplete stop observation fields') from error
    return dict(changed_writes=len(events),bus_writes=int(end['bus_writes']),witnesses=witnesses,
                scope='Changed RAM writes and context PCs; source-selected ordering, not retained operands, buffer units, committed file extent or audio acceptance')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log',type=Path)
    parser.add_argument('--source-a00',type=Path,required=True)
    args = parser.parse_args()
    try:
        contract(args.source_a00.read_bytes())
        report = check(args.log.read_text())
    except (ValueError,OSError) as error:
        parser.exit(1,f'MU4 recorder stop: {error}\n')
    print(json.dumps(report,indent=2))


if __name__ == '__main__':
    main()

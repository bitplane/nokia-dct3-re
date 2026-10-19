"""Bind actual McBSP buffer transitions to owned writes and completed serial words."""
import argparse
from bisect import bisect_left
from collections import Counter
from decimal import Decimal
import json
from pathlib import Path
import struct
import sys

if __package__:
    from .mu4_playback_frame_check import rows
    from .mu4_playback_capture_check import (FILES, parse_rows, validate_stream, parse_slot_row,
        validate_slots, parse_tx_time_row, validate_tx_times, compare_serial_callbacks)
    from .mu4_playback_dxr_check import check_writes, parse_row
else:
    from mu4_playback_frame_check import rows
    from mu4_playback_capture_check import (FILES, parse_rows, validate_stream, parse_slot_row,
        validate_slots, parse_tx_time_row, validate_tx_times, compare_serial_callbacks)
    from mu4_playback_dxr_check import check_writes, parse_row


def bind_lifecycle(events, writes, tx, times, *, first_ns=1_000_000_000,
                   last_ns=1_250_000_000, first_ordinal=1):
    """Current 16-bit MU4 format; prefix state remains explicitly unknown."""
    if len(tx) != len(times) or len(set(times)) != len(times):
        raise ValueError('invalid completed-TX timestamp extent')
    tx_by_time = dict(zip(times,tx))
    owned = {ordinal:(when,word) for when,ordinal,word,owner,reserved in writes if owner == 4}
    if len(owned) != sum(owner == 4 for _,_,_,owner,_ in writes):
        raise ValueError('duplicate owned DXR ordinal')
    buffer_known = shift_known = False
    buffer = shift = retained = None
    accepted,discarded,ignored,completed = set(),set(),set(),{}
    kinds,unknown = Counter(),0
    previous = first_ns
    for ordinal,row in enumerate(events,first_ordinal):
        time,word,owner,source = int(row['ns']),int(row['word'],16),int(row['dma_owner']),int(row['dma_read'])
        kind = row['kind']
        if (int(row['ordinal']) != ordinal or not previous <= time <= last_ns
                or not 0 <= word <= 0xffff or row['audio_acceptance'] != '0'):
            raise ValueError('invalid McBSP lifecycle event')
        previous = time
        kinds[kind] += 1
        if kind in ('accept','overwrite','ignore'):
            if owner != 4 or not source or owned.get(source) != (time,word) or source in accepted|ignored:
                raise ValueError('buffer event lacks unique actual channel-3 DXR write')
        elif owner or source:
            raise ValueError('non-write transition claims active DMA ownership')
        if kind == 'reset':
            for value in (buffer,shift):
                if value is not None and value[0] is not None:
                    discarded.add(value[0])
            buffer = shift = None
            buffer_known = shift_known = True
            retained = (None,0)
        elif kind == 'ignore':
            ignored.add(source)
        elif kind in ('accept','overwrite'):
            if buffer_known and ((kind == 'accept' and buffer is not None) or (kind == 'overwrite' and buffer is None)):
                raise ValueError('accept/overwrite contradicts observed DXR occupancy')
            if buffer is not None and buffer[0] is not None:
                discarded.add(buffer[0])
            buffer = retained = (source,word)
            buffer_known = True
            accepted.add(source)
        elif kind in ('load_fresh','load_repeat'):
            if shift_known and shift is not None:
                raise ValueError('shift load replaces an active word without restart/reset')
            if kind == 'load_fresh':
                if buffer_known and buffer is None:
                    raise ValueError('fresh shift load without full DXR')
                value = buffer if buffer_known else (None,word)
            else:
                if buffer_known and buffer is not None:
                    raise ValueError('repeat shift load with a fresh buffered word')
                value = retained if retained is not None else (None,word)
            if value[1] != word:
                raise ValueError('loaded shift word differs from its actual buffer epoch')
            shift = value
            shift_known = buffer_known = True
            buffer = None
        elif kind == 'restart':
            if shift_known and (shift is None or shift[1] != word):
                raise ValueError('restart does not retain its active shift word')
            if not shift_known:
                shift = (None,word)
                shift_known = True
        elif kind == 'wait_frame':
            if (shift_known and shift is not None) or (buffer_known and buffer is not None):
                raise ValueError('frame wait contradicts active shift or buffered word')
            if retained is not None and retained[1] != word:
                raise ValueError('frame wait differs from retained DXR word')
        elif kind == 'complete':
            if time not in tx_by_time or tx_by_time[time] != word:
                raise ValueError('lifecycle completion lacks exact completed-TX callback')
            if shift_known and (shift is None or shift[1] != word):
                raise ValueError('completed shift word differs from observed load')
            if shift is None or shift[0] is None:
                unknown += 1
            else:
                completed.setdefault(shift[0],[]).append((time,word))
            shift,shift_known = None,True
        else:
            raise ValueError('unknown McBSP lifecycle transition')
    pending = {value[0] for value in (buffer,shift) if value is not None and value[0] is not None}
    if accepted-set(completed)-discarded-pending:
        raise ValueError('accepted buffer epochs disappear without completion/discard/pending state')
    report = dict(events=len(events),kinds=dict(kinds),accepted=len(accepted),ignored=len(ignored),
                  discarded=len(discarded),endpoint_pending=sorted(pending),
                  discarded_source_ordinals=sorted(discarded),
                  source_epochs_completed=len(completed),unknown_prefix_completions=unknown,
                  internal_buffer_loads_observed=bool(kinds['load_fresh']+kinds['load_repeat']),
                  native_audio_validated=False)
    return report,completed


def codec_control_history(text):
    registers = [0x97,0x97,0x79,0x79,0x0a,0x08,0x07,0x01,0x20,0]
    history = []
    fields = {'power':6,'analog':4,'digital':5,'format':7,'rate':8,'active':9,
              'hp_left':2,'hp_right':3}
    previous = 0
    for row in rows(text,'mu4_codec_control'):
        time,word = int(row['ns']),int(row['word'],16)
        if time < previous or not 0 <= word <= 0xffff or row['audio_acceptance'] != '0':
            raise ValueError('invalid codec control chronology')
        previous = time
        address,data = word >> 9,word & 0x1ff
        if address == 15 and data == 0:
            registers = [0x97,0x97,0x79,0x79,0x0a,0x08,0x07,0x01,0x20,0]
        elif address < 10:
            registers[address] = data
            if address in (2,3) and data & 0x100:
                registers[address^1] = (registers[address^1] & ~0x7f) | (data & 0x7f)
        if any(int(row[field],16) != registers[index] for field,index in fields.items()):
            raise ValueError('codec snapshot differs from control-word/latch contract')
        history.append((time,tuple(registers)))
    return history


def frame_wait_din(events,din,slots):
    """Bounded observed underrun-to-next-load intervals, not analog pin proof."""
    if len(slots) != 9*len(din):
        raise ValueError('frame-wait DIN metadata extent mismatch')
    intervals, pending = [], None
    for row in events:
        kind,when = row['kind'],int(row['ns'])
        if kind == 'wait_frame':
            if pending is not None:
                raise ValueError('duplicate wait without intervening frame load')
            pending = (when,0xffff if int(row['word'],16)&1 else 0)
        elif kind in ('load_fresh','load_repeat','reset') and pending is not None:
            intervals.append((pending[0],when,pending[1]))
            pending = None
    times = [when for when,slot in struct.iter_unpack('<QB',slots)]
    count = 0
    for first,last,expected in intervals:
        for index in range(bisect_left(times,first),bisect_left(times,last)):
            if din[index] != expected:
                raise ValueError('codec DIN during frame wait differs from held final bit')
            count += 1
    return {'closed_intervals':len(intervals),'din_words':count,
            'open_endpoint_interval':pending is not None,
            'scope':'Observed frame-wait intervals only; not whole-session or silicon acceptance'}


def startup_report(text,writes,tx,times,din,slots):
    events,ends = rows(text,'mu4_tx_startup'),rows(text,'mu4_tx_startup_terminal')
    if not events and not ends:
        return None
    frames = rows(text,'mu4_startup_frame')
    if (len(ends) != 1 or len(events) != int(ends[0]['events']) or not events
            or len(frames) != int(ends[0]['frames']) or ends[0]['capped'] != '0'
            or ends[0]['first'] != '0.817000000' or ends[0]['last'] != '0.824000000'
            or Decimal(ends[0]['t']) < Decimal('0.824')):
        raise ValueError('incomplete startup framing capture')
    for ordinal,row in enumerate(events,1):
        if (int(row['ordinal']) != ordinal or row['pcr'] != '000e'
                or row['xcr1'] != '0140' or row['xcr2'] != '0044'):
            raise ValueError('startup format or event sequence changed')
    previous,state = 817_000_000,None
    for frame in frames:
        time,current = int(frame['ns']),int(frame['state'])
        if not previous <= time <= 824_000_000 or current not in (0,1) or current == state:
            raise ValueError('invalid startup frame-edge sequence')
        previous,state = time,current
    first = next((i for i,row in enumerate(events) if row['kind'] == 'accept' and row['dma_read'] == '1'),None)
    resets = [] if first is None else [i for i in range(first) if events[i]['kind'] == 'reset']
    if not resets:
        raise ValueError('no observed reset before first DMA acceptance')
    tail = events[resets[-1]:]
    report,completed = bind_lifecycle(tail,writes,tx,times,first_ns=817_000_000,
                                     last_ns=824_000_000,first_ordinal=int(tail[0]['ordinal']))
    first_time,first_word,first_source = min((time,word,source)
        for source,values in completed.items() for time,word in values)
    sink_records = list(struct.iter_unpack('<QB',slots))
    sink = bisect_left([time for time,slot in sink_records],first_time)
    if sink == len(sink_records) or sink_records[sink][0]-first_time > 84 or din[sink] != first_word:
        raise ValueError('startup completion lacks chronological DIN pair')
    report.update(first_completed_source=first_source,first_completed_slot=sink_records[sink][1],
                  frame_wait=frame_wait_din(events,din,slots),
                  frame_edges=len(frames),scope='Actual startup reset/write/load/completion identities; '
                  'does not decide whether the firmware sequence or emulated timing matches silicon')
    return report


def verify(directory):
    text = (directory/'error.log').read_text()
    terminal = rows(text,'mu4_playback_capture_terminal')
    lifecycle_end = rows(text,'mu4_tx_lifecycle_terminal')
    events = rows(text,'mu4_tx_lifecycle')
    if (len(terminal) != 1 or terminal[0]['closed'] != '1' or terminal[0]['illegal'] != '0'
            or Decimal(terminal[0]['t']) < Decimal('1.25') or terminal[0]['audio_acceptance'] != '0'
            or len(lifecycle_end) != 1 or not events
            or not len(events) == int(lifecycle_end[0]['events']) < 100000
            or lifecycle_end[0]['capped'] != '0' or lifecycle_end[0]['first'] != '1.000000000'
            or lifecycle_end[0]['last'] != '1.250000000'
            or lifecycle_end[0]['t'] != terminal[0]['t']):
        raise ValueError('missing, capped or incomplete bounded capture closure')
    metadata = parse_rows(text)
    streams,reports = {},{}
    for name,row in metadata.items():
        streams[name],reports[name] = validate_stream(row,(directory/FILES[name]).read_bytes())
    slots = (directory/'mu4_playback_din.slots').read_bytes()
    validate_slots(parse_slot_row(text,True),slots,len(streams['codec_din']))
    times = validate_tx_times(parse_tx_time_row(text,True),(directory/'mu4_playback_tx.times').read_bytes(),len(streams['mcbsp_tx']))
    raw_writes = (directory/'mu4_playback_dxr.writes').read_bytes()
    write_report = check_writes(parse_row(text),raw_writes,streams['dma_source'])
    writes = list(struct.iter_unpack('<QIHBB',raw_writes))
    lifecycle,completed = bind_lifecycle(events,writes,streams['mcbsp_tx'],times)
    # Keep the original whole-run verifier separate: this accepts bounded capture
    # integrity, not its twenty-second execution/title/audio-completion contract.
    serial = compare_serial_callbacks(streams['mcbsp_tx'],times,streams['codec_din'],slots)
    final = {}
    for row in rows(text,'mu4_filter_bus'):
        address = int(row['address'],16)
        if row['kind'] == 'write' and 0x1000 <= address <= 0x10ff:
            final[address] = Decimal(row['t']),int(row['word'],16)
    selected = {}
    for row in rows(text,'mu4_playback_dma_epoch'):
        address,word = int(row['address'],16),int(row['word'],16)
        if (address in final and row['last_write_pc'] == '02a2ea'
                and final[address] == (Decimal(row['last_write_t']),word)):
            source = int(row['read'])
            if (row['ownership'] != 'active_source_read' or row['channel'] != '3'
                    or row['last_write_mask'] != 'ffff' or not 1 <= source <= len(streams['dma_source'])
                    or streams['dma_source'][source-1] != word):
                raise ValueError('selected sample lacks actual source-stream identity')
            if address in selected:
                raise ValueError('a final filter word is consumed more than once in the selected epoch')
            selected[address] = source
    if not final or set(selected) != set(final) or len(set(selected.values())) != len(selected):
        raise ValueError('final filter sample block lacks exactly-once actual DMA coverage')
    sink_times = [value[0] for value in struct.iter_unpack('<QB',slots)]
    sink_slots = [value[1] for value in struct.iter_unpack('<QB',slots)]
    delivered,channel_slots = 0,Counter()
    for address,source in selected.items():
        completions = completed.get(source,[])
        if len(completions) != 1:
            raise ValueError('selected sample lacks exactly one observed serial completion')
        time,word = completions[0]
        sink = bisect_left(sink_times,time)
        if (sink == len(sink_times) or sink_times[sink]-time > 84
                or streams['codec_din'][sink] != word):
            raise ValueError('selected serial epoch lacks chronological codec DIN correspondence')
        delivered += 1
        channel_slots[f'bank_{address & ~63:04x}_slot_{sink_slots[sink]}'] += 1
    startup = startup_report(text,writes,streams['mcbsp_tx'],times,streams['codec_din'],slots)
    controls = codec_control_history(text)
    route = None
    if controls:
        first_selected = min(time for source in selected.values() for time,word in completed[source])
        snapshots = [registers for time,registers in controls if time <= first_selected]
        if not snapshots:
            raise ValueError('no codec control state before selected sample delivery')
        registers = snapshots[-1]
        route = dict(power=f'{registers[6]:03x}',analog=f'{registers[4]:03x}',digital=f'{registers[5]:03x}',
                     format=f'{registers[7]:03x}',active=bool(registers[9]&1),
                     dac_selected=bool(registers[4]&0x10),dac_power_down=bool(registers[6]&8),
                     output_power_down=bool(registers[6]&0x10),digital_dac_mute=bool(registers[5]&8),
                     headphone_mute_requested=[(registers[channel]&0x7f)==0 for channel in (2,3)],
                     analog_effect_validated=False)
    return dict(streams=reports,dma_to_dxr=write_report,lifecycle=lifecycle,serial_callbacks=serial,
                startup=startup,codec_route=route,
                final_filter_words=len(final),final_words_consumed=len(selected),final_words_at_din=delivered,
                selected_dma_read_range=[min(selected.values()),max(selected.values())],
                bounded_sample_to_din_epochs_validated=True,
                channel_slots=dict(channel_slots),native_audio_validated=False,
                scope='Bounded actual buffer loads/completions and timestamp-paired codec DIN; '
                      'not whole playback, unmatched-DIN pin ownership, codec analogue routing or GSM speech')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(verify(args.directory),indent=2))
    except (ValueError,KeyError,OSError,ArithmeticError) as error:
        print(f'FAIL: {error}',file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

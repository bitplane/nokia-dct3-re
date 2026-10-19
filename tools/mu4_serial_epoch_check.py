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
    kinds,unknown,completed_times = Counter(),0,set()
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
            if (owner != 4 or not source or owned.get(source) != (time,word)
                    or source in accepted or source in ignored):
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
            if time in completed_times or time not in tx_by_time or tx_by_time[time] != word:
                raise ValueError('lifecycle completion lacks exact completed-TX callback')
            completed_times.add(time)
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
    report = dict(events=sum(kinds.values()),kinds=dict(kinds),accepted=len(accepted),ignored=len(ignored),
                  discarded=len(discarded),endpoint_pending=sorted(pending),
                  discarded_source_ordinals=sorted(discarded),
                  source_epochs_completed=len(completed),unknown_prefix_completions=unknown,
                  internal_buffer_loads_observed=bool(kinds['load_fresh']+kinds['load_repeat']),
                  native_audio_validated=False)
    return report,completed


TX_EVENT_NAMES = ('reset','ignore','accept','overwrite','load_fresh','load_repeat',
                  'complete','restart','wait_frame')


def tx_event_rows(data):
    if len(data)%16:
        raise ValueError('truncated binary transmitter event')
    for ordinal,(when,word,event,owner,source) in enumerate(struct.iter_unpack('<QHBBI',data),1):
        if event >= len(TX_EVENT_NAMES) or owner > 6:
            raise ValueError('invalid binary transmitter event or owner')
        yield dict(ordinal=str(ordinal),kind=TX_EVENT_NAMES[event],word=f'{word:04x}',
                   ns=str(when),dma_owner=str(owner),dma_read=str(source),audio_acceptance='0')


def check_tx_event_capture(text,data):
    ends = rows(text,'mu4_tx_event_capture')
    if len(ends) != 1:
        raise ValueError('missing or repeated binary transmitter capture terminal')
    end = ends[0]
    if (end['capped'] != '0' or end['encoding'] != 'le64ns_u16word_u8event_u8owner_u32source'
            or end['file'] != 'mu4_playback_tx.events' or end['save_restore'] != 'unsupported'
            or end['observer_writes'] != '0' or end['audio_acceptance'] != '0'
            or not 0 < int(end['records']) <= 8_000_000
            or len(data) != int(end['bytes']) or len(data) != int(end['records'])*16):
        raise ValueError('incomplete, capped or changed binary transmitter capture')
    checksum = 2166136261
    for value in data:
        checksum = ((checksum ^ value)*16777619)&0xffffffff
    if f'{checksum:08x}' != end['fnv1a']:
        raise ValueError('binary transmitter capture hash mismatch')
    return {'records':int(end['records']),'fnv1a':end['fnv1a']}


def full_lifecycle_report(text,data,writes,tx,times,din,slots,stop_ns):
    integrity = check_tx_event_capture(text,data)
    report,_ = bind_lifecycle(tx_event_rows(data),writes,tx,times,
                              first_ns=0,last_ns=stop_ns)
    if (report['kinds'].get('complete',0) != len(times)
            or sum(report['kinds'].get(kind,0) for kind in ('accept','overwrite','ignore')) != len(writes)):
        raise ValueError('full event stream does not cover every captured TX/DXR word')
    serial = compare_serial_callbacks(tx,times,din,slots)
    if not serial['all_completed_tx_paired']:
        raise ValueError('full transmitter capture lacks exact chronological codec DIN pairs')
    report.update(capture=integrity,nonshifting_din=nonshifting_din(
        tx_event_rows(data),times,din,slots),
        all_completed_tx_paired=True,
        scope='Actual full-capture buffer transitions; not whole-track or silicon timing acceptance')
    return report


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


def codec_capture_horizon(text,slots,stop_ns):
    """Recovered active MU4 bench clock, not measured converter timing."""
    history = codec_control_history(text)
    if not history or not slots or len(slots)%9:
        raise ValueError('codec endpoint requires control history and complete DIN records')
    if any(when > stop_ns for when,registers in history):
        raise ValueError('codec control observation extends beyond capture closure')
    registers = history[-1][1]
    if (not registers[9]&1 or not registers[7]&0x40 or registers[6]&0xc0
            or registers[7]&0x5f != 0x53 or registers[8]&0x3f != 0x23):
        raise ValueError('codec endpoint is outside the qualified active master profile')
    last,slot = struct.unpack('<QB',slots[-9:])
    # USB/272 at the research machine's declared MCLK; CLKIN halves BCLK.
    numerator = 272*1_000_000_000*(2 if registers[8]&0x40 else 1)
    maximum_gap = (numerator+11_995_200-1)//11_995_200
    if slot not in (0,1) or not 0 <= stop_ns-last <= maximum_gap:
        raise ValueError('active codec DIN capture stops before the declared endpoint')
    return {'last_callback_ns':last,'stop_ns':stop_ns,'gap_ns':stop_ns-last,
            'maximum_gap_ns':maximum_gap,'declared_mclk_hz':11_995_200,
            'scope':'Active fixed-rate research bench endpoint; not physical clock calibration'}


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


def nonshifting_din(events,tx_times,din,slots,max_delay_ns=84):
    """Qualify held levels only after a known completion, never by zero matching."""
    if len(slots) != 9*len(din):
        raise ValueError('nonshifting DIN metadata extent mismatch')
    if not isinstance(max_delay_ns,int) or max_delay_ns < 0:
        raise ValueError('nonshifting callback bound must be nonnegative')
    intervals, pending, active, held = [], None, None, None
    previous = -1
    for row in events:
        when,kind = int(row['ns']),row['kind']
        if when < previous:
            raise ValueError('nonmonotonic transmitter observation')
        previous = when
        if kind in ('load_fresh','load_repeat','restart','reset'):
            if pending is not None:
                first,word,reason = pending
                intervals.append((first,when,word,reason))
                pending = None
            if kind == 'reset':
                # Reset does not publish a TX bit in this model. A partial
                # shift's last pin value is not recoverable from word events.
                if active is False and held is not None:
                    pending = (when,held,'reset')
                else:
                    held = None
                active = False
            else:
                active = True
        elif kind == 'complete':
            held,active = 0xffff if int(row['word'],16)&1 else 0,False
        elif kind == 'wait_frame':
            if active is not False or held is None or pending is not None:
                raise ValueError('frame wait lacks a unique completed-word pin predecessor')
            if held != (0xffff if int(row['word'],16)&1 else 0):
                raise ValueError('frame wait retained word disagrees with completed final bit')
            pending = (when,held,'wait_frame')
    sink_times = [when for when,slot in struct.iter_unpack('<QB',slots)]
    if any(a >= b for a,b in zip(sink_times,sink_times[1:])):
        raise ValueError('nonincreasing codec callback times')
    if any(a >= b for a,b in zip(tx_times,tx_times[1:])):
        raise ValueError('nonincreasing transmitter callback times')
    paired, cursor = set(),0
    for when in tx_times:
        while cursor < len(sink_times) and sink_times[cursor] < when:
            cursor += 1
        if cursor < len(sink_times) and sink_times[cursor]-when <= max_delay_ns:
            paired.add(cursor)
            cursor += 1
    starts = [first for first,last,word,kind in intervals]
    covered,uncovered = Counter(),[]
    for index,when in enumerate(sink_times):
        if index in paired:
            continue
        interval = bisect_left(starts,when+1)-1
        if interval < 0 or when >= intervals[interval][1]:
            uncovered.append(index)
            continue
        first,last,word,kind = intervals[interval]
        if din[index] != word:
            raise ValueError('unpaired codec word contradicts known held pin level')
        completion = bisect_left(tx_times,first)
        if completion < len(tx_times) and tx_times[completion] < last:
            raise ValueError('claimed nonshifting interval contains a TX completion')
        covered[kind] += 1
    return {'paired_words':len(paired),'unpaired_words':len(din)-len(paired),
            'covered_by_transition':dict(covered),'uncovered_words':len(uncovered),
            'first_uncovered_indices':uncovered[:8],
            'scope':'Bounded emulated completion/reset/wait ownership; not physical pin timing or native audio conformance'}


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
                  nonshifting_din=nonshifting_din(events,times,din,slots),
                  frame_edges=len(frames),scope='Actual startup reset/write/load/completion identities; '
                  'does not decide whether the firmware sequence or emulated timing matches silicon')
    return report


def verify(directory,require_full=False,transport_only=False):
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
    startup = startup_report(text,writes,streams['mcbsp_tx'],times,streams['codec_din'],slots)
    full = None
    if rows(text,'mu4_tx_event_capture'):
        raw_events = (directory/'mu4_playback_tx.events').read_bytes()
        full = full_lifecycle_report(text,raw_events,writes,streams['mcbsp_tx'],times,
            streams['codec_din'],slots,int(Decimal(terminal[0]['t'])*1_000_000_000))
        full['codec_capture_horizon'] = codec_capture_horizon(text,slots,
            int(Decimal(terminal[0]['t'])*1_000_000_000))
    elif require_full or transport_only:
        raise ValueError('full transmitter transition capture required')
    if transport_only:
        return dict(streams=reports,dma_to_dxr=write_report,lifecycle=lifecycle,
                    serial_callbacks=serial,startup=startup,full_capture_lifecycle=full,
                    bounded_sample_to_din_epochs_validated=False,native_audio_validated=False,
                    scope='Transport-only capture; no decoder/filter arithmetic or whole-track acceptance')
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
                startup=startup,full_capture_lifecycle=full,codec_route=route,
                final_filter_words=len(final),final_words_consumed=len(selected),final_words_at_din=delivered,
                selected_dma_read_range=[min(selected.values()),max(selected.values())],
                bounded_sample_to_din_epochs_validated=True,
                channel_slots=dict(channel_slots),native_audio_validated=False,
                scope='Bounded actual buffer loads/completions and timestamp-paired codec DIN; '
                      'not whole playback, physical pin timing, codec analogue routing or GSM speech')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--full',action='store_true',help='require full-capture binary buffer transitions')
    parser.add_argument('--transport-only',action='store_true',
                        help='require full transport capture without claiming selected filter provenance')
    args = parser.parse_args()
    try:
        print(json.dumps(verify(args.directory,args.full,args.transport_only),indent=2))
    except (ValueError,KeyError,OSError,ArithmeticError) as error:
        print(f'FAIL: {error}',file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

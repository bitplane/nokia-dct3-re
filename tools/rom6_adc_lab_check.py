"""Independently check raw ADC oscillator provenance; never qualify RF decoding."""
import math
import re
from functools import lru_cache
from tools.gsm_sch_signal import HYPERFRAME, sch_information_bits, sch_burst_bits, gmsk_burst_samples
from tools.gsm_xcch_signal import normal_control_bursts,broadcast_octets,broadcast_si_index,paging_fill_octets
from tools.rom6_timed_output_check import nanoseconds

WORD=re.compile(r'mfi_rx_word: sequence=(\d+) value=([0-9a-f]{4}) source_tick=(\d+) t=(\d+\.\d{9})')
ENABLE=re.compile(r'mfi_rx_enable: state=([01]) produced=(\d+) consumed=(\d+) overrun=(\d+) underrun=(\d+) t=\d+\.\d{9}')
SUMMARY=re.compile(r'mfi_rx_summary: produced=(\d+) consumed=(\d+) queued=(\d+) overrun=(\d+) underrun=(\d+) fault=([01]) divider=(\d+) physical_clock_claim=0')
EPOCH=re.compile(r'mfi_rx_epoch: first_tick=(\d+) divider=(\d+) physical_clock_claim=0')
ACTIVE_WORD=re.compile(r'rom6_burst_active_word: sequence=(\d+) read=(\d+) value=([0-9a-f]{4}) t=(\d+\.\d{9}) observer_writes=0')


def continuous_queue_bound(first_tick,divider,consumed,next_read_ns):
    """Counterfactual uninterrupted-producer demand, allowing log rounding."""
    if min(first_tick,consumed,next_read_ns)<0 or divider<=0:
        raise ValueError('invalid continuous queue bound arguments')
    def required(stamp):
        reference_tick=max(0,stamp)*13_000_000//1_000_000_000
        produced=max(0,(reference_tick-first_tick)//divider+1)
        return max(0,produced-consumed)
    return required(next_read_ns-1),required(next_read_ns+1)


def queued_read_count(text):
    active=False
    total=0
    events=list(ENABLE.finditer(text))
    summaries=list(SUMMARY.finditer(text))
    if (not events or len(events)!=text.count('mfi_rx_enable:') or
            len(summaries)!=1 or text.count('mfi_rx_summary:')!=1 or
            summaries[0].start()<events[-1].start()):
        raise ValueError('incomplete MFI activation/consumption accounting')
    for event in events:
        state,*values=map(int,event.groups())
        produced,consumed,overrun,underrun=values
        if consumed>produced or bool(state)==active:
            raise ValueError('invalid MFI activation/consumption transition')
        if state:
            if values!=[0,0,0,0]:
                raise ValueError('MFI activation does not start with cleared accounting')
        else:
            total+=consumed
        active=bool(state)
    produced,consumed,queued,overrun,underrun,_,_=map(int,summaries[0].groups())
    if consumed>produced or queued!=produced-consumed:
        raise ValueError('MFI final queue accounting differs')
    if active:
        total+=consumed
    elif any((produced,consumed,overrun,underrun)):
        raise ValueError('disabled MFI accounting changed')
    return total


@lru_cache(maxsize=16)
def sch_reference(frame):
    return gmsk_burst_samples(sch_burst_bits(sch_information_bits(frame,42)))


@lru_cache(maxsize=16)
def bcch_reference(burst,kind=2,combined_ccch=False):
    octets=broadcast_octets(kind,combined_ccch=combined_ccch)
    return gmsk_burst_samples(normal_control_bursts(octets)[burst])


@lru_cache(maxsize=4)
def paging_reference(burst):
    return gmsk_burst_samples(normal_control_bursts(paging_fill_octets())[burst])


def paging_burst(frame):
    position=frame%51
    for start in (6,12,16):
        if start<=position<start+4:return position-start
    return None


def reference_word(sequence,tick,tone,word_divider=6,burst=False,sch=False,bcch=False,broadcast=False,combined_ccch=False,paging=False):
    if not tone: return 0
    component=(sequence-1)&1
    conversion_tick=tick-word_divider*component # Q is latched with the preceding I.
    pch=paging_burst(conversion_tick//60000) if paging else None
    if pch is not None and conversion_tick%60000<7104:
        if word_divider!=12:raise ValueError('paging source requires twelve-tick word clock')
        return paging_reference(pch)[(conversion_tick%60000)//12][component]&0xffff
    if bcch and 2<=(conversion_tick//60000)%51<=5 and conversion_tick%60000<7104:
        if word_divider!=12:
            raise ValueError('BCCH fixture requires the declared twelve-tick word clock')
        kind=broadcast_si_index(conversion_tick//60000) if broadcast else 2
        return bcch_reference((conversion_tick//60000)%51-2,kind,combined_ccch)[(conversion_tick%60000)//12][component]&0xffff
    if sch and (conversion_tick//60000)%51 in (1,11,21,31,41) and conversion_tick%60000<7104:
        if word_divider!=12:
            raise ValueError('SCH fixture requires the declared twelve-tick word clock')
        return sch_reference((conversion_tick//60000)%HYPERFRAME)[(conversion_tick%60000)//12][component]&0xffff
    if burst and not ((conversion_tick//60000)%51 in (0,10,20,30,40) and conversion_tick%60000<7104):
        return 0
    phase=2*math.pi*((conversion_tick//12)%16)/16
    return round(1024*(math.sin(phase) if component else math.cos(phase)))&0xffff


def check_active_burst_window(text,sch=False,window='active',bcch=False,broadcast=False):
    """Join a later raw-read window to clock epochs, not read-time phase."""
    if window not in ('active','sch_raw'):
        raise ValueError('unknown raw sample observation window')
    prefix=f'rom6_burst_{window}'
    pattern=ACTIVE_WORD if window=='active' else re.compile(prefix+r'_word: sequence=(\d+) read=(\d+) value=([0-9a-f]{4}) t=(\d+\.\d{9}) observer_writes=0')
    rows=list(pattern.finditer(text))
    complete=re.findall(prefix+r'_complete: words=(\d+) cap=1024 reads=(\d+) observer_writes=0',text)
    totals=re.findall(r'rom6_adc_lab_complete: result_words=\d+ cap=32768 reads=(\d+) observer_writes=0',text)
    if (len(rows)!=1024 or len(rows)!=text.count(prefix+'_word:') or len(complete)!=1 or
            complete[0][0]!='1024' or len(totals)!=1 or complete[0][1]!=totals[0]):
        raise ValueError('incomplete active-burst raw-read window')
    active=False
    first_tick=None
    closed=0
    last_read=None
    active_words=inactive_words=epochs=0
    sch_words=sch_nonzero=0
    observed_epochs=set()
    ticks=[]
    events=sorted([(m.start(),'enable',m) for m in ENABLE.finditer(text)]+
                  [(m.start(),'epoch',m) for m in EPOCH.finditer(text)]+
                  [(m.start(),'word',m) for m in rows])
    for _,kind,event in events:
        if kind=='enable':
            state,produced,consumed,overrun,underrun=map(int,event.groups())
            if bool(state)==active or consumed>produced or overrun or underrun:
                raise ValueError('invalid active-window capture accounting')
            active=bool(state)
            first_tick=None
            if not active: closed+=consumed
            elif produced or consumed: raise ValueError('capture did not start with empty counters')
        elif kind=='epoch':
            if not active or first_tick is not None or event.group(2)!='12':
                raise ValueError('active-window source epoch differs')
            first_tick=int(event.group(1))
            epochs+=1
        else:
            sequence,read,value,stamp=event.groups()
            sequence,read,value=int(sequence),int(read),int(value,16)
            if not active or first_tick is None or read<=closed or sequence!=len(ticks)+1:
                raise ValueError('raw burst read has no owning capture epoch')
            if last_read is not None and read!=last_read+1:
                raise ValueError('active-burst raw window skips a read')
            local=read-closed
            tick=first_tick+(local-1)*12
            if value!=reference_word(local,tick,True,12,True,sch,bcch,broadcast,
                                     combined_ccch='npe3_bts_layout: combined_ccch=1 ' in text,
                                     paging='npe3_bts_pch:' in text):
                raise ValueError('active burst differs from independent clocked waveform')
            if tick*1_000_000_000>(nanoseconds(stamp)+1)*13_000_000:
                raise ValueError('active burst consumed before its conversion clock')
            if not ticks and not value: raise ValueError('active window did not begin on a nonzero word')
            conversion=tick-12*((local-1)&1)
            is_sch=sch and (conversion//60000)%51 in (1,11,21,31,41) and conversion%60000<7104
            sch_words+=int(is_sch)
            sch_nonzero+=int(is_sch and value!=0)
            on=(conversion//60000)%51 in (0,10,20,30,40) and conversion%60000<7104
            on=on or (sch and (conversion//60000)%51 in (1,11,21,31,41) and conversion%60000<7104)
            on=on or (bcch and 2<=(conversion//60000)%51<=5 and conversion%60000<7104)
            on=on or ('npe3_bts_pch:' in text and paging_burst(conversion//60000) is not None and conversion%60000<7104)
            active_words+=int(on)
            inactive_words+=int(not on)
            ticks.append(tick)
            observed_epochs.add(epochs)
            last_read=read
    if not active_words or not inactive_words or last_read>int(totals[0]):
        raise ValueError('active window does not cover both tone and off-slot samples')
    if window=='sch_raw' and not sch_nonzero:
        raise ValueError('SCH observation window contains no delivered coded waveform')
    return dict(checked_words=len(rows),first_read=int(rows[0].group(2)),last_read=last_read,
                first_source_tick=ticks[0],last_source_tick=ticks[-1],
                tone_words=active_words,off_slot_words=inactive_words,sch_words=sch_words,sch_nonzero_words=sch_nonzero,
                source_epochs=len(observed_epochs),fcch_detection_qualified=False,sch_qualified=False)


def check(text,tone,burst=False,sch=False,bcch=False,broadcast=False):
    combined_ccch='npe3_bts_layout: combined_ccch=1 ' in text
    paging='npe3_bts_pch:' in text
    if paging:
        header='npe3_bts_pch: starts=6,12,16 octets='+paging_fill_octets().hex()+' tsc=2 no_identity=1 raw_adc_only=1 physical_claim=0'
        if not combined_ccch or not broadcast or text.count(header)!=1 or text.count('npe3_bts_pch:')!=1:
            raise ValueError('idle paging source declaration differs')
    headers=re.findall(r'rf_adc_lab: direct_adc=1 tuned_air_source=0 tone=([01]) reference=13000000 source_phase_divider=12 word_divider=(6|12) amplitude=1024 physical_clock_claim=0',text)
    if len(headers)!=1 or text.count('rf_adc_lab:')!=1 or headers[0][0]!=str(int(tone)):
        raise ValueError('missing explicit ADC-pin laboratory source')
    divider=int(headers[0][1])
    burst_header='rf_adc_burst: frame_ticks=60000 multiframe=51 frames=0,10,20,30,40 active_ticks=7104 epoch=0 envelope=rectangular other_slots=zero tuned_air_source=0'
    if text.count('rf_adc_burst:')!=int(burst and not sch) or burst and not sch and text.count(burst_header)!=1:
        raise ValueError('finite-burst ADC source declaration differs')
    sync_header='rf_adc_sync: frame_ticks=60000 multiframe=51 fcch_frames=0,10,20,30,40 sch_frames=1,11,21,31,41 active_ticks=7104 epoch=0 envelope=rectangular bt=0.3 bsic=42 waveform_samples_per_symbol=4 other_slots='+('bcch' if bcch else 'zero')+' tuned_air_source=0'
    bcch_header='rf_adc_bcch: frames=2,3,4,5 tsc=2 octets=49061b000100f11000014000000000000000002b2b2b2b repeated_si3_only=1 epoch=0 physical_claim=0'
    if broadcast:
        bcch_header='rf_adc_bcch: frames=2,3,4,5 tsc=2 schedule=0,1,2,3,1,1,2,3 repeated_si3_only=0 arfcn=35 epoch=0 physical_claim=0'
        declarations=re.findall(r'rf_adc_broadcast_si: index=([0-3]) arfcn=35 octets=([0-9a-f]{46}) physical_claim=0',text)
        if not bcch or declarations!=[(str(kind),broadcast_octets(kind,combined_ccch=combined_ccch).hex()) for kind in range(4)] or text.count('rf_adc_broadcast_si:')!=4:
            raise ValueError('broadcast data differs from independent single-cell reference')
    elif 'rf_adc_broadcast_si:' in text:
        raise ValueError('unexpected full broadcast source')
    if text.count('rf_adc_bcch:')!=int(bcch) or bcch and (not sch or text.count(bcch_header)!=1):
        raise ValueError('BCCH source declaration differs')
    if text.count('rf_adc_sync:')!=int(sch) or sch and (not burst or divider!=12 or text.count(sync_header)!=1):
        raise ValueError('coded SCH source declaration differs')
    summaries=re.findall(r'mfi_rx_summary: [^\r\n]*\bdivider=(\d+) physical_clock_claim=0',text)
    if summaries!=[str(divider)]: raise ValueError('MFI word clock differs from declared fixture')
    totals=re.findall(r'rom6_adc_lab_complete: result_words=(\d+) cap=32768 reads=(\d+) observer_writes=0',text)
    native=re.findall(r'rom4_interface_summary: [^\r\n]*\brf_reads=(\d+)\b',text)
    if len(totals)!=1 or len(native)!=1 or totals[0][1]!=native[0] or int(totals[0][0])>=32768:
        raise ValueError('independent P27 read accounting differs or result capture capped')
    consumed=queued_read_count(text)
    if consumed!=int(native[0]):
        raise ValueError('P27 reads include words not consumed from the clocked queue')
    capacities=re.findall(r'mfi_rx_capacity_observation: configured=(\d+) peak=(\d+) physical_capacity_claim=0',text)
    if len(capacities)!=1 or text.count('mfi_rx_capacity_observation:')!=1:
        raise ValueError('missing provisional staging capacity accounting')
    capacity,peak=map(int,capacities[0])
    if capacity<32 or capacity%32 or peak>capacity:
        raise ValueError('invalid provisional staging capacity accounting')
    words=list(WORD.finditer(text))
    if len(words)<32 or len(words)!=text.count('mfi_rx_word:'):
        raise ValueError('missing or malformed delivered waveform prefix')
    trace_totals=re.findall(r'mfi_rx_trace_complete: words=(\d+) blocks=(\d+) word_cap=4096 block_cap=128',text)
    if (len(trace_totals)!=1 or trace_totals[0][0]!=str(len(words)) or len(words)>4096 or
            int(trace_totals[0][1])!=text.count('mfi_rx_block:') or int(trace_totals[0][1])>128):
        raise ValueError('MFI bounded trace accounting differs')
    first_reads=re.findall(r'rom6_adc_first_read: t=(\d+\.\d{9}) cpu_pc=[0-9a-f]{6} observer_writes=0',text)
    if len(first_reads)!=1 or text.count('rom6_adc_first_read:')!=1 or first_reads[0]!=words[0].group(4):
        raise ValueError('first P27 read is not the first clocked MFI word')
    last_sequence=last_tick=None
    epochs=0
    for word in words:
        sequence,value,tick,time=word.groups()
        sequence,tick=int(sequence),int(tick)
        if sequence==1:
            epochs+=1; last_sequence=last_tick=None
        if last_sequence is None and sequence!=1:
            raise ValueError('delivered waveform epoch is missing its first word')
        if (last_sequence is not None and (sequence!=last_sequence+1 or tick!=last_tick+divider)):
            raise ValueError('delivered waveform prefix skips a source clock')
        if int(value,16)!=reference_word(sequence,tick,tone,divider,burst,sch,bcch,broadcast,combined_ccch,paging):
            raise ValueError('ADC waveform differs from independent oscillator reference')
        # Log timestamps have nanosecond precision; the component gate checks
        # exact attoseconds independently. This is only a formatting allowance.
        if tick*1_000_000_000>(nanoseconds(time)+1)*13_000_000:
            raise ValueError('MFI consumed a word before its source transfer clock')
        last_sequence,last_tick=sequence,tick
    faults=re.findall(r'mfi_rx_fault: [^\r\n]+',text)
    contexts=re.findall(r'rf_adc_lab_fault_context: cpu_pc=([0-9a-f]{6}) st1=([0-9a-f]{4}) imr=([0-9a-f]{4}) ifr=([0-9a-f]{4}) observer_writes=0 t=(\d+\.\d{9})',text)
    if len(contexts)!=len(faults) or len(contexts)!=text.count('rf_adc_lab_fault_context:'):
        raise ValueError('missing or malformed passive fault context')
    fault_times=re.findall(r'mfi_rx_fault: [^\r\n]*\bt=(\d+\.\d{9})',text)
    if len(fault_times)!=len(faults) or [row[-1] for row in contexts]!=fault_times:
        raise ValueError('fault context is not on the matching transport event')
    return dict(direct_adc_only=True,tone=tone,finite_fcch_bursts=burst,coded_sch_source=sch,coded_bcch_source=bcch,full_broadcast_source=broadcast,word_divider=divider,
                provisional_staging_words=capacity,peak_queued_words=peak,physical_buffer_capacity_qualified=False,
                checked_prefix_words=len(words),
                total_p27_reads=int(native[0]),independent_read_count_matches=True,
                no_pre_capture_p27_read=True,all_reads_from_clocked_queue=True,
                observed_prefix_epochs=epochs,transport_faults=faults,
                fault_cpu_contexts=[dict(zip(('bus_context_pc','st1','imr','ifr','time'),row)) for row in contexts],
                transport_fault_free=not faults,physical_clock_qualified=False,
                tuned_rf_qualified=False,decoder_qualified=False,camping_qualified=False)

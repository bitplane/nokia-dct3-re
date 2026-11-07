"""Independently check raw ADC oscillator provenance; never qualify RF decoding."""
import math
import re
from tools.rom6_timed_output_check import nanoseconds

WORD=re.compile(r'mfi_rx_word: sequence=(\d+) value=([0-9a-f]{4}) source_tick=(\d+) t=(\d+\.\d{9})')
ENABLE=re.compile(r'mfi_rx_enable: state=([01]) produced=(\d+) consumed=(\d+) overrun=(\d+) underrun=(\d+) t=\d+\.\d{9}')
SUMMARY=re.compile(r'mfi_rx_summary: produced=(\d+) consumed=(\d+) queued=(\d+) overrun=(\d+) underrun=(\d+) fault=([01]) divider=(\d+) physical_clock_claim=0')


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


def reference_word(sequence,tick,tone,word_divider=6):
    if not tone: return 0
    component=(sequence-1)&1
    conversion_tick=tick-word_divider*component # Q is latched with the preceding I.
    phase=2*math.pi*((conversion_tick//12)%16)/16
    return round(1024*(math.sin(phase) if component else math.cos(phase)))&0xffff


def check(text,tone):
    headers=re.findall(r'rf_adc_lab: direct_adc=1 tuned_air_source=0 tone=([01]) reference=13000000 source_phase_divider=12 word_divider=(6|12) amplitude=1024 physical_clock_claim=0',text)
    if len(headers)!=1 or text.count('rf_adc_lab:')!=1 or headers[0][0]!=str(int(tone)):
        raise ValueError('missing explicit ADC-pin laboratory source')
    divider=int(headers[0][1])
    summaries=re.findall(r'mfi_rx_summary: [^\r\n]*\bdivider=(\d+) physical_clock_claim=0',text)
    if summaries!=[str(divider)]: raise ValueError('MFI word clock differs from declared fixture')
    totals=re.findall(r'rom6_adc_lab_complete: result_words=(\d+) cap=32768 reads=(\d+) observer_writes=0',text)
    native=re.findall(r'rom4_interface_summary: [^\r\n]*\brf_reads=(\d+)\b',text)
    if len(totals)!=1 or len(native)!=1 or totals[0][1]!=native[0] or int(totals[0][0])>=32768:
        raise ValueError('independent P27 read accounting differs or result capture capped')
    consumed=queued_read_count(text)
    if consumed!=int(native[0]):
        raise ValueError('P27 reads include words not consumed from the clocked queue')
    words=list(WORD.finditer(text))
    if len(words)<32 or len(words)!=text.count('mfi_rx_word:'):
        raise ValueError('missing or malformed delivered waveform prefix')
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
        if int(value,16)!=reference_word(sequence,tick,tone,divider):
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
    return dict(direct_adc_only=True,tone=tone,word_divider=divider,checked_prefix_words=len(words),
                total_p27_reads=int(native[0]),independent_read_count_matches=True,
                no_pre_capture_p27_read=True,all_reads_from_clocked_queue=True,
                observed_prefix_epochs=epochs,transport_faults=faults,
                fault_cpu_contexts=[dict(zip(('bus_context_pc','st1','imr','ifr','time'),row)) for row in contexts],
                transport_fault_free=not faults,physical_clock_qualified=False,
                tuned_rf_qualified=False,decoder_qualified=False,camping_qualified=False)

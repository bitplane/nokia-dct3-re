"""Check native complex correlation products, not an FCCH success verdict."""
import hashlib
import re

from tools.rom6_adc_filter_check import check_filter_program,signed
from tools.rom6_timed_output_check import nanoseconds

INPUT=re.compile(r'rom6_detect_product_input: phase=(\d+) mode=([0-9a-f]{4}) values=([0-9a-f]{4}(?:,[0-9a-f]{4}){3}) st1=([0-9a-f]{4}) observer_writes=0')
OUTPUT=re.compile(r'rom6_detect_product_output: phase=(\d+) a=([0-9a-f]{10}) b=([0-9a-f]{10}) st1=([0-9a-f]{4}) observer_writes=0')
METRIC=re.compile(r'rom6_detect_metric: sequence=(\d+) a=([0-9a-f]{10}) corr=([0-9a-f]{4}(?:,[0-9a-f]{4}){3}) energy=([0-9a-f]{4},[0-9a-f]{4}) thresholds=([0-9a-f]{4},[0-9a-f]{4}) mode=([0-9a-f]{4}) st1=([0-9a-f]{4}) observer_writes=0')
THRESHOLD=re.compile(r'rom6_detect_threshold: sequence=(\d+) t=(\d+\.\d{9}) value=([0-9a-f]{4}) a=([0-9a-f]{10}) cpu_pc=000b9a observer_writes=0')
FCCH_STATE=re.compile(r'rom6_burst_fcch_state: sequence=(\d+) t=(\d+\.\d{9}) address=([0-9a-f]{4}) value=([0-9a-f]{4}) pc=([0-9a-f]{6}) reads=(\d+) observer_writes=0')
FCCH_ACCUMULATE=re.compile(r'rom6_burst_fcch_accumulate: t=(\d+\.\d{9}) previous=([0-9a-f]{4}) value=([0-9a-f]{4}) a=([0-9a-f]{10}) b=([0-9a-f]{10}) st1=([0-9a-f]{4}) peak=([0-9a-f]{4}) base=([0-9a-f]{4}) index=([0-9a-f]{4}) pc=000d0c observer_writes=0')
PEAK=re.compile(r'rom6_burst_peak: sequence=(\d+) t=(\d+\.\d{9}) index=([0-9a-f]{4}) reads=(\d+) mode=([0-9a-f]{4}) producer=([0-9a-f]{4}) consumer=([0-9a-f]{4}) pc=([0-9a-f]{6}) observer_writes=0')


def check_peak_clock(text):
    """Join bounded peak indices to delivered source clocks, not CPU read phase."""
    from tools.rom6_adc_lab_check import ENABLE,EPOCH
    rows=list(PEAK.finditer(text))
    totals=re.findall(r'rom6_burst_peak_complete: writes=(\d+) cap=128 observer_writes=0',text)
    if (len(totals)!=1 or len(rows)!=text.count('rom6_burst_peak:') or
            not rows or len(rows)!=min(int(totals[0]),128)):
        raise ValueError('incomplete bounded native peak observations')
    events=sorted([(m.start(),'enable',m) for m in ENABLE.finditer(text)]+
                  [(m.start(),'epoch',m) for m in EPOCH.finditer(text)]+
                  [(m.start(),'peak',m) for m in rows])
    closed=0
    active=False
    epoch=None
    joined=[]
    for _,kind,row in events:
        if kind=='enable':
            state,produced,consumed,overrun,underrun=map(int,row.groups())
            if bool(state)==active or overrun or underrun or consumed>produced:
                raise ValueError('invalid peak-clock capture transition')
            active=bool(state)
            epoch=None
            if not active: closed+=consumed
        elif kind=='epoch':
            if not active or epoch is not None or row[2]!='12':
                raise ValueError('native peak source epoch differs')
            epoch=int(row[1])
        else:
            sequence,stamp,index,reads,mode,producer,consumer,pc=row.groups()
            index,reads,mode=int(index,16),int(reads),int(mode,16)
            local=reads-closed
            if (not active or epoch is None or local<=0 or local%32 or
                    index!=local//32+1 or int(sequence)!=len(joined)+1 or mode not in (0,1) or
                    pc not in ('000bab','000ced')):
                raise ValueError('native peak index differs from consumed 32-word blocks')
            tick=epoch+(local-1)*12
            if tick*1_000_000_000>(nanoseconds(stamp)+1)*13_000_000:
                raise ValueError('native peak precedes its source samples')
            joined.append((tick,mode))
    return dict(checked_peaks=len(joined),observed_peak_writes=int(totals[0]),
                first_last_source_ticks=[joined[0][0],joined[-1][0]],
                modes=sorted({row[1] for row in joined}),words_per_index=32,index_bias=1,
                physical_capture_qualified=False,sch_qualified=False,camping_qualified=False)


def check_finite_peak_accumulation(text,program,mcu,*,product='8210'):
    """Join table contents and signed residuals, not a physical phase verdict."""
    check_consumer_program(program,mcu,product=product)
    table=[int.from_bytes(program[2*at:2*at+2],'big') for at in range(0xd76,0xd8a)]
    # Twenty successive FCCH offsets: five per 51-frame multiframe. The
    # firmware represents time in 32-quarter-symbol units. Do not impose a
    # nearest-rounding policy: its sixteenth entry differs by 3/4 of a unit.
    for number,value in enumerate(table,1):
        frames=(number//5)*51+(number%5)*10
        if abs(value*32-frames*5000)>32:
            raise ValueError('native FCCH timing table differs from GSM frame spacing')
    rows=list(FCCH_ACCUMULATE.finditer(text))
    if not rows or len(rows)!=text.count('rom6_burst_fcch_accumulate:'):
        raise ValueError('missing or unsupported native FCCH residual observations')
    previous=None
    last_time=-1
    for row in rows:
        stamp,old,value,a,b,st1,peak,base,index=row.groups()
        old,value,a,b,st1,peak,base,index=[int(word,16) for word in (old,value,a,b,st1,peak,base,index)]
        address=0xd89+signed(index)
        if not 0xd76<=address<0xd8a or nanoseconds(stamp)<last_time:
            raise ValueError('native FCCH table selection/order differs')
        expected_table=table[address-0xd76]
        expected=(signed(old)+peak-base-expected_table)&((1<<40)-1)
        if (a!=expected_table or b!=expected or value!=(expected&0xffff) or
                previous is not None and old not in (0,previous)):
            raise ValueError('native FCCH residual differs from independent table arithmetic')
        previous=value
        last_time=nanoseconds(stamp)
    return dict(checked_updates=len(rows),table_entries=len(table),last_residual=signed(previous),
                mapped_table_read_qualified=True,physical_frame_phase_qualified=False,
                sch_qualified=False,camping_qualified=False)


def check_finite_search_acceptance(text,program,mcu,*,product='8210'):
    """Observe native control acceptance, not independently decoded FCCH timing."""
    check_consumer_program(program,mcu,product=product)
    rows=list(FCCH_STATE.finditer(text))
    totals=re.findall(r'rom6_burst_fcch_complete: writes=(\d+) cap=256 observer_writes=0',text)
    if (not rows or len(rows)!=text.count('rom6_burst_fcch_state:') or
            totals!=[str(len(rows))] or len(rows)>256):
        raise ValueError('incomplete finite FCCH state observations')
    updates=[]
    accepted=None
    last_time=last_read=-1
    for number,row in enumerate(rows,1):
        sequence,stamp,address,value,pc,reads=row.groups()
        if int(sequence)!=number:
            raise ValueError('finite FCCH state ordering differs')
        time,read=nanoseconds(stamp),int(reads)
        if time<last_time or read<last_read:
            raise ValueError('finite FCCH state chronology differs')
        last_time,last_read=time,read
        if address=='2f65':
            value=int(value,16)
            if accepted is not None or pc!='000d27' or updates and value!=((updates[-1][0]+1)&0xffff):
                raise ValueError('finite FCCH confirmation counter differs')
            tick=nanoseconds(stamp)*13_000_000//1_000_000_000
            if tick//60000%51 not in (0,10,20,30,40) or not 7104<=tick%60000<9000:
                raise ValueError('FCCH confirmations no longer follow scheduled tone bursts')
            updates.append((value,stamp))
        if address=='2627':
            if pc!='000d60' or int(value,16)&0x80==0 or accepted is not None:
                raise ValueError('native finite-search acceptance flag differs')
            if len(updates)<6 or updates[-1][0]!=0:
                raise ValueError('search acceptance preceded its confirmation sequence')
            accepted=stamp
    if accepted is None:
        raise ValueError('native finite-search acceptance path not observed')
    return dict(native_acceptance_observed=True,confirmation_updates=len(updates),
                acceptance_time=accepted,source_burst_alignment_observed=True,
                physical_capture_qualified=False,fcch_timing_qualified=False,
                sch_qualified=False,camping_qualified=False)


def check_consumer_program(program,mcu,*,product='8210'):
    block=program[0xb40*2:0xd80*2]
    offset={'8210':0x1170b0,'6210':0x20de4}[product]
    if (hashlib.sha256(block).hexdigest()!='245f696206a75730ab96ea480d5927b1b194533947d8fb732f057d4dbb3ecfb0' or
            mcu[offset:offset+len(block)]!=block):
        raise ValueError('search consumer is not the pinned own MCU upload')


def check_threshold_reads(text):
    rows=list(THRESHOLD.finditer(text))
    totals=re.findall(r'rom6_detect_threshold_complete: reads=(\d+) cap=32 observer_writes=0',text)
    if len(totals)!=1 or int(totals[0])<32 or len(rows)!=32 or text.count('rom6_detect_threshold:')!=32:
        raise ValueError('missing/malformed native search threshold reads')
    previous_time=-1
    for number,row in enumerate(rows,1):
        if int(row[1])!=number or float(row[2])<previous_time:
            raise ValueError('native search threshold sequence/time differs')
        previous_time=float(row[2])
    events=sorted([(row.start(),'read',row) for row in rows]+
                  [(row.start(),'metric',row) for row in METRIC.finditer(text)])
    pending=None
    joined=above=0
    for _,kind,row in events:
        if kind=='metric':
            if pending is not None: raise ValueError('unconsumed native search metric')
            pending=int(row[2],16)&0xffff
        else:
            sequence,time,threshold,a=row.groups()
            value=int(a,16)
            if int(sequence)!=joined+1 and pending is not None:
                raise ValueError('native search metric/read order differs')
            if threshold!='0574': raise ValueError('native uploaded search threshold differs')
            if pending is not None:
                if value!=pending: raise ValueError('native metric/consumer join differs')
                joined+=1; above+=value>int(threshold,16); pending=None
    if joined!=16 or pending is not None: raise ValueError('native search metric joins incomplete')
    return dict(total_reads=int(totals[0]),captured_reads=32,joined_metrics=joined,
                joined_above_threshold=above,threshold=1396,
                scope='first sixteen metric returns joined to actual uploaded threshold reads',
                fcch_qualified=False)


def complex_products(words):
    x0,y0,x1,y1=map(signed,words)
    mask=(1<<40)-1
    return (x0*x1+y0*y1)&mask,(x1*y0-y1*x0)&mask


def check_mode1_products(text):
    """Check paired mode-1 products and delayed history copies, not SCH success."""
    inputs=re.compile(r'rom6_mode1_product_input: phase=(\d+) values=([0-9a-f]{4}(?:,[0-9a-f]{4}){3}) st1=([0-9a-f]{4}) observer_writes=0')
    outputs=re.compile(r'rom6_mode1_product_output: phase=(\d+) a=([0-9a-f]{10}) b=([0-9a-f]{10}) ar2=([0-9a-f]{4}) ar3=([0-9a-f]{4}) word2f51=([0-9a-f]{4}) words=([0-9a-f]{4}(?:,[0-9a-f]{4}){3}) st1=([0-9a-f]{4}) observer_writes=0')
    events=[]
    for expression,kind in ((inputs,'input'),(outputs,'output')):
        rows=list(expression.finditer(text))
        if len(rows)!=50 or len(rows)!=text.count('rom6_mode1_product_'+kind+':'):
            raise ValueError('incomplete mode-1 product observations')
        events.extend((row.start(),kind,row) for row in rows)
    if re.findall(r'rom6_mode1_complete: phases=(\d+) cap=100 observer_writes=0',text)!=['100']:
        raise ValueError('incomplete mode-1 product window')
    active=None
    count=0
    distinct=set()
    for _,kind,row in sorted(events):
        if kind=='input':
            phase,words,st1=row.groups()
            if active is not None or int(phase)!=2*count+1 or int(st1,16)&0x340!=0x100:
                raise ValueError('mode-1 product ownership/status differs')
            active=[int(word,16) for word in words.split(',')]
        else:
            phase,a,b,ar2,ar3,pointer,words,st1=row.groups()
            history=[int(word,16) for word in words.split(',')]
            actual=int(a,16),int(b,16)
            if (active is None or int(phase)!=2*count+1 or
                    actual!=complex_products(active) or history!=active[2:]*2 or
                    ar3!='2f54' or int(st1,16)&0x340!=0x100):
                raise ValueError('mode-1 complex arithmetic/history differs')
            distinct.add(actual)
            count+=1
            active=None
    if active is not None or len(distinct)<4:
        raise ValueError('unfinished or insufficiently varied mode-1 window')
    return dict(products=count,distinct_products=len(distinct),history_copy_qualified=True,
                normalized_metric_qualified=False,sch_qualified=False,camping_qualified=False)


def normalized_metric(correlation,energy,mode=0):
    """Independent integer ratio for the observed nonsaturating domain."""
    if mode not in (0,1): raise ValueError('unsupported detector mode')
    if not 0<=energy<0x80000000: raise ValueError('unsupported metric energy domain')
    shift=31-energy.bit_length() if energy else 0
    def high17(value):
        field=((value<<shift)>>25)&0x1ffff
        return field-0x20000 if field&0x10000 else field
    denominator=(high17(energy)**2)&0xffff
    denominator=max(1,denominator)
    if denominator>=0x8000: raise ValueError('unsupported signed metric divisor')
    # Both delayed branches execute their shift/square slots. Mode 1 skips
    # only each following two-bit scale, not the squaring operation.
    numerator=sum(high17(value)**2 for value in correlation)<<(8 if mode else 10)
    quotient,remainder=divmod(numerator,2*denominator)
    if quotient>=0x8000: raise ValueError('unsupported metric quotient range')
    # Fifteen conditional division steps retain a scaled remainder above the
    # low-word quotient. Compare the full native accumulator, not just Q.
    return (remainder<<15)|quotient


def check_mode1_metrics(text):
    expression=re.compile(r'rom6_mode1_metric: sequence=(\d+) a=([0-9a-f]{10}) corr=([0-9a-f]{4}(?:,[0-9a-f]{4}){3}) energy=([0-9a-f]{4},[0-9a-f]{4}) mode=0001 st1=([0-9a-f]{4}) observer_writes=0')
    rows=list(expression.finditer(text))
    if (len(rows)!=16 or text.count('rom6_mode1_metric:')!=16 or
            re.findall(r'rom6_mode1_metric_complete: calls=(\d+) cap=16 observer_writes=0',text)!=['16']):
        raise ValueError('incomplete mode-1 metric observations')
    def long_words(words):
        high,low=(int(word,16) for word in words)
        value=(high<<16)|low
        return value-(1<<32) if value&0x80000000 else value
    for number,row in enumerate(rows,1):
        sequence,a,corr,energy,st1=row.groups()
        words=corr.split(',')
        expected=normalized_metric((long_words(words[:2]),long_words(words[2:])),
                                   long_words(energy.split(',')),1)
        if int(sequence)!=number or int(st1,16)&0x340!=0x100 or int(a,16)!=expected:
            raise ValueError('native mode-1 metric arithmetic/order differs')
    return dict(normalized_metrics=len(rows),mode=1,sch_qualified=False,camping_qualified=False)


def check(text,program,mcu,resident):
    check_filter_program(program,mcu,resident)
    check_consumer_program(program,mcu)
    if (program[0x4146*2:0x423b*2]!=resident[0x4146*2:0x423b*2] or
            program[0x604*2:0x605*2]!=bytes.fromhex('fc00')):
        raise ValueError('native correlation code/entry hook differs')
    totals=re.findall(r'rom6_detect_product_complete: phases=(\d+) cap=64 observer_writes=0',text)
    if totals!=['34']: raise ValueError('incomplete initial seventeen-product native window')
    events=[]
    for expression,kind in ((INPUT,'input'),(OUTPUT,'output')):
        rows=list(expression.finditer(text))
        if len(rows)!=17 or len(rows)!=text.count(expression.pattern.split(':',1)[0]+':'):
            raise ValueError('missing/malformed native complex-product '+kind)
        events.extend((row.start(),kind,row) for row in rows)
    active=None
    count=0
    values=set()
    for _,kind,row in sorted(events):
        if kind=='input':
            phase,mode,words,st1=row.groups()
            if active is not None or int(phase)!=2*count+1 or mode!='0000' or int(st1,16)&0x340!=0x100:
                raise ValueError('native complex-product input ownership/mode differs')
            active=complex_products([int(word,16) for word in words.split(',')])
        else:
            phase,a,b,st1=row.groups()
            actual=int(a,16),int(b,16)
            if (active is None or int(phase)!=2*count+1 or actual!=active or
                    int(st1,16)&0x340!=0x100):
                raise ValueError('native complex-product arithmetic/order differs')
            values.add(actual); count+=1; active=None
    if active is not None: raise ValueError('unfinished native complex product')
    metrics=list(METRIC.finditer(text))
    totals=re.findall(r'rom6_detect_metric_complete: calls=(\d+) cap=16 observer_writes=0',text)
    if totals!=['16'] or len(metrics)!=16 or text.count('rom6_detect_metric:')!=16:
        raise ValueError('missing/malformed native normalized-metric prefix')
    metric_values=set()
    for number,row in enumerate(metrics,1):
        sequence,a,corr,energy,thresholds,mode,st1=row.groups()
        def long_words(words):
            high,low=(int(word,16) for word in words)
            value=(high<<16)|low
            return value-(1<<32) if value&0x80000000 else value
        words=corr.split(',')
        expected=normalized_metric((long_words(words[:2]),long_words(words[2:])),long_words(energy.split(',')))
        if (int(sequence)!=number or mode!='0000' or int(st1,16)&0x340!=0x100 or
                int(a,16)!=expected):
            raise ValueError('native normalized detector metric differs')
        metric_values.add((expected&0xffff,thresholds))
    return dict(products=count,values=[dict(dot=a,cross=b) for a,b in sorted(values)],
                normalized_metrics=len(metrics),metric_values=[dict(value=value,thresholds=thresholds) for value,thresholds in sorted(metric_values)],
                consumer=check_threshold_reads(text),
                scope='initial seventeen products, native mode0 only',
                normalized_detector_metric_qualified=True,fcch_qualified=False,camping_qualified=False)

"""Check native raw log-power arithmetic, not final RF calibration or replies."""
import math
import re
import struct

from tools.rom6_adc_filter_check import COEFFICIENTS,check_filter_program,signed

MASK40=(1<<40)-1
INPUT=re.compile(r'rom6_measure_input: sequence=(\d+) a=([0-9a-f]{10}) ar2=([0-9a-f]{4}) calibration=([0-9a-f]{4}) flags3ec8=([0-9a-f]{4}) flags263b=([0-9a-f]{4}) st1=([0-9a-f]{4}) observer_writes=0')
OUTPUT=re.compile(r'rom6_measure_output: sequence=(\d+) a=([0-9a-f]{10}) b=([0-9a-f]{10}) ar2=([0-9a-f]{4}) calibration=([0-9a-f]{4}) st1=([0-9a-f]{4}) observer_writes=0')
RETURN=re.compile(r'rom6_measure_rssi_return: sequence=(\d+) b=([0-9a-f]{10}) observer_writes=0')
EPOCH=re.compile(r'mfi_rx_epoch: first_tick=(\d+) divider=12 physical_clock_claim=0')
RESULT=re.compile(r'rom6_adc_result_word: sequence=(\d+) t=\d+\.\d{9} value=([0-9a-f]{4}) mode022c=([0-9a-f]{4}) work022a=([0-9a-f]{4}) cpu_pc=([0-9a-f]{6}) mask=ffff observer_writes=0')
CORRECT_INPUT=re.compile(r'rom6_measure_correct_input: sequence=(\d+) key=([0-9a-f]{4}) pointer=([0-9a-f]{4}) raw=([0-9a-f]{4}) flags=([0-9a-f]{4}) main=([0-9a-f]{4}(?:,[0-9a-f]{4}){3}) other=([0-9a-f]{4}(?:,[0-9a-f]{4}){3}) st1=([0-9a-f]{4}) observer_writes=0')
CORRECT_OUTPUT=re.compile(r'rom6_measure_correct_output: sequence=(\d+) pointer=([0-9a-f]{4}) value=([0-9a-f]{4}) st1=([0-9a-f]{4}) observer_writes=0')
RX_COMMIT=re.compile(r'rom6_measure_rx_commit: sequence=(\d+) t=(\d+\.\d{9}) previous=([0-9a-f]{4}) producer=([0-9a-f]{4}) consumer=([0-9a-f]{4}) words=([0-9a-f]{4}(?:,[0-9a-f]{4})*) mask=ffff cpu_pc=([0-9a-f]{6}) observer_writes=0')


def signed40(value):
    return value-(1<<40) if value&(1<<39) else value


def log_power_q16(value):
    """Native six-significant-bit log approximation, reconstructed as math."""
    if not 0<=value<=MASK40: raise ValueError('log-power input is not a 40-bit word')
    if signed40(value)<=0: return value
    exponent=max(0,value.bit_length()-6)
    index=value>>exponent
    lookup=math.floor(640*math.log10(index))
    return ((lookup<<16)+exponent*(1541<<13))&MASK40


def tone_power(first_tick,tone):
    if not tone: return 0
    phase=(first_tick//12)%16
    # The discard fills history with complete tone periods. Subsequent FIR
    # outputs are quarter-turns apart, so their squared magnitude is constant.
    i=(sum(c*round(1024*math.cos((phase+2*k)*math.pi/8)) for k,c in enumerate(COEFFICIENTS))+32768)>>16
    q=(sum(c*round(1024*math.sin((phase+2*k)*math.pi/8)) for k,c in enumerate(COEFFICIENTS))+32768)>>16
    return i*i+q*q


def check_measure_program(program,mcu,resident):
    check_filter_program(program,mcu,resident)
    if (program[0x4315*2:0x4348*2]!=resident[0x4315*2:0x4348*2] or
            program[0x242c*2:0x2437*2]!=resident[0x242c*2:0x2437*2] or
            program[0x71a3*2:0x71d8*2]!=resident[0x71a3*2:0x71d8*2] or
            program[0x0606*2:0x0607*2]!=bytes.fromhex('fc00')):
        raise ValueError('native log-power routine/entry hook differs')


def check(text,program,mcu,resident,data,tone):
    check_measure_program(program,mcu,resident)
    lookup=[0]+[math.floor(640*math.log10(index)) for index in range(1,64)]
    if len(data)!=0x20000 or data[0xb09e*2:0xb0de*2]!=struct.pack('>64H',*lookup):
        raise ValueError('native log-power table differs from independent logarithms')
    totals=re.findall(r'rom6_measure_complete: calls=(\d+) pending=(\d+) cap=32 observer_writes=0',text)
    if totals!=[('32','0')]: raise ValueError('incomplete native log-power prefix')
    kinds=((INPUT,'input'),(OUTPUT,'output'),(RETURN,'return'),(EPOCH,'epoch'),(RESULT,'result'))
    events=[]
    for expression,kind in kinds:
        rows=list(expression.finditer(text))
        if len(rows)!=text.count(expression.pattern.split(':',1)[0]+':'):
            raise ValueError('malformed native measurement '+kind)
        if kind in ('input','output') and len(rows)!=32:
            raise ValueError('missing native log-power '+kind)
        events.extend((row.start(),kind,row) for row in rows)
    events.sort()
    calls={}
    active=last_output=pending_store=None
    epoch=None
    outputs=returns=stores=0
    result_sequence=0
    modes=set()
    raw_values=[]
    seen_returns=set()
    for _,kind,row in events:
        if kind=='epoch':
            epoch=int(row[1])
            if epoch%12: raise ValueError('unaligned native measurement source epoch')
        elif kind=='input':
            number,a,ar2,calibration,flags,band,st1=(int(value,base) for value,base in zip(row.groups(),(10,16,16,16,16,16,16)))
            if number!=len(calls)+1 or active is not None or pending_store is not None or st1&0x35f!=0x100:
                raise ValueError('native log-power input order/arithmetic mode differs')
            calls[number]=(a,ar2,calibration,flags,band,epoch)
            active=number
        elif kind=='output':
            number,a,b,ar2,calibration,st1=(int(value,base) for value,base in zip(row.groups(),(10,16,16,16,16,16)))
            if number!=active or st1&0x35f!=0x100:
                raise ValueError('native log-power output order/arithmetic mode differs')
            power,pointer,index,flags,band,_=calls[number]
            if flags&0x20:
                base=0x474 if band&4 else 0x49c
                address=(base+signed(index))&0xffff
                correction=signed(calibration)<<16
                modes.add(f'table_{base:04x}')
            else:
                address=pointer
                correction=signed(calibration)<<22
                modes.add('direct_x64')
                if calibration!=index: raise ValueError('direct native calibration changed')
            if ar2!=address or a!=log_power_q16(power) or b!=(a-correction)&MASK40:
                raise ValueError('native log-power/calibration arithmetic differs')
            active=None; outputs+=1; last_output=(number,b)
        elif kind=='return':
            number,b=int(row[1]),int(row[2],16)
            if number>=32 or number in seen_returns or active is not None or last_output!=(number,b) or pending_store is not None:
                raise ValueError('native raw RSSI return lacks its conversion')
            power,_,_,_,_,source_tick=calls[number]
            if source_tick is None or power!=tone_power(source_tick,tone):
                raise ValueError('native raw RSSI power differs from phased laboratory signal')
            returns+=1
            seen_returns.add(number)
            pending_store=(b>>16)&0xffff
        else:
            number,value,mode,work,pc=row.groups()
            result_sequence+=1
            if int(number)!=result_sequence: raise ValueError('native measurement store accounting differs')
            if pending_store is not None:
                if pc!='002432' or mode!='0004' or work!='0000' or int(value,16)!=pending_store:
                    raise ValueError('native raw RSSI store differs from its conversion')
                raw_values.append(signed(pending_store)); stores+=1; pending_store=None
    if active is not None or pending_store is not None or outputs!=32 or not returns or stores!=returns:
        raise ValueError('native raw RSSI prefix is incomplete')
    return dict(conversions=outputs,raw_rssi_stores=stores,calibration_modes=sorted(modes),
                raw_signed_values=sorted(set(raw_values)),phased_signal_power_qualified=True,
                log_power_arithmetic_qualified=True,raw_rssi_store_qualified=True,
                units='1/64 dB relative to firmware calibration',
                physical_dbm_qualified=False,post_correction_qualified=False,
                rssi_response_packet_qualified=False,fcch_qualified=False,camping_qualified=False)


def frequency_correction(raw,key,flags,main,other):
    if not 0<=key<=1023 or len(main)!=4 or len(other)!=4:
        raise ValueError('unsupported native correction channel/descriptor')
    descriptor=main
    extended=bool(flags&0x80) and key>=975
    if extended: key-=1024
    elif flags&0x80 and key>124: descriptor=other
    low,high,left,right=map(signed,descriptor)
    if extended or key<=low: delta,slope=low-key,left
    elif key>=high: delta,slope=key-high,right
    else: return raw,False
    # NORM with T=16 followed by signed MPYA uses the resulting high word.
    delta=signed(delta&0xffff)
    return (signed(raw)+((delta*slope+128)>>8))&0xffff,True


def check_frequency_correction(text):
    totals=re.findall(r'rom6_measure_correct_complete: calls=(\d+) pending=(\d+) cap=16 observer_writes=0',text)
    if totals!=[('16','0')]: raise ValueError('incomplete native correction prefix')
    events=[]
    for expression,kind in ((CORRECT_INPUT,'input'),(CORRECT_OUTPUT,'output'),(RESULT,'store'),(RETURN,'raw_return')):
        rows=list(expression.finditer(text))
        if len(rows)!=text.count(expression.pattern.split(':',1)[0]+':'):
            raise ValueError('malformed native correction '+kind)
        if kind in ('input','output') and len(rows)!=16:
            raise ValueError('missing native correction '+kind)
        events.extend((row.start(),kind,row) for row in rows)
    events.sort()
    pending_return=last_raw=active=None
    count=stores=joined=unchanged=0
    channels=[]
    for _,kind,row in events:
        if kind=='raw_return':
            if pending_return is not None: raise ValueError('unconsumed raw correction return')
            pending_return=int(row[1])
        elif kind=='store':
            value,pc=int(row[2],16),row[5]
            if pc=='002432':
                last_raw=(value,pending_return)
                pending_return=None
            elif active is not None:
                if pc!='0071d7' or not active['write'] or active['stored'] or value!=active['value']:
                    raise ValueError('native frequency correction store differs')
                active['stored']=True; stores+=1
        elif kind=='input':
            number,key,pointer,raw,flags,main,other,st1=row.groups()
            number,key,pointer,raw,flags,st1=(int(value,base) for value,base in zip((number,key,pointer,raw,flags,st1),(10,16,16,16,16,16)))
            if active is not None or number!=count+1 or pointer!=0x22b or st1&0x340!=0x100:
                raise ValueError('native correction input order/ownership differs')
            if last_raw is None or raw!=last_raw[0]: raise ValueError('native correction lacks its raw store')
            value,write=frequency_correction(raw,key,flags,
                [int(word,16) for word in main.split(',')],[int(word,16) for word in other.split(',')])
            active=dict(number=number,value=value,write=write,stored=False,joined=last_raw[1] is not None)
            last_raw=None; channels.append(key)
        else:
            number,pointer,value,st1=(int(value,base) for value,base in zip(row.groups(),(10,16,16,16)))
            if (active is None or number!=active['number'] or pointer!=0x22b or
                    value!=active['value'] or st1&0x340!=0x100 or active['stored']!=active['write']):
                raise ValueError('native correction output/arithmetic differs')
            count+=1; joined+=active['joined']; unchanged+=not active['write']; active=None
    if active is not None or count!=16: raise ValueError('native correction did not finish')
    return dict(calls=count,stores=stores,no_store_returns=unchanged,joined_raw_measurements=joined,
                channels=channels,scope='first16 correction calls; only observed channel branches are native-qualified',
                mcu_response_packet_qualified=False,physical_dbm_qualified=False,camping_qualified=False)


def check_rx_publications(text):
    """Decode actual committed ring spans; never infer a missing reply payload."""
    totals=re.findall(r'rom6_measure_rx_complete: commits=(\d+) cap=(64|256) observer_writes=0',text)
    rows=list(RX_COMMIT.finditer(text))
    if (len(totals)!=1 or text.count('rom6_measure_rx_complete:')!=1 or
            not rows or len(rows)>int(totals[0][1]) or int(totals[0][0])!=len(rows) or
            len(rows)!=text.count('rom6_measure_rx_commit:')):
        raise ValueError('incomplete or malformed native RX publication capture')
    previous_end=None
    previous_time=-1
    packets=[]
    for number,row in enumerate(rows,1):
        sequence,time,start,end,consumer,words,pc=row.groups()
        start,end,consumer=(int(value,16) for value in (start,end,consumer))
        packed=[int(word,16) for word in words.split(',')]
        if (int(sequence)!=number or float(time)<previous_time or
                not all(0x80<=value<0xe4 for value in (start,end,consumer)) or
                previous_end is not None and start!=previous_end or
                (end-start)%0x64!=len(packed)):
            raise ValueError('native RX cursor/time continuity differs')
        offset=0
        while offset<len(packed):
            header=packed[offset]
            length=header>>8
            count=(length+3)//2
            if offset+count>len(packed): raise ValueError('native RX packet exceeds committed words')
            payload=b''.join(word.to_bytes(2,'big') for word in packed[offset+1:offset+count])[:length]
            packets.append(dict(commit=number,time=time,type=header&0xff,payload=payload.hex(),cpu_pc=pc))
            offset+=count
        previous_end=end; previous_time=float(time)
    return dict(commits=len(rows),packets=packets,
                rssi_reply_observed=any(packet['type'] in (0x83,0x8b) for packet in packets),
                scope='all captured native ring commits within this bounded laboratory run',
                mcu_consumption_qualified=False,camping_qualified=False)


def check_fcch_only_sch_failure(text,program):
    """Join this finite, SCH-absent fixture's CRC status to its actual packet."""
    fragments={
        0x4bcd:(0x7715,0x000e,0x7714,0x0080,0xf274,0x2c88,0x7713,0x2f5b),
        0x4efb:(0x6ff8,0x2f20,0x0c48,0x1a93),
        0x4f53:(0xf030,0x03ff,0x60f8,0x0008,0x03ff,0x7711,0,0xfd20,0x6d91,0xfe00,0x7311,0x2f20)}
    for address,words in fragments.items():
        size=len(words)*2
        if program[address*2:address*2+size]!=struct.pack('>'+str(len(words))+'H',*words):
            raise ValueError('SCH failure formatter/CRC program differs')
    sources=re.findall(r'rom6_burst_block_sources: sequence=(\d+) channel=([0-9a-f]{4}) selection=([0-9a-f]{4}) crc=([0-9a-f]{4}) bsic=([0-9a-f]{4}) frame=([0-9a-f]{4}),([0-9a-f]{4}) arfcn=([0-9a-f]{4}) shift=([0-9a-f]{4}) decoded=([0-9a-f]{4}),([0-9a-f]{4}),([0-9a-f]{4}) observer_writes=0',text)
    crc=re.findall(r'rom6_burst_crc: sequence=(\d+) t=(\d+\.\d{9}) status=([0-9a-f]{4}) remainder=([0-9a-f]{4}) input=([0-9a-f]{4}),([0-9a-f]{4}),([0-9a-f]{4}) pc=([0-9a-f]{6}) observer_writes=0',text)
    fields=re.findall(r'rom6_burst_block_field: sequence=(\d+) t=(\d+\.\d{9}) address=([0-9a-f]{4}) value=([0-9a-f]{4}) pc=([0-9a-f]{6}) observer_writes=0',text)
    packets=[packet for packet in check_rx_publications(text)['packets']
             if packet['type']==0x80 and packet['payload'].startswith('40')]
    if (len(sources)!=1 or len(crc)!=1 or len(fields)!=7 or len(packets)!=1 or
            text.count('rom6_burst_block_sources:')!=1 or text.count('rom6_burst_crc:')!=1 or
            text.count('rom6_burst_block_field:')!=7):
        raise ValueError('bounded SCH failure chain is incomplete')
    sequence=int(sources[0][0])
    channel,selection,status,bsic,frame0,frame1,arfcn,shift,*decoded=(int(word,16) for word in sources[0][1:])
    if sequence!=1 or channel!=0x40 or selection!=0 or status!=1:
        raise ValueError('FCCH-only fixture did not take its failed SCH formatter')
    _,crc_time,crc_status,remainder,*rest=crc[0]
    if crc[0][0]!='1' or int(crc_status,16)!=status or int(remainder,16)==0x3ff:
        raise ValueError('SCH status is not the failing native CRC decision')
    if any(int(word,16) for word in rest[:3]) or decoded!=[0,0,0] or int(remainder,16)!=0:
        raise ValueError('zero SCH input no longer gives the qualified zero-codeword CRC negative')
    swap=lambda word: ((word&255)<<8)|(word>>8)
    expected=[(channel<<8)|(bsic&255),(status<<8)|(frame0&255),frame1,arfcn,shift,
              swap(decoded[0]),swap(decoded[1])]
    previous_time=float(crc_time)
    for index,row in enumerate(fields):
        ordinal,stamp,address,value,_=row
        if (int(ordinal)!=index+1 or int(address,16)!=0x2f5b+index or int(value,16)!=expected[index] or
                not previous_time<=float(stamp)<float(packets[0]['time'])):
            raise ValueError('SCH formatter field does not join CRC and ring publication')
        previous_time=float(stamp)
    body=b''.join(word.to_bytes(2,'big') for word in expected).hex()
    if packets[0]['payload']!=body:
        raise ValueError('published SCH body differs from native formatter sources')
    return dict(channel=channel,error_flag=status,crc_remainder=0,zero_codeword=True,
                serialized_words=7,publication_time=packets[0]['time'],payload=body,
                frame_fields_are_metadata=True,decoded_frame_number_qualified=False,
                sch_qualified=False,camping_qualified=False)

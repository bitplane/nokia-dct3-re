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

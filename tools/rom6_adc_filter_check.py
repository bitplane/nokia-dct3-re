"""Qualify a bounded native FIR prefix, not RF timing or FCCH detection."""
import re

from tools.dsp_rom6_candidate_check import PINNED,digest
from tools.rom6_adc_lab_check import WORD
from tools.rom6_receive_contract import check_program

INITIAL=re.compile(r'rom6_adc_processing_initial: history=([0-9a-f]{4}(?:,[0-9a-f]{4}){15}) coefficients=([0-9a-f]{4}(?:,[0-9a-f]{4}){7}) observer_writes=0')
HISTORY=re.compile(r'rom6_adc_history: read=(\d+) address=([0-9a-f]{4}) value=([0-9a-f]{4}) observer_writes=0')
FILTER=re.compile(r'rom6_adc_filter: write=(\d+) read=(\d+) address=([0-9a-f]{4}) value=([0-9a-f]{4}) cpu_pc=([0-9a-f]{6}) st1=([0-9a-f]{4}) observer_writes=0')
COMPLETE=re.compile(r'rom6_adc_processing_complete: history_writes=(\d+) filter_writes=(\d+) input_cap=1024 output_cap=128 observer_writes=0')
MIXER=re.compile(r'rom6_adc_mixer: write=(\d+) read=(\d+) previous=([0-9a-f]{4}) pointer=([0-9a-f]{4}) mode=([0-9a-f]{4}) values=([0-9a-f]{4}(?:,[0-9a-f]{4}){1,3}) cpu_pc=([0-9a-f]{6}) observer_writes=0')
COEFFICIENTS=(0x0387,0x11a3,0x2ad6,0x4000,0x4000,0x2ad6,0x11a3,0x0387)


def signed(word):
    return word-0x10000 if word&0x8000 else word


def fir_word(history,coefficients,last,component):
    # Eight signed Q16 coefficients, oldest pair first. Each four scalar
    # inputs produce Q then I, with half-up rounding to the stored high word.
    total=sum(signed(coefficients[k])*signed(history[(last-15+2*k+component)%16]) for k in range(8))
    return ((total+0x8000)>>16)&0xffff


def check_filter_program(program,mcu,resident):
    check_program(program,mcu)
    if (digest(resident)!=PINNED['working/rom6_program.be16'] or
            program[0x2468*2:0x24d4*2]!=resident[0x2468*2:0x24d4*2] or
            program[0x25bc*2:0x25c6*2]!=resident[0x25bc*2:0x25c6*2]):
        raise ValueError('native FIR code differs from supplied resident ROM')
    if (program[0x590*2:0x736*2]!=mcu[0x115128:0x115128+844] or
            program[0x05de*2:0x05df*2]!=bytes.fromhex('fc00')):
        raise ValueError('native FIR entry hook is not the observed return')


def check(text,program,mcu,resident):
    check_filter_program(program,mcu,resident)
    initial=list(INITIAL.finditer(text))
    complete=list(COMPLETE.finditer(text))
    if (len(initial)!=1 or text.count('rom6_adc_processing_initial:')!=1 or
            len(complete)!=1 or text.count('rom6_adc_processing_complete:')!=1):
        raise ValueError('missing or malformed native FIR prefix accounting')
    history,coefficients=([int(word,16) for word in group.split(',')] for group in initial[0].groups())
    if len(history)!=16 or tuple(coefficients)!=COEFFICIENTS:
        raise ValueError('native FIR history/coefficient geometry differs')
    inputs=list(WORD.finditer(text))[:1024]
    if len(inputs)!=1024 or [int(word.group(1)) for word in inputs]!=list(range(1,1025)):
        raise ValueError('native FIR lacks the complete first waveform prefix')
    histories=list(HISTORY.finditer(text))
    outputs=list(FILTER.finditer(text))
    if (len(histories)!=1024 or text.count('rom6_adc_history:')!=1024 or
            len(outputs)!=128 or text.count('rom6_adc_filter:')!=128 or
            complete[0].groups()!=('1024','496')):
        raise ValueError('native FIR prefix is incomplete or has unexpected accounting')
    events=sorted([(row.start(),'input',row) for row in histories]+
                  [(row.start(),'output',row) for row in outputs])
    read=written=nonzero=0
    for _,kind,row in events:
        if kind=='input':
            number,address,value=(int(item,base) for item,base in zip(row.groups(),(10,16,16)))
            read+=1
            if number!=read or address!=0x200+(read-1)%16 or value!=int(inputs[read-1].group(2),16):
                raise ValueError('native FIR history is not the ordered delivered waveform')
            history[address-0x200]=value
        else:
            number,at_read,address,value,pc,st1=(int(item,base) for item,base in zip(row.groups(),(10,10,16,16,16,16)))
            written+=1
            component=1 if written&1 else 0
            if (number!=written or at_read!=read or read!=32+4*((written+1)//2) or
                    address!=0xc8+(written-1)%16 or pc!=(0x2496 if component else 0x2497) or
                    st1&0x3ff!=0x100):
                raise ValueError('native FIR output order, arithmetic mode or writer differs')
            if value!=fir_word(history,coefficients,(read-1)%16,component):
                raise ValueError('native FIR differs from independent signed convolution')
            nonzero+=value!=0
    return dict(discarded_scalar_words=32,history_words=1024,checked_output_words=written,
                nonzero_output_words=nonzero,output_order='Q,I',complex_decimation=2,
                scope='first128 output words within first1024 delivered inputs',
                physical_clock_qualified=False,rssi_qualified=False,fcch_qualified=False,
                sch_qualified=False,bcch_qualified=False,camping_qualified=False)


def quarter_rate_mix(words,group):
    """Rotate Q,I pairs by -pi/2 per pair, then sum each group."""
    result=[]
    for first in range(0,len(words)//2,group):
        real=imag=0
        for index in range(group):
            q,i=map(signed,words[2*(first+index):2*(first+index)+2])
            r,j=((i,q),(q,-i),(-i,-q),(-q,i))[index%4]
            real+=r; imag+=j
        result.extend((real&0xffff,imag&0xffff))
    return result


def check_mixer(text):
    # Caller first qualifies these FIR words and code with check(). This is a
    # bounded arithmetic join, not a separate waveform/physical-clock oracle.
    filtered=list(FILTER.finditer(text))
    mixers=list(MIXER.finditer(text))
    totals=re.findall(r'rom6_adc_mixer_complete: writes=(\d+) cap=8 observer_writes=0',text)
    if (len(filtered)!=128 or len(mixers)!=8 or text.count('rom6_adc_mixer:')!=8 or
            totals!=['31'] or text.count('rom6_adc_mixer_complete:')!=1):
        raise ValueError('missing or incomplete native mixer prefix')
    previous=None
    values=[]
    modes=set()
    for index,row in enumerate(mixers,1):
        number,read,old,pointer,mode,payload,pc=row.groups()
        old,pointer,mode=int(old,16),int(pointer,16),int(mode,16)
        words=[int(word,16) for word in payload.split(',')]
        expected=quarter_rate_mix([int(word.group(4),16) for word in filtered[(index-1)*16:index*16]],8 if mode==1 else 4)
        if (number!=str(index) or int(read)!=32+32*index or mode not in (0,1) or
                pc!='0024d7' or not 0x80<=old<0xc8 or
                pointer!=0x80+(old-0x80+len(expected))%0x48 or
                (previous is not None and old!=previous)):
            raise ValueError('native mixer order, pointer or mode differs')
        if words!=expected: raise ValueError('native mixer differs from independent quarter-rate rotation')
        previous=pointer
        values.extend(map(signed,words)); modes.add(mode)
    adjacent=[values[index]*values[index+2]+values[index+1]*values[index+3]
              for index in range(0,len(values)-2,2)]
    return dict(checked_blocks=8,checked_output_words=len(values),modes=sorted(modes),
                adjacent_group_dot_products=adjacent,physical_clock_qualified=False,
                fcch_detection_qualified=False,camping_qualified=False)

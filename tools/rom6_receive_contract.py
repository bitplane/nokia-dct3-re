"""Check passive ROM6 receive chronology, not physical sample-clock fidelity."""
from collections import Counter
import hashlib
import re

from tools.rom6_radio_census import WRITE
from tools.rom6_timed_output_check import nanoseconds
from tools.rom6_tuning_contract import fields

WORD=re.compile(r'rom6_receive_word: sequence=(\d+) t=(\d+\.\d{9}) value=([0-9a-f]{4}) mask=([0-9a-f]{4}) cpu_pc=([0-9a-f]{6}) pmst=([0-9a-f]{4}) imr=([0-9a-f]{4}) ifr=([0-9a-f]{4}) observer_writes=0')
COMPLETE=re.compile(r'rom6_receive_complete: reads=(\d+) captured=(\d+) cap=(\d+) observer_writes=0')
COEFFICIENTS='0387,11a3,2ad6,4000,4000,2ad6,11a3,0387'
REGIONS=(
    (0x1845,0x18d2,0x115f1a,'bce951612776b11c87f4beda52b0ee527056fc0f9e8767174b626f64c6b786c3'),
    (0x07c0,0x07c4,0x11ab94,'b27015781ac089b067fce880b4062aeb1a545758ce293f9ab531d244ac92315e'),
    (0x13d2,0x140e,0x115634,'c301dfaa143af16af3d42d2a1cf601e8f26cdffe70e85f3c3eb123341fb3abe6'),
    (0x1980,0x1990,0x116190,'5b6b44a5d062c8836f7809e6fb6e87466507ed4cf135ea05ae7912c7ba651c5b'),
)
SOURCE_OFFSETS = {
    # Pinned MCU builds: NSM-3 v5.31 and NPE-3 v5.56 PPM C, not all revisions.
    '8210': tuple(region[2] for region in REGIONS),
    '6210': (0x1f8b6, 0x24ad0, 0x1efd0, 0x1fb2c),
}
SOURCE_HASH_OVERRIDES = {
    ('6210', 0x13d2): 'dd4cc8c2b76ab553ca73f72e136d512512113845dab1f0cdf79b211a41a5a179',
}


def check_program(program,mcu,*,product='8210'):
    if len(program)!=0x20000:
        raise ValueError('incomplete first-read program view')
    if product not in SOURCE_OFFSETS:
        raise ValueError('unknown receive upload profile')
    for (start,end,_,digest),offset in zip(REGIONS,SOURCE_OFFSETS[product]):
        digest=SOURCE_HASH_OVERRIDES.get((product,start),digest)
        block=program[start*2:end*2]
        if hashlib.sha256(block).hexdigest()!=digest or mcu[offset:offset+len(block)]!=block:
            raise ValueError('receive code is not the pinned own upload')


def position_reference(ar1,length):
    value=ar1-32
    return (value+length if value<0 else value)&0xffff


def capture_start_reference(ar1,lead,length):
    value=ar1-lead
    return (value+length if value<0 else value)&0xffff


def check_stop_polls(text, writes):
    polls=[fields(row) for row in re.findall(r'rom6_capture_stop_poll: ([^\r\n]+)',text)]
    stops=[row for row in writes if (row['port'],row['cpu_pc'])==('000b','0015d0')]
    if not stops or len(polls)!=len(stops):
        raise ValueError('missing capture stop counter chronology')
    expected={'sequence','first','first_pc','first_t','first_mask','last','last_pc','last_t','last_mask','observer_writes'}
    for poll,stop in zip(polls,stops):
        # Read-tap contexts are not issuing instruction addresses (3ee4/3eea).
        if (set(poll)!=expected or poll['sequence']!=stop['sequence'] or
                poll['observer_writes']!='0' or poll['first_mask']!='ffff' or
                poll['last_mask']!='ffff' or poll['first_pc'] not in ('003eea','003ef0') or
                poll['last_pc']!='003ef0'):
            raise ValueError('capture stop poll ownership differs')
        if not nanoseconds(poll['first_t'])<nanoseconds(poll['last_t'])<nanoseconds(stop['t']):
            raise ValueError('capture stop poll timestamp order differs')
        first,last=(int(poll[name],16)&0x3fff for name in ('first','last'))
        if first!=last:
            raise ValueError('capture stop did not use a stable masked counter')
        value=last+int(stop['tail'],16)
        length=int(stop['length'],16)
        if value>=length: value-=length
        if (value&0xffff)!=int(stop['value'],16):
            raise ValueError('capture stop counter/addition/wrap differs')
    return len(polls)


def check_capture_program(program,mcu,resident):
    # The stop writer is an own uploaded replacement, not the resident 3e7a.
    block=program[0x15c4*2:0x15d2*2]
    if (len(program)!=0x20000 or len(resident)!=0x20000 or
            hashlib.sha256(block).hexdigest()!=
            '820291aa03aec557fd522ba30fc3d54adc3689b0e4bafb6f6ff37c3b68d65453' or
            mcu[0x115a18:0x115a18+len(block)]!=block or mcu.count(block)!=1):
        raise ValueError('capture stop writer is not the pinned own upload')
    for start,end in ((0x3e1b,0x3e34),(0x3ed8,0x3ee1),(0x3ee1,0x3ef3),
                      (0x3f25,0x3f3a),(0x3fdc,0x3fe8)):
        if program[start*2:end*2]!=resident[start*2:end*2]:
            raise ValueError('capture resident code differs from the supplied ROM')


def check_capture_programming(text,stream,program,mcu,resident):
    check_capture_program(program,mcu,resident)
    complete=re.findall(r'rom6_capture_complete: writes=(\d+) cap=32768 observer_writes=0',text)
    rows=[fields(row) for row in re.findall(r'rom6_capture_write: ([^\r\n]+)',text)]
    if len(complete)!=1 or not 548<int(complete[0])<32768 or len(rows)!=int(complete[0]):
        raise ValueError('missing, incomplete or capped capture programming')
    expected={'sequence','t','port','value','ar1','a','b','lead','tail','length','cpu_pc','mask','observer_writes'}
    counts,leads,tails=Counter(),Counter(),Counter()
    observed=[]
    last=-1
    for index,row in enumerate(rows,1):
        if set(row)!=expected or row['sequence']!=str(index) or row['mask']!='ffff' or row['observer_writes']!='0':
            raise ValueError('capture programming metadata differs')
        stamp=nanoseconds(row['t'])
        if stamp<last: raise ValueError('capture programming timestamp order differs')
        last=stamp
        port,value,ar1,lead,tail,length=(int(row[key],16) for key in ('port','value','ar1','lead','tail','length'))
        pc=row['cpu_pc']
        counts[f'{port:04x}:{pc}']+=1
        if (port,pc)==(0x0a,'003e24'):
            if value!=capture_start_reference(ar1,lead,length) or value!=int(row['b'],16)&0xffff:
                raise ValueError('capture start subtraction/wrap differs')
            leads[f'{lead:04x}']+=1
        elif (port,pc)==(0x0b,'0015d0'):
            # A is the value after the stable P0d read, tail addition and one
            # conditional subtraction. P0d itself is not logged in this census.
            if value!=int(row['a'],16)&0xffff:
                raise ValueError('capture stop accumulator differs')
            tails[f'{tail:04x}']+=1
        elif (port,pc) in ((0x0a,'003f27'),(0x0a,'003edc'),(0x0b,'003e2c')):
            if value!=0x3a98: raise ValueError('capture sentinel differs')
        elif (port,pc)==(0x0b,'003f38'):
            if value: raise ValueError('capture initial stop differs')
        elif (port,pc)==(0x11,'003fe5'):
            if value!=0x01fe: raise ValueError('uninterpreted P11 initialization differs')
        else:
            raise ValueError('unclassified capture port writer')
        observed.append((row['t'],pc,row['port'],row['value'],row['mask']))
    actual=[]
    for line in stream:
        match=WRITE.fullmatch(line)
        if not match: raise ValueError('malformed capture programming bus source')
        time,pc,port,value,mask=(item.decode() for item in match.groups())
        if port in ('000a','000b','0011'): actual.append((time,pc,port,value,mask))
    if actual!=observed: raise ValueError('capture programming differs from independent bus stream')
    stop_polls=check_stop_polls(text,rows)
    return dict(writes=len(rows),writers=dict(sorted(counts.items())),
                lead_values=dict(sorted(leads.items())),tail_values=dict(sorted(tails.items())),
                start_operand_qualified=True,stop_accumulator_qualified=True,
                stop_poll_value_qualified=True,stop_polls=stop_polls,physical_capture_edges_qualified=False,
                p11_capacity_qualified=False)


def check_short_program(short_program,mcu):
    block=short_program[0x09d8*2:0x09f4*2]
    if (len(short_program)!=0x4000 or hashlib.sha256(block).hexdigest()!=
            '3bc200ec0b2f0dc4e405da8bc274db0a660220a3ef8dc11c192c3e53cdf62825'
            or block!=mcu[0x116de0:0x116de0+56] or mcu.count(block)!=1):
        raise ValueError('short receive reload is not the pinned own upload')


def check_programming(text,stream,short_program,mcu):
    check_short_program(short_program,mcu)
    totals=re.findall(r'rom6_receive_programming_complete: positions=(\d+) programming=(\d+) cap=16384 observer_writes=0',text)
    if len(totals)!=1 or any(not 548<int(value)<16384 for value in totals[0]):
        raise ValueError('missing or capped receive-programming census')
    positions=[fields(row) for row in re.findall(r'rom6_receive_position: ([^\r\n]+)',text)]
    programming=[fields(row) for row in re.findall(r'rom6_receive_programming: ([^\r\n]+)',text)]
    counts=Counter()
    moduli=Counter()
    for rows,kind in ((positions,'position'),(programming,'programming')):
        expected_fields={'sequence','t','value','ar1','length','cpu_pc','mask','observer_writes'}
        if kind=='programming': expected_fields.add('position1844')
        if len(rows)!=int(totals[0][0 if kind=='position' else 1]):
            raise ValueError('receive-programming accounting differs')
        last=-1
        for ordinal,row in enumerate(rows,1):
            if set(row)!=expected_fields or row['sequence']!=str(ordinal) or row['mask']!='ffff' or row['observer_writes']!='0':
                raise ValueError('incomplete or unordered receive-programming fields')
            stamp=nanoseconds(row['t'])
            if stamp<last: raise ValueError('receive-programming timestamp order differs')
            last=stamp
            value,ar1,length=(int(row[name],16) for name in ('value','ar1','length'))
            pc=row['cpu_pc']
            counts[kind+':'+pc]+=1
            if kind=='position':
                if pc in ('000f15','000f66'):
                    if value: raise ValueError('initial receive position clear differs')
                elif pc=='00140d':
                    if value!=position_reference(ar1,length):
                        raise ValueError('receive position subtraction/wrap differs')
                    moduli[f'{length:04x}']+=1
                else: raise ValueError('unclassified receive-position writer')
            elif pc=='003f18':
                if value!=0x3a98: raise ValueError('receive cancellation sentinel differs')
            elif pc=='003e32':
                if value!=ar1: raise ValueError('direct receive position differs from AR1')
            elif pc=='00198c':
                if value!=int(row['position1844'],16):
                    raise ValueError('deferred receive position differs from saved position')
            else: raise ValueError('unclassified P09 writer')
    reloads=[fields(row) for row in re.findall(r'rom6_receive_reload: ([^\r\n]+)',text)]
    if any(set(row)!={'t','value','a','b','ar1','cpu_pc','mask','observer_writes'} or
           row['observer_writes']!='0' or row['mask']!='ffff' for row in reloads):
        raise ValueError('receive reload metadata incomplete')
    actual=[]
    actual_reload=[]
    for line in stream:
        row=WRITE.fullmatch(line)
        if not row: raise ValueError('malformed receive-programming bus source')
        time,pc,port,value,mask=(word.decode() for word in row.groups())
        if port=='0009': actual.append((time,pc,value,mask))
        if port=='000e': actual_reload.append((time,pc,value,mask))
    if actual!=[(row['t'],row['cpu_pc'],row['value'],row['mask']) for row in programming]:
        raise ValueError('receive-programming capture differs from independent bus stream')
    if actual_reload!=[(row['t'],row['cpu_pc'],row['value'],row['mask']) for row in reloads]:
        raise ValueError('receive reload capture differs from independent bus stream')
    saved=None
    events=[(nanoseconds(row['t']),'position',row) for row in positions]
    events.extend((nanoseconds(row['t']),'programming',row) for row in programming)
    for stamp,kind,row in sorted(events,key=lambda event:event[0]):
        if kind=='position': saved=int(row['value'],16)
        elif int(row['position1844'],16)!=saved:
            raise ValueError('P09 observer did not see the latest captured position store')
    return dict(position_stores=len(positions),p09_writes=len(programming),
                writer_contexts=dict(counts),position_moduli=dict(moduli),
                observed_reload_writes=len(reloads),
                all_observed_writers_classified=True,short_reload_own_upload=True,
                physical_clock_or_irq_proved=False)


def check(text,stream,program,mcu):
    check_program(program,mcu)
    coefficients=re.findall(r'rom6_receive_coefficients: data=3ebd words=8 values=([^\s]+) observer_writes=0',text)
    if coefficients!=[COEFFICIENTS]:
        raise ValueError('first-read receive coefficients differ or are missing')
    rows=list(WORD.finditer(text))
    totals=COMPLETE.findall(text)
    if len(totals)!=1 or totals[0][1:]!=('1024','1024') or len(rows)!=1024:
        raise ValueError('missing complete bounded receive prefix')
    if text.count('rom6_receive_word:')!=len(rows) or int(totals[0][0])<len(rows):
        raise ValueError('malformed receive accounting')
    stamps=[]
    for sequence,row in enumerate(rows,1):
        ordinal,time,value,mask,pc,pmst,imr,ifr=row.groups()
        expected_pc=(0x18a9,0x18af,0x18b5,0x18bb)[(sequence-1)%4]
        if int(ordinal)!=sequence or mask!='ffff' or int(pc,16)!=expected_pc:
            raise ValueError('unexpected native receive read sequence')
        if value!='0000' or pmst!='07ac':
            raise ValueError('baseline silence or native vector page changed')
        stamps.append(nanoseconds(time))
    if any(a>=b for a,b in zip(stamps,stamps[1:])):
        raise ValueError('unordered receive timestamps')
    first={3:None,9:None}
    for line in stream:
        row=WRITE.fullmatch(line)
        if not row: raise ValueError('malformed receive source writes')
        time,pc,port,value,mask=(word.decode() for word in row.groups())
        if nanoseconds(time)>=stamps[0]: break
        if int(port,16) in first: first[int(port,16)]=int(value,16)
    if first!={3:0,9:0x3a98}:
        raise ValueError('first receive read no longer precedes active P03/P09 programming')
    starts=stamps[::32]
    gaps=[b-a for a,b in zip(starts,starts[1:])]
    if any(not 4_500_000<gap<4_700_000 for gap in gaps):
        raise ValueError('baseline 32-word groups no longer follow modeled frame cadence')
    return dict(prefix_words=len(rows),groups=len(starts),words_per_group=32,
        total_reads=int(totals[0][0]),group_gap_ns_min=min(gaps),group_gap_ns_max=max(gaps),
        first_p03='0000',first_p09='3a98',own_uploaded_code=True,
        first_read_coefficients=COEFFICIENTS.split(','),
        physical_sample_clock_proved=False,physical_interrupt_owner_proved=False)

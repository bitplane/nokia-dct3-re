"""Check passive ROM6 receive chronology, not physical sample-clock fidelity."""
import hashlib
import re

from tools.rom6_radio_census import WRITE
from tools.rom6_timed_output_check import nanoseconds

WORD=re.compile(r'rom6_receive_word: sequence=(\d+) t=(\d+\.\d{9}) value=([0-9a-f]{4}) mask=([0-9a-f]{4}) cpu_pc=([0-9a-f]{6}) pmst=([0-9a-f]{4}) imr=([0-9a-f]{4}) ifr=([0-9a-f]{4}) observer_writes=0')
COMPLETE=re.compile(r'rom6_receive_complete: reads=(\d+) captured=(\d+) cap=(\d+) observer_writes=0')
REGIONS=(
    (0x1845,0x18d2,0x115f1a,'bce951612776b11c87f4beda52b0ee527056fc0f9e8767174b626f64c6b786c3'),
    (0x07c0,0x07c4,0x11ab94,'b27015781ac089b067fce880b4062aeb1a545758ce293f9ab531d244ac92315e'),
    (0x13d2,0x140e,0x115634,'c301dfaa143af16af3d42d2a1cf601e8f26cdffe70e85f3c3eb123341fb3abe6'),
)


def check_program(program,mcu):
    if len(program)!=0x20000:
        raise ValueError('incomplete first-read program view')
    for start,end,offset,digest in REGIONS:
        block=program[start*2:end*2]
        if hashlib.sha256(block).hexdigest()!=digest or mcu[offset:offset+len(block)]!=block:
            raise ValueError('receive code is not the pinned own upload')


def check(text,stream,program,mcu):
    check_program(program,mcu)
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
        physical_sample_clock_proved=False,physical_interrupt_owner_proved=False)

"""Independent GSM05.03 4.1/4.4 control-channel coding checks for lab sources."""
TSC2='01000011101110100100001110'


def broadcast_si_index(frame):
    if frame<0: raise ValueError('negative TDMA frame')
    return (0,1,2,3,1,1,2,3)[(frame//51)%8]


def broadcast_octets(kind,arfcn=35,*,combined_ccch=False):
    """Single-cell GSM900 laboratory broadcasts,PLMN001-01,LAC/CI1."""
    if not 1<=arfcn<=124: raise ValueError('laboratory broadcast supports GSM900 bitmap0 only')
    if kind==0:
        block=bytearray([0x55,6,0x19]+[0]*19+[0x2b])
        bit=arfcn-1
        block[18-bit//8]|=1<<(bit%8)
        return bytes(block)
    # Sixteen BCCH Frequency List octets, NCC Permitted, three RACH octets.
    if kind==1: return bytes([0x59,6,0x1a]+[0]*16+[0xff,0,0,0])
    if kind==2:
        block=bytearray.fromhex('49061b000100f11000014000000000000000002b2b2b2b')
        block[10]|=int(combined_ccch)
        return bytes(block)
    if kind==3: return bytes([0x31,6,0x1c,0,0xf1,0x10,0,1]+[0]*5+[0x2b]*10)
    raise ValueError('unknown laboratory SI index')


def control_bits(octets):
    if not isinstance(octets,bytes) or len(octets)!=23:
        raise ValueError('control block requires23 octets')
    information=tuple((octets[i//8]>>(i%8))&1 for i in range(184))
    polynomial=0
    for bit in information: polynomial=(polynomial<<1)|bit
    remainder=polynomial<<40
    generator=(1<<40)|(1<<26)|(1<<23)|(1<<17)|(1<<3)|1
    while remainder.bit_length()>40:
        remainder^=generator<<(remainder.bit_length()-41)
    parity=remainder^((1<<40)-1)
    return information+tuple((parity>>bit)&1 for bit in range(39,-1,-1))+(0,)*4


def coded_control(octets):
    uncoded=control_bits(octets)
    result=[]
    for index,bit in enumerate(uncoded):
        past=lambda distance:uncoded[index-distance] if index>=distance else 0
        result.extend((bit^past(3)^past(4),bit^past(1)^past(3)^past(4)))
    return tuple(result)


def normal_control_bursts(octets):
    """Independent four-burst xCCH interleaving with laboratory TSC2."""
    bursts=[[0]*148 for _ in range(4)]
    for burst in bursts:
        burst[60]=burst[87]=1
        burst[61:87]=map(int,TSC2)
    for index,bit in enumerate(coded_control(octets)):
        position=2*((49*index)%57)+(index%8)//4
        at=3+position if position<57 else 88+position-57
        bursts[index%4][at]=bit
    return tuple(tuple(burst) for burst in bursts)


def check_vector(vector):
    octets=bytes.fromhex(vector['octets'])
    expected=coded_control(octets)
    if (vector.get('tsc')!=2 or
            vector.get('information')!=''.join(map(str,control_bits(octets)[:184])) or
            vector.get('coded')!=''.join(map(str,expected))):
        raise ValueError('xCCH octet order,FIRE or convolutional coding differs')
    bursts=vector.get('normal_bursts')
    if (not isinstance(bursts,list) or len(bursts)!=4 or
            any(not isinstance(burst,str) or len(burst)!=148 or set(burst)-{'0','1'} for burst in bursts)):
        raise ValueError('xCCH needs four complete binary normal bursts')
    for burst in bursts:
        if burst[:3]!='000' or burst[-3:]!='000' or burst[61:87]!=TSC2 or burst[60]+burst[87]!='11':
            raise ValueError('normal-burst tails,training or stealing flags differ')
    for index,bit in enumerate(expected):
        position=2*((49*index)%57)+(index%8)//4
        at=3+position if position<57 else 88+position-57
        if int(bursts[index%4][at])!=bit:
            raise ValueError('xCCH interleaving or burst placement differs')
    return dict(octets=octets.hex(),coded_bits=456,bursts=4,training_sequence=2,
                source_bits_qualified=True,native_decoding_qualified=False,camping_qualified=False)

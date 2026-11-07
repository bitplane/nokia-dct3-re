"""Standards-derived laboratory source bits, never decoded-message injection.

GSM 05.03 4.7 defines the SCH parity and rate-1/2 convolutional code.
GSM 05.02 5.2.5 defines the burst, including its extended training sequence.
Information-bit packing into BSIC/frame-number fields is a separate layer.
"""
import math

TRAINING=tuple((0xb962040f2d45761b>>bit)&1 for bit in range(63,-1,-1))
HYPERFRAME=26*51*2048


def sch_information_bits(frame,bsic):
    """Pack RFN/BSIC per GSM 04.08 9.1.30 and the 04.04 bit order."""
    if (type(frame) is not int or not 0<=frame<HYPERFRAME or
            frame%51 not in (1,11,21,31,41) or type(bsic) is not int or not 0<=bsic<64):
        raise ValueError('SCH requires a scheduled hyperframe position and six-bit BSIC')
    t1=frame//(26*51)
    t2=frame%26
    t3=(frame%51-1)//10
    octets=((bsic<<2)|(t1>>9),(t1>>1)&255,((t1&1)<<7)|(t2<<2)|(t3>>1),t3&1)
    return tuple((octets[index//8]>>(index%8))&1 for index in range(25))


def sch_uncoded_bits(information):
    """Return 25 information, ten parity and four terminating zero bits."""
    information=tuple(information)
    if len(information)!=25 or any(type(bit) not in (int,bool) or bit not in (0,1) for bit in information):
        raise ValueError('SCH requires exactly 25 binary information bits')
    information=tuple(map(int,information))
    polynomial=0
    for bit in information: polynomial=(polynomial<<1)|bit
    polynomial<<=10
    remainder=polynomial
    # x^10+x^8+x^6+x^5+x^4+x^2+1; the complete block has remainder 0x3ff.
    while remainder.bit_length()>=11:
        remainder^=0x575<<(remainder.bit_length()-11)
    parity=remainder^0x3ff
    return information+tuple((parity>>bit)&1 for bit in range(9,-1,-1))+(0,)*4


def sch_coded_bits(information):
    """Return e(0)..e(77), with zero prehistory and no interleaving."""
    uncoded=sch_uncoded_bits(information)
    result=[]
    for index,bit in enumerate(uncoded):
        delayed=lambda distance: uncoded[index-distance] if index>=distance else 0
        result.extend((bit^delayed(3)^delayed(4),
                       bit^delayed(1)^delayed(3)^delayed(4)))
    return tuple(result)


def sch_burst_bits(information):
    """Return the 148 active bits: tails, 39 coded, 64 training, 39 coded, tails."""
    coded=sch_coded_bits(information)
    return (0,)*3+coded[:39]+TRAINING+coded[39:]+(0,)*3


def gaussian_phase_response(time):
    """Integral of the GSM 05.04 BT=0.3 Gaussian/rectangle pulse, T=1."""
    sigma=math.sqrt(math.log(2))/(2*math.pi*0.3)
    def primitive(value):
        return (value*(1+math.erf(value/(sigma*math.sqrt(2))))/2+
                sigma/math.sqrt(2*math.pi)*math.exp(-value*value/(2*sigma*sigma)))
    return primitive(time+0.5)-primitive(time-0.5)


def gmsk_burst_samples(bits,samples_per_symbol=4,amplitude=1024):
    """Ideal active-burst I/Q; dummy ones outside, differential precoding, phase(0)=0.

    No RF power-ramp, channel, noise or receiver-selected time origin is implied.
    Four samples/symbol represent either quarter-bit phase of the lab's
    two-complex-sample/symbol interface after a capture is enabled.
    """
    bits=tuple(bits)
    if (not bits or any(type(bit) not in (int,bool) or bit not in (0,1) for bit in bits) or
            type(samples_per_symbol) is not int or samples_per_symbol<2 or
            type(amplitude) is not int or not 0<amplitude<=32767):
        raise ValueError('invalid binary burst, sample cadence or signed amplitude')
    previous=1
    changes=[]
    for index,bit in enumerate((*bits,1)):
        if bit!=previous: changes.append(index)
        previous=bit
    count=len(bits)*samples_per_symbol
    pulse={index:gaussian_phase_response(index/samples_per_symbol) for index in range(-count,count+1)}
    origin=sum(pulse[-index*samples_per_symbol] for index in changes)
    result=[]
    for index in range(count):
        phase=math.pi*index/(2*samples_per_symbol)-math.pi*(
            sum(pulse[index-bit*samples_per_symbol] for bit in changes)-origin)
        result.append((round(amplitude*math.cos(phase)),round(amplitude*math.sin(phase))))
    return tuple(result)

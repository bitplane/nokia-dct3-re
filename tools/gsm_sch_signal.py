"""Standards-derived laboratory source bits, never decoded-message injection.

GSM 05.03 4.7 defines the SCH parity and rate-1/2 convolutional code.
GSM 05.02 5.2.5 defines the burst, including its extended training sequence.
Information-bit packing into BSIC/frame-number fields is a separate layer.
"""

TRAINING=tuple((0xb962040f2d45761b>>bit)&1 for bit in range(63,-1,-1))


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

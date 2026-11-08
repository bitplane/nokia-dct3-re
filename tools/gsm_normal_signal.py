"""Exact noiseless normal-burst/xCCH decoding; not RF demodulation or FEC.

The serializer lane is separately observed at the native P27 boundary.
TS45.002 training set1 and TS45.003 control coding define the bit checks.
"""
from tools.gsm_xcch_signal import control_bits

TSC0=tuple(map(int,'00100101110000100010010111'))


def decode_p27_normal(words,training=TSC0):
    words=tuple(words)
    if len(words)!=15 or any(type(word) is not int or not 0<=word<65536 for word in words):
        raise ValueError('normal serializer requires fifteen words')
    if len(training)!=26 or any(bit not in (0,1) for bit in training):
        raise ValueError('normal training requires26 binary bits')
    bits=tuple(word>>bit&1 for word in words for bit in range(12))
    starts=[at for at in range(16) if bits[at:at+3]==(0,)*3 and
            bits[at+61:at+87]==training and bits[at+145:at+148]==(0,)*3]
    if len(starts)!=1:
        raise ValueError('normal serializer lacks unique tails/training')
    at=starts[0]
    if bits[at+60]!=1 or bits[at+87]!=1:
        raise ValueError('normal xCCH stealing flags differ')
    return bits[at+3:at+60]+bits[at+88:at+145]


def decode_control(bursts):
    if len(bursts)!=4 or any(len(burst)!=114 or any(bit not in (0,1) for bit in burst) for burst in bursts):
        raise ValueError('control decode requires four complete114-bit burst payloads')
    coded=tuple(bursts[index%4][2*((49*index)%57)+(index%8)//4] for index in range(456))
    uncoded=[]
    # With a zero initial state the even output determines the input bit.
    # Checking the odd output makes this an exact decoder,not a Viterbi guess.
    for index in range(228):
        past=lambda distance:uncoded[index-distance] if index>=distance else 0
        bit=coded[2*index]^past(3)^past(4)
        if coded[2*index+1]!=(bit^past(1)^past(3)^past(4)):
            raise ValueError('normal block convolutional code differs')
        uncoded.append(bit)
    octets=bytes(sum(uncoded[index*8+bit]<<bit for bit in range(8)) for index in range(23))
    if tuple(uncoded)!=control_bits(octets):
        raise ValueError('normal block FIRE parity or tails differ')
    return octets

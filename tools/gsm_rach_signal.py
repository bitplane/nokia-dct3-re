"""Exact, noiseless GSM access-burst checks; not an RF receiver.

TS45.003 4.6 defines CRC6, BSIC colouring and the 36 coded bits.
TS45.002 5.2.7 defines the 88 active bits including tails/training.
"""
HEADER=tuple(map(int,'0011101001001011011111111001100110101010001111000'))


def coded_access(ra,bsic):
    if type(ra) is not int or not 0<=ra<256 or type(bsic) is not int or not 0<=bsic<64:
        raise ValueError('access coding requires an octet and six-bit BSIC')
    information=tuple(ra>>bit&1 for bit in range(8))
    polynomial=0
    for bit in information: polynomial=(polynomial<<1)|bit
    remainder=polynomial<<6
    while remainder.bit_length()>6:
        remainder^=0x6f<<(remainder.bit_length()-7)
    parity=remainder^63^bsic
    uncoded=information+tuple(parity>>bit&1 for bit in range(5,-1,-1))+(0,)*4
    result=[]
    for index,bit in enumerate(uncoded):
        past=lambda distance:uncoded[index-distance] if index>=distance else 0
        result.extend((bit^past(3)^past(4),bit^past(1)^past(3)^past(4)))
    return tuple(result)


def decode_p27_access(words,bsic):
    """Check a complete 15-word, low-12-bit/LSB-first serializer transaction.

    The lane follows the resident's 12-bit shifts. Framing is located within
    the first word plus its alignment prefix, not selected from RA_INFO.
    Only exact codewords are accepted; there is no error correction.
    """
    words=tuple(words)
    if len(words)!=15 or any(type(word) is not int or not 0<=word<65536 for word in words):
        raise ValueError('access serializer requires exactly fifteen 16-bit words')
    bits=tuple(word>>bit&1 for word in words for bit in range(12))
    starts=[at for at in range(16) if bits[at:at+49]==HEADER]
    if len(starts)!=1:
        raise ValueError('access serializer has no unique tail/training framing')
    start=starts[0]
    if bits[start+85:start+88]!=(0,0,0):
        raise ValueError('access serializer tail differs')
    payload=bits[start+49:start+85]
    matches=[ra for ra in range(256) if coded_access(ra,bsic)==payload]
    if len(matches)!=1:
        raise ValueError('access serializer CRC/BSIC/convolution differs')
    return dict(ra=matches[0],framing_offset=start,active_bits=88,coded_bits=36)

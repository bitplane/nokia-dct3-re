"""Own NSM-3 type-02 serializer translation, not a DSP lifetime model."""

import hashlib


def verify(image):
    if hashlib.sha1(image).hexdigest() != 'c1a0fe95cedb89a92b19654208cc4855e1a4988e':
        raise ValueError('requires acquired 8210 v5.31 PPM C')
    # Complete constructor through its send call, in CPU big-endian byte order.
    code = image[0x2b3910 - 0x200000:0x2b39b4 - 0x200000]
    if hashlib.sha256(code).hexdigest() != 'c40bbc2bf5ca68fef00887967f067e382d07c1c3d036e24921767ee97a419d51':
        raise ValueError('own type-02 serializer differs')


def serialize(descriptor):
    """Return wire envelope and descriptor after the constructor's byte-A store.

    This translates only 2b3910, not every type-02 constructor. Subtype
    meanings and cancellation of other transactions remain unknown.
    """
    if len(descriptor) != 24:
        raise ValueError('requires a 24-byte descriptor capture')
    source = bytearray(descriptor)
    packet = bytearray(24)
    packet[:4] = bytes.fromhex('00021402')
    for destination, origin in ((4, 0), (11, 1), (5, 6), (10, 2),
                                (14, 8), (15, 9), (18, 4), (19, 5),
                                (21, 13), (22, 14), (23, 15),
                                (8, 16), (9, 17)):
        packet[destination] = source[origin]
    packet[6] = source[6] & 7
    high_nibble = source[1] & 0xf0
    if high_nibble in (0x10, 0x50):
        packet[12] = source[10] = 0x60 if high_nibble == 0x10 else 0x50
    # The exact 0x50 subtype bypasses this tail, not the whole high-nibble family.
    if source[1] != 0x50:
        packet[16] = 0x10 if source[21] == 1 else 0
        packet[20] = source[20]
        packet[7] = source[22]
    return bytes(packet), bytes(source)

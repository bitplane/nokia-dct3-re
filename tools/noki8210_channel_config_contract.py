"""Own NSM-3 type-02 serializer translation, not a DSP lifetime model."""

import hashlib
import re


def verify(image):
    if hashlib.sha1(image).hexdigest() != 'c1a0fe95cedb89a92b19654208cc4855e1a4988e':
        raise ValueError('requires acquired 8210 v5.31 PPM C')
    # Complete constructor through its send call, in CPU big-endian byte order.
    code = image[0x2b3910 - 0x200000:0x2b39b4 - 0x200000]
    if hashlib.sha256(code).hexdigest() != 'c40bbc2bf5ca68fef00887967f067e382d07c1c3d036e24921767ee97a419d51':
        raise ValueError('own type-02 serializer differs')
    for start, end, digest in (
            (0x287390, 0x2873b2, '2e585b67ece4b7eac46d458838e5761cffc48df40bf1b05eab6baecc3f60450e'),
            (0x2eb226, 0x2eb268, '5ee8068452c0df6cca825ea8a84c8793da10943d35db494428af2711b9f024f6'),
            (0x21ed80, 0x21ed86, 'af866e3bd35be84b2e79ec791f201a9715e87eee53caaa01f08c1d1b63fc605b'),
            (0x2eaf74, 0x2eafc6, 'cbef82273be309607dd124561497f83ab0d47d884adb2019adb984cc2c0cc9f9'),
            (0x305228, 0x3052b2, '0e2336faf53cb572df0e9474a0d838e0d7c2959b5fd4741bcb1c909deadd5666')):
        if hashlib.sha256(image[start - 0x200000:end - 0x200000]).hexdigest() != digest:
            raise ValueError('own acquisition configuration producer differs')


def verify_bookkeeping_trace(text):
    """Check captured saved-descriptor ownership, not DSP measurement lifetime."""
    pending = None
    completion = None
    count = 0
    previous_time = -1.0
    for line in text.splitlines():
        if '8210_channel_config_bookkeeping:' not in line:
            continue
        fields = dict(re.findall(r'(\w+)=(\S+)', line))
        required = {'pc', 'caller', 'argument', 'saved', 'data', 'controller', 'state', 't'}
        if not required <= fields.keys():
            raise ValueError('incomplete bookkeeping observation')
        time = float(fields['t'])
        if time < previous_time:
            raise ValueError('unordered bookkeeping observations')
        previous_time = time
        if fields['pc'] == '002eaf74':
            saved = int(fields['saved'], 16)
            argument = int(fields['argument'], 16)
            if saved == 0 and argument != 0:
                if pending or completion:
                    raise ValueError('overlapping configuration ownership')
                pending = True
            elif saved != 0 and argument == 0:
                if not pending or fields['caller'] != '002df295':
                    raise ValueError('configuration completion lacks matching ownership')
                descriptor = bytes.fromhex(fields['data'])
                controller = bytearray.fromhex(fields['controller'])
                if len(descriptor) != 12 or len(controller) != 12:
                    raise ValueError('unexpected bookkeeping capture length')
                for destination, origin in ((6, 2), (4, 8), (5, 9),
                                            (7, 6), (8, 1), (9, 10)):
                    controller[destination] = descriptor[origin]
                completion = bytes(controller)
                pending = None
            else:
                raise ValueError('unsupported bookkeeping path in observation')
        elif fields['pc'] == '00305228' and fields['caller'] == '002eafc5':
            if completion is None or int(fields['saved'], 16) != 0:
                raise ValueError('dispatch before saved-descriptor release')
            if bytes.fromhex(fields['controller']) != completion:
                raise ValueError('completed controller differs from saved descriptor')
            completion = None
            count += 1
    if not count or pending or completion:
        raise ValueError('missing or incomplete configuration completion')
    return count


def acquisition_descriptor(record):
    """Translate the observed record copy at 287390 -> 2eb226.

    The common helper 2eaf74 also changes firmware bookkeeping, outside this
    byte-copy reference. This is not a complete behavioral implementation.
    """
    if len(record) != 24:
        raise ValueError('requires a 24-byte acquisition record capture')
    descriptor = bytearray(24)
    descriptor[0:2] = b'\x04\x50'
    descriptor[8:10] = record[6:8]
    descriptor[6] = record[10]
    descriptor[12:16] = record[0:4]
    return bytes(descriptor)


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

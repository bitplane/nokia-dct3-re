"""Pure observed ROM4 record arithmetic; no handset identity or provisioning.

The acquired ROM4 data image holds the lock table at b6df and encoding
schedule at b6f7. Callers must supply the observed four-byte chip field.
The retained transform is independently present in NSM-1 and NSK-1 MCU code.
"""
import struct

from tools.nse5_transform_trace_check import inverse_transform_words,transform_words


LOCK_TABLE_82 = bytes.fromhex('d1a25feb4ff02d7b0f4ce1c3')
ENCODE_SCHEDULE = tuple(value*0x101 for value in bytes.fromhex('b173e65aab478e0d1a34680b'))


def retained_record_transform(record: bytes, identity_record: bytes) -> bytes:
    if len(record) != 24 or len(identity_record) != 12:
        raise ValueError('request/identity record must contain twenty-four/twelve bytes')
    work = bytearray(identity_record*2)
    for index in range(0,24,2):
        product = work[index]*work[index+1]
        work[index:index+2] = product.to_bytes(2,'little')
    pad = []
    for value in reversed(work):
        reversed_complement = 0
        for bit in range(8):
            reversed_complement = (reversed_complement << 1) | (1 ^ ((value >> bit) & 1))
        pad.append(reversed_complement)
    return bytes(value ^ mask for value,mask in zip(record,pad))


def decode_record(encoded: bytes, chip: bytes) -> bytes:
    if len(encoded) != 12 or len(chip) != 4:
        raise ValueError('record/chip must contain twelve/four bytes')
    table = bytes(value ^ (chip[index] if index < 4 else 0)
                  for index,value in enumerate(LOCK_TABLE_82))
    words = inverse_transform_words(struct.unpack('>6H',encoded),
                                    struct.unpack('>6H',table),ENCODE_SCHEDULE)
    return struct.pack('>6H',*words)


def encode_record(plain: bytes, chip: bytes) -> bytes:
    """Encode explicit caller-supplied data, without selecting valid policy fields."""
    if len(plain) != 12 or len(chip) != 4:
        raise ValueError('record/chip must contain twelve/four bytes')
    table = bytes(value ^ (chip[index] if index < 4 else 0)
                  for index,value in enumerate(LOCK_TABLE_82))
    words = transform_words(struct.unpack('>6H',plain),
                            struct.unpack('>6H',table),ENCODE_SCHEDULE)
    return struct.pack('>6H',*words)

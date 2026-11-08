"""Offline ROM6 record arithmetic; callers own table, identity and policy.

The acquired resident wrappers8508/8513/8529 copy six DROM key words,
XOR the chip's32 bits into the first pair, then8555 applies the linear
preconditioning and bit reversal before the shared twelve-round transform.
No handset configuration, storage editing or validation flag is supplied.
"""
import struct

from tools.nse5_transform_trace_check import (
    inverse_transform_words,linear_mix,reverse_words,transform_words,
)

SCHEDULE=tuple(value*257 for value in bytes.fromhex('d0162c58b071e2d55a67ce8d'))


def effective_key(table,chip):
    if not isinstance(table,bytes) or len(table)!=12 or not isinstance(chip,bytes) or len(chip)!=4:
        raise ValueError('ROM6 record transform requires twelve table bytes and four chip bytes')
    mixed=bytes(value^(chip[index] if index<4 else 0) for index,value in enumerate(table))
    return reverse_words(linear_mix(struct.unpack('>6H',mixed)))


def decode_record(encoded,chip,table):
    if not isinstance(encoded,bytes) or len(encoded)!=12:
        raise ValueError('ROM6 encoded record must contain twelve bytes')
    return struct.pack('>6H',*transform_words(struct.unpack('>6H',encoded),effective_key(table,chip),SCHEDULE))


def encode_record(plain,chip,table):
    """Inverse arithmetic only; caller-supplied plaintext need not be valid policy."""
    if not isinstance(plain,bytes) or len(plain)!=12:
        raise ValueError('ROM6 plaintext record must contain twelve bytes')
    return struct.pack('>6H',*inverse_transform_words(struct.unpack('>6H',plain),effective_key(table,chip),SCHEDULE))

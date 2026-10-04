"""Offline MSID codec using the independently recovered DSP word transform.

Numeric decoder tables were published by myke2002 in the 2003 DCT3 MBUS
discussion. The algorithm here is the native-observed ROM4 word model and
its independently derived inverse, not a transcription of that source.
This module does not select a handset's family, chip ID or provisioning.
"""

import struct

try:
    from tools.nse5_transform_trace_check import transform_words, inverse_transform_words
except ModuleNotFoundError:
    from nse5_transform_trace_check import transform_words, inverse_transform_words


DECODER_TABLES = {
    0x82: bytes.fromhex("9f7aad34e77927734e410d26"),
    0x83: bytes.fromhex("50f36525d2b1c1b609aeff4c"),
}
DECODE_SCHEDULE = tuple(value * 0x101 for value in
                        bytes.fromhex("d0162c58b071e2d55a67ce8d"))


def _table(family):
    if family not in DECODER_TABLES:
        raise ValueError("unsupported MSID family; no automatic handset selection")
    return struct.unpack(">6H", DECODER_TABLES[family])


def decode_msid(msid):
    if len(msid) != 13:
        raise ValueError("MSID must contain one family byte and twelve codec bytes")
    words = transform_words(struct.unpack(">6H", msid[1:]),
                            _table(msid[0]), DECODE_SCHEDULE)
    return struct.pack(">6H", *words)


def encode_msid(plain, family):
    """Encode caller-supplied checksum/chip/signature bytes, without inventing them."""
    if len(plain) != 12:
        raise ValueError("MSID plaintext must contain exactly twelve bytes")
    words = inverse_transform_words(struct.unpack(">6H", plain),
                                    _table(family), DECODE_SCHEDULE)
    return bytes([family]) + struct.pack(">6H", *words)

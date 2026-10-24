"""Bounded M2BUS frame encoding/validation, independent of handset semantics."""
from dataclasses import dataclass
from functools import reduce
from operator import xor
import struct


@dataclass(frozen=True)
class Frame:
    destination: int
    source: int
    kind: int
    sequence: int
    payload: bytes = b''

    def encode(self):
        if any(not 0 <= value <= 255 for value in
               (self.destination, self.source, self.kind, self.sequence)):
            raise ValueError('M2BUS header field outside byte range')
        if len(self.payload) > 65535 or (self.kind == 0x7f and self.payload):
            raise ValueError('invalid M2BUS payload extent')
        header = bytes((0x1f, self.destination, self.source, self.kind))
        body = header + (b'' if self.kind == 0x7f else struct.pack('>H', len(self.payload)) + self.payload)
        body += bytes((self.sequence,))
        return body + bytes((reduce(xor, body),))


def decode(data):
    if len(data) < 6 or data[0] != 0x1f or reduce(xor, data):
        raise ValueError('invalid M2BUS marker/checksum')
    if data[3] == 0x7f:
        expected, payload = 6, b''
    else:
        if len(data) < 8:
            raise ValueError('short M2BUS frame')
        expected, payload = 8 + int.from_bytes(data[4:6], 'big'), data[6:-2]
    if len(data) != expected:
        raise ValueError('M2BUS length mismatch')
    return Frame(data[1], data[2], data[3], data[-2], payload)

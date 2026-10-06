"""Decode big-endian startup copy records without executing their writes."""
import struct


def decode_startup_records(image, image_base, table_address):
    """Return ordered (source, destination, payload) records and terminator end.

    Destinations may be MMIO, and overlapping writes remain ordered. The
    zero-size terminator is only one word, not a complete record header.
    """
    cursor = table_address - image_base
    records = []
    while True:
        if cursor < 0 or cursor + 4 > len(image):
            raise ValueError('truncated startup size/terminator')
        size = struct.unpack_from('>I', image, cursor)[0]
        if not size:
            return records, image_base + cursor + 4
        if cursor + 8 > len(image) or size > len(image) - cursor - 8:
            raise ValueError('truncated startup record')
        destination = struct.unpack_from('>I', image, cursor + 4)[0]
        if destination + size > 0x100000000:
            raise ValueError('startup destination overflow')
        start = cursor + 8
        records.append((image_base + start, destination, image[start:start + size]))
        cursor = (start + size + 3) & ~3


def initialized_bytes(records, address, size):
    """Read fully covered initialized bytes, respecting last-writer ownership."""
    result = bytearray(size)
    covered = bytearray(size)
    for _, destination, payload in records:
        low = max(address, destination)
        high = min(address + size, destination + len(payload))
        if low < high:
            result[low - address:high - address] = payload[low - destination:high - destination]
            covered[low - address:high - address] = b'\x01' * (high - low)
    if not all(covered):
        raise ValueError('uncovered startup destination')
    return bytes(result)

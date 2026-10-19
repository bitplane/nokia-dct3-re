"""Bounded AAC-LC configuration relocation, not decoder conformance."""


def program_configuration_prefix(asc):
    """Relocate an explicit AAC-LC PCE into an in-band ID_PCE element.

    Preserve syntax fields and comment bytes, recomputing byte alignment at
    the new bit origin. Unsupported GASpecificConfig extensions fail closed.
    """
    bits = ''.join(f'{byte:08b}' for byte in asc)
    cursor = 0

    def read(width):
        nonlocal cursor
        if cursor + width > len(bits):
            raise ValueError('truncated AudioSpecificConfig/PCE')
        value = bits[cursor:cursor+width]
        cursor += width
        return int(value, 2) if width else 0

    object_type, frequency, channels = read(5), read(4), read(4)
    if object_type != 2 or frequency >= 13 or channels != 0:
        raise ValueError('requires AAC-LC indexed frequency and explicit PCE')
    if read(3):
        raise ValueError('unsupported GASpecificConfig flags')
    start = cursor
    read(4)  # element_instance_tag
    pce_object, pce_frequency = read(2), read(4)
    if pce_object != 1 or pce_frequency != frequency:
        raise ValueError('PCE and AudioSpecificConfig disagree')
    front, side, back, lfe, assoc, cc = (read(n) for n in (4, 4, 4, 2, 3, 4))
    if read(1):
        read(4)  # mono_mixdown_element_number
    if read(1):
        read(4)  # stereo_mixdown_element_number
    if read(1):
        read(3)  # matrix_mixdown_idx and pseudo_surround_enable
    for count, width in ((front+side+back, 5), (lfe+assoc, 4), (cc, 5)):
        for _ in range(count):
            read(width)
    body = bits[start:cursor]
    padding = (-cursor) % 8
    if read(padding):
        raise ValueError('nonzero source PCE alignment padding')
    comment_start = cursor
    comment_bytes = read(8)
    read(comment_bytes * 8)
    comment = bits[comment_start:cursor]
    if cursor != len(bits):
        raise ValueError('unhandled AudioSpecificConfig tail')
    relocated = '101' + body  # AAC syntactic element ID_PCE
    relocated += '0' * ((-len(relocated)) % 8)
    relocated += comment
    return bytes(int(relocated[i:i+8], 2) for i in range(0, len(relocated), 8))

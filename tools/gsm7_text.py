"""GSM 03.38 default/extension alphabet and SMS/USSD text packing.

TS 23.038 v3.3.0 sections 6.1.2.3, 6.2.1 and 6.2.1.1. No national
language tables or implicit replacement/UCS-2 conversion are performed.
"""

DEFAULT_ALPHABET = (
    '@\u00a3$\u00a5\u00e8\u00e9\u00f9\u00ec\u00f2\u00c7\n\u00d8\u00f8\r\u00c5\u00e5'
    '\u0394_\u03a6\u0393\u039b\u03a9\u03a0\u03a8\u03a3\u0398\u039e\x1b\u00c6\u00e6\u00df\u00c9'
    ' !"#\u00a4%&\'()*+,-./0123456789:;<=>?'
    '\u00a1ABCDEFGHIJKLMNOPQRSTUVWXYZ\u00c4\u00d6\u00d1\u00dc\u00a7'
    '\u00bfabcdefghijklmnopqrstuvwxyz\u00e4\u00f6\u00f1\u00fc\u00e0'
)
DEFAULT_CODES = {character: index for index, character in enumerate(DEFAULT_ALPHABET)
                 if character != '\x1b'}
EXTENSION_CODES = {
    '\f': 0x0a, '^': 0x14, '{': 0x28, '}': 0x29, '\\': 0x2f,
    '[': 0x3c, '~': 0x3d, ']': 0x3e, '|': 0x40, '\u20ac': 0x65,
}


def encode_text(text: str, *, ussd: bool = False) -> tuple[str, int] | None:
    """Return packed hex and the unpadded septet count, or reject unmappable text."""
    septets = bytearray()
    for character in text:
        if character in DEFAULT_CODES:
            septets.append(DEFAULT_CODES[character])
        elif character in EXTENSION_CODES:
            septets.extend((0x1b, EXTENSION_CODES[character]))
        else:
            return None
    count = len(septets)
    if ussd:
        # USSD has no TP-UDL: spare zero septets must not appear as '@',
        # and a real final CR on an octet boundary must survive stripping.
        if count % 8 == 7 or (count and count % 8 == 0 and septets[-1] == 0x0d):
            septets.append(0x0d)
    accumulator = bits = 0
    packed = bytearray()
    for value in septets:
        accumulator |= value << bits
        bits += 7
        while bits >= 8:
            packed.append(accumulator & 0xff)
            accumulator >>= 8
            bits -= 8
    if bits:
        packed.append(accumulator & 0xff)
    return packed.hex(), count

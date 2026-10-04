"""Check the recovered 8250 service-record envelope, not its identity verdict."""

import argparse
import json
import struct


def inspect_record_reply(payload):
    """Inspect compact type-74 primitive 35 bytes before MCU reframing.

    MCU 28d286..28d2c6 accepts size 32 directly or size 34 with a
    modulo-16-bit sum of 25 big-endian words XOR ffff. Other context
    gates and the decoded record's meaning are intentionally not modeled.
    """
    if len(payload) < 2 or payload[0] != 0x35:
        raise ValueError("expected primitive 35 and its size byte")
    size = payload[1]
    if size not in (0x32, 0x34):
        raise ValueError("unsupported record size")
    if len(payload) != size + 2:
        raise ValueError("record size does not match payload extent")
    result = {
        "primitive": 0x35, "size": size,
        "checksum_present": size == 0x34,
        "identity_verdict": "not evaluated",
    }
    if size == 0x34:
        words = struct.unpack_from(">25H", payload, 2)
        expected = (sum(words) & 0xffff) ^ 0xffff
        stored = struct.unpack_from(">H", payload, 52)[0]
        result.update(checksum_expected=expected, checksum_stored=stored,
                      checksum_matches=stored == expected)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("payload", help="hexadecimal compact DSP response")
    args = parser.parse_args()
    try:
        result = inspect_record_reply(bytes.fromhex(args.payload))
    except ValueError as error:
        parser.exit(1, f"8250 service envelope: {error}\n")
    print(json.dumps(result, indent=2))
    if result.get("checksum_matches") is False:
        parser.exit(1, "8250 service envelope: checksum mismatch\n")


if __name__ == "__main__":
    main()

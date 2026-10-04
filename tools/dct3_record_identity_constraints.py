"""Optional offline constraint experiment; never provisions a handset.

Requires z3-solver separately. The family, no-preprocessing assumption and
ROM4 marker contract are hypotheses, not claims about fitted ROM6 silicon.
"""

import argparse
import hashlib
import json
from pathlib import Path
import struct

from tools.nse5_transform_trace_check import inverse_transform_words


ROWS = (0x4ed, 0x89f, 0x3b5, 0x27e, 0xed4, 0x9f8,
        0xb53, 0x7e2, 0xd4e, 0xf89, 0x53b, 0xe27)
KEY = bytes.fromhex("7bb4d0ef9eb20abe73dad335")
SCHEDULE = bytes.fromhex("b173e65aab478e0d1a34680b")


def inverse_coefficients():
    inverse = [0] * 8
    for value in range(8):
        output = sum((((value >> i) & 1) ^
                      (((value >> ((i + 1) % 3)) & 1) |
                       (((value >> ((i + 2) % 3)) & 1) ^ 1))) << i
                     for i in range(3))
        inverse[output] = value
    coefficients = []
    for bit in range(3):
        values = [(value >> bit) & 1 for value in inverse]
        for variable in range(3):
            for mask in range(8):
                if mask & (1 << variable):
                    values[mask] ^= values[mask ^ (1 << variable)]
        coefficients.append(values)
    return coefficients


def symbolic_inverse(z3, encoded, table):
    def rotate(data, start, count):
        word = z3.RotateRight(z3.Concat(*data[start:start + 4]), count)
        data[start:start + 4] = [z3.Extract(31 - i * 8, 24 - i * 8, word)
                                for i in range(4)]

    data = [z3.BitVecVal(int(f"{byte:08b}"[::-1], 2), 8)
            for byte in reversed(encoded)]
    coefficients = inverse_coefficients()
    for round_index in reversed(range(12)):
        if round_index < 11:
            rotate(data, 0, 1)
            rotate(data, 8, 22)
            decoded = [None] * 12
            for group in range(4):
                inputs = [data[group + 4 * i] for i in range(3)]
                for bit, polynomial in enumerate(coefficients):
                    value = z3.BitVecVal(0, 8)
                    for mask, active in enumerate(polynomial):
                        if active:
                            term = z3.BitVecVal(255, 8)
                            for variable in range(3):
                                if mask & (1 << variable):
                                    term = term & inputs[variable]
                            value = value ^ term
                    decoded[group + 4 * bit] = value
            data = decoded
            rotate(data, 0, 22)
            rotate(data, 8, 1)
        mixed = []
        for row in ROWS:
            value = z3.BitVecVal(0, 8)
            for index in range(12):
                if row & (1 << index):
                    value = value ^ data[index]
            mixed.append(value)
        for index in (2, 3, 8, 9):
            mixed[index] = mixed[index] ^ SCHEDULE[round_index]
        data = [mixed[index] ^ table[index] for index in range(12)]
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pmm", type=Path)
    parser.add_argument("--timeout-ms", type=int, default=30000)
    parser.add_argument("--all-distinct", action="store_true",
                        help="constrain both distinct low-record ciphertexts with one chip")
    args = parser.parse_args()
    try:
        import z3
    except ImportError:
        parser.exit(1, "optional experiment requires z3-solver\n")
    if args.timeout_ms <= 0:
        parser.error("timeout must be positive")
    image = args.pmm.read_bytes()
    if hashlib.sha1(image).hexdigest() != "a974fb5fddcd0438ac4aaf32b431f1453e8d923c":
        parser.error("not the pinned acquired 8250 PMM; no donor input accepted")
    raw = image[0x10046:0x1005e]
    if len(raw) != 24:
        parser.error("PMM lacks the acquired layout's first 24-byte record")
    # Check this separate symbolic implementation against the native word
    # inverse before asking it a question with an unknown identity.
    fixed = KEY[:1] + bytes((KEY[1] ^ 0x16, KEY[2], KEY[3] ^ 0x10)) + KEY[4:]
    for start in (0, 12):
        actual = bytes(z3.simplify(value).as_long() for value in
                       symbolic_inverse(z3, raw[start:start + 12], fixed))
        expected = struct.pack(">6H", *inverse_transform_words(
            struct.unpack(">6H", raw[start:start + 12]),
            struct.unpack(">6H", fixed), tuple(value * 257 for value in SCHEDULE)))
        if actual != expected:
            parser.exit(1, "symbolic/native-word codec disagreement\n")
    chip = z3.BitVec("chip24", 24)
    chip_bytes = [z3.BitVecVal(0, 8), z3.Extract(23, 16, chip),
                  z3.Extract(15, 8, chip), z3.Extract(7, 0, chip)]
    table = [KEY[index] ^ chip_bytes[index] if index < 4
             else z3.BitVecVal(KEY[index], 8) for index in range(12)]
    solver = z3.SolverFor("QF_BV")
    solver.set(timeout=args.timeout_ms)
    records = [raw]
    if args.all_distinct:
        records = list(dict.fromkeys(image[0x10046 + index * 24:0x1005e + index * 24]
                                    for index in range(10)))
    for record in records:
        for start in (0, 12):
            decoded = symbolic_inverse(z3, record[start:start + 12], table)
            solver.add(decoded[10] == 0x54, decoded[11] == 0xc2)
    result = solver.check()
    report = {"result": str(result), "timeout_ms": args.timeout_ms,
              "model": "family83/raw-input/24-bit-chip/ROM4-marker54c2",
              "distinct_records": len(records),
              "codec_crosscheck": "passed", "provisioning": "none"}
    if result == z3.unknown:
        report["reason"] = solver.reason_unknown()
    elif result == z3.sat:
        report["candidate_chip"] = f"{solver.model()[chip].as_long():06x}"
        report["candidate_is_original_identity"] = "not established"
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Check observed ROM4 rotation and mixing helpers against word arithmetic."""

import argparse
from pathlib import Path
import re


ROTATION = re.compile(
    r"nse5_compat_rotation: base=([0-9a-f]{4}) "
    r"input=([0-9a-f:]+) output=([0-9a-f:]+) other=([0-9a-f]{4}) "
    r"other_input=([0-9a-f:]+) other_output=([0-9a-f:]+) t=([0-9.]+)"
)
MIX = re.compile(r"nse5_compat_mix: input=([0-9a-f:]+) output=([0-9a-f:]+) t=([0-9.]+)")
REVERSE = re.compile(
    r"nse5_compat_reverse: base=([0-9a-f]{4}) input=([0-9a-f:]+) "
    r"output=([0-9a-f:]+) t=([0-9.]+)"
)
ROUNDS = re.compile(r"nse5_compat_rounds: count=(\d+) schedule=([0-9a-f]{4}) base=([0-9a-f]{4}) t=([0-9.]+)")
NONLINEAR = re.compile(r"nse5_compat_nonlinear: input=([0-9a-f:]+) output=([0-9a-f:]+) t=([0-9.]+)")
CODEC = re.compile(r"nse5_compat_codec: input=([0-9a-f:]+) table=([0-9a-f:]+) schedule=([0-9a-f:]+) output=([0-9a-f:]+) t=([0-9.]+)")


def check_codec(text: str) -> int:
    count = 0
    for match in CODEC.finditer(text):
        words, table, schedule, output = (
            tuple(int(word, 16) for word in match[index].split(":"))
            for index in range(1, 5)
        )
        if transform_words(words, table, schedule) != output:
            raise ValueError(f"complete transform mismatch at t={match[5]}")
        count += 1
    if not count:
        raise ValueError("no complete transform observations")
    return count


def nonlinear_words(words: tuple[int, ...]) -> tuple[int, ...]:
    if len(words) != 6:
        raise ValueError("nonlinear stage requires six words")
    return tuple(words[i] ^ (words[(i + 2) % 6] | (words[(i + 4) % 6] ^ 0xffff))
                 for i in range(6))


def linear_mix(words: tuple[int, ...]) -> tuple[int, ...]:
    return (mix_words(words) + mix_words(words[2:] + words[:2]) +
            mix_words(words[4:] + words[:4]))


def reverse_words(words: tuple[int, ...]) -> tuple[int, ...]:
    return tuple(int(f"{word:016b}"[::-1], 2) for word in reversed(words))


def transform_words(words: tuple[int, ...], table: tuple[int, ...],
                    schedule: tuple[int, ...]) -> tuple[int, ...]:
    if len(words) != 6 or len(table) != 6 or len(schedule) != 12:
        raise ValueError("transform requires six data/table words and twelve schedule words")
    for index, round_word in enumerate(schedule):
        mixed = [word ^ key for word, key in zip(words, table)]
        mixed[1] ^= round_word
        mixed[4] ^= round_word
        words = linear_mix(tuple(mixed))
        if index < 11:
            words = rotate32(*words[:2], 10) + words[2:4] + rotate32(*words[4:], 31)
            words = nonlinear_words(words)
            words = rotate32(*words[:2], 31) + words[2:4] + rotate32(*words[4:], 10)
    return reverse_words(words)


def check_nonlinear(text: str) -> int:
    count = 0
    for match in NONLINEAR.finditer(text):
        before = tuple(int(word, 16) for word in match[1].split(":"))
        after = tuple(int(word, 16) for word in match[2].split(":"))
        if after != nonlinear_words(before):
            raise ValueError(f"nonlinear mismatch at t={match[3]}")
        count += 1
    if not count:
        raise ValueError("no observed nonlinear stage records")
    return count


def check_structure(text: str) -> tuple[int, int]:
    reversals = 0
    for match in REVERSE.finditer(text):
        before = tuple(int(word, 16) for word in match[2].split(":"))
        after = tuple(int(word, 16) for word in match[3].split(":"))
        if len(before) != 6 or len(after) != 6:
            raise ValueError("reversal requires six words per operand")
        expected = tuple(int(f"{word:016b}"[::-1], 2) for word in reversed(before))
        if after != expected:
            raise ValueError(f"bit reversal mismatch at t={match[4]}")
        reversals += 1
    rounds = list(ROUNDS.finditer(text))
    if not reversals or not rounds:
        raise ValueError("missing reversal or round observations")
    if any(int(match[1]) != 11 for match in rounds):
        raise ValueError("round loop must execute eleven iterations before its final mix")
    return reversals, len(rounds)


def mix_words(words: tuple[int, ...]) -> tuple[int, int]:
    # Straight-line 0x7fb1..0x7fe7, with AR3's three source addresses
    # resolved by the observation. TI SPRU172C defines the logical shifts
    # and unextended Smem XORs; this is not a general CPU interpreter.
    if len(words) != 6:
        raise ValueError("mix requires three resolved word pairs")
    x, y, u, v, p, q = words
    a = x ^ (x << 8) ^ y
    b = y ^ (y << 8) ^ x

    def fold(value: int) -> int:
        value = (value & 0xffff) << 8
        return value | (value >> 16)

    b = fold(b)
    b = fold(b ^ x)
    b ^= u
    a ^= v
    a = (a << 8) & 0xffffffff
    a ^= v
    a >>= 8
    b ^= p
    b = (b << 8) & 0xffffffff
    b ^= p
    b >>= 8
    b = fold(b)
    b = fold(b ^ q)
    a = fold(a ^ q)
    a = fold(a ^ p ^ q)
    return a & 0xffff, b & 0xffff


def check_mix(text: str) -> int:
    count = 0
    for match in MIX.finditer(text):
        before = tuple(int(word, 16) for word in match[1].split(":"))
        after = tuple(int(word, 16) for word in match[2].split(":"))
        if after != mix_words(before):
            raise ValueError(f"mix mismatch at t={match[3]}")
        count += 1
    if not count:
        raise ValueError("no observed mixing helper records")
    return count


def rotate32(high: int, low: int, count: int) -> tuple[int, int]:
    value = (high << 16) | low
    value = ((value >> count) | (value << (32 - count))) & 0xffffffff
    return value >> 16, value & 0xffff


def check_trace(text: str) -> int:
    count = 0
    for match in ROTATION.finditer(text):
        before = tuple(int(word, 16) for word in match[2].split(":"))
        after = tuple(int(word, 16) for word in match[3].split(":"))
        other_before = tuple(int(word, 16) for word in match[5].split(":"))
        other_after = tuple(int(word, 16) for word in match[6].split(":"))
        if any(len(words) != 2 for words in (before, after, other_before, other_after)):
            raise ValueError("rotation record requires two words per operand")
        if after != rotate32(*before, 10) or other_after != rotate32(*other_before, 31):
            raise ValueError(f"rotation mismatch at {match[1]}, t={match[7]}")
        count += 1
    if not count:
        raise ValueError("no observed rotation helper records")
    return count


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path)
    args = parser.parse_args()
    try:
        count = check_trace(args.trace.read_text())
        mix_count = check_mix(args.trace.read_text())
        reversal_count, round_count = check_structure(args.trace.read_text())
        nonlinear_count = check_nonlinear(args.trace.read_text())
        codec_count = check_codec(args.trace.read_text())
    except (OSError, ValueError) as error:
        parser.exit(1, f"NSE-5 transform check: {error}\n")
    print(f"NSE-5 transform helpers: {count} rotations and {mix_count} mixes match word arithmetic")
    print(f"NSE-5 transform structure: {reversal_count} reversals and {round_count} round loops checked")
    print(f"NSE-5 nonlinear stage: {nonlinear_count} calls checked")
    print(f"NSE-5 complete transforms: {codec_count} match independent word model")


if __name__ == "__main__":
    main()

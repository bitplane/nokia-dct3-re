#!/usr/bin/env python3
"""Verify the NSE-8 v6.00 Cell Broadcast firmware-consumer contract."""

from __future__ import annotations

import argparse
from pathlib import Path

import capstone


FLASH_BASE = 0x200000

ANCHORS = {
	# Task 22 maps message classes 5, 7 and 0x0a to the shared primitive
	# handler.  Class 7 is reached after subtracting 2, 1, 2 from the class.
	0x23D646: ("ldrb", "r1, [r4, #3]"),
	0x23D64E: ("subs", "r0, r1, #2"),
	0x23D654: ("subs", "r0, #1"),
	0x23D65A: ("subs", "r0, #2"),
	0x23D6D6: ("movs", "r0, #9"),
	0x23D6DA: ("bl", "#0x23cde0"),
	# Primitive 0x30 selects the 12-byte descriptor path.
	0x23CE30: ("subs", "r0, #0xb"),
	0x23CE34: ("beq", "#0x23ceba"),
	0x23CEBA: ("movs", "r0, #0xc"),
	0x23CEC2: ("movs", "r0, #0xaa"),
	# Four leading bytes are copied verbatim, followed by a big-endian
	# length, capped at 0xaa, and a blob beginning at message byte 7.
	0x23CECA: ("movs", "r0, #7"),
	0x23CED0: ("strb", "r0, [r4]"),
	0x23CED4: ("strb", "r0, [r4, #1]"),
	0x23CED8: ("strb", "r0, [r4, #2]"),
	0x23CEDC: ("strb", "r0, [r4, #3]"),
	0x23CEE2: ("lsls", "r2, r2, #8"),
	0x23CEE4: ("orrs", "r0, r2"),
	0x23CEEA: ("cmp", "r2, #0xaa"),
	0x23CEF4: ("bl", "#0x2b5c7c"),
	0x23CEFA: ("bl", "#0x2b2ec8"),
	# The router posts the descriptor into the task-5 MMI pipeline.
	0x2B2EC8: ("push", "{lr}"),
	0x2B2ECC: ("ldr", "r0, [pc, #0xa0]"),
	0x2B2ECE: ("bl", "#0x2af6ea"),
	# The only modeled physical DSP ingress is the task-4 packet ring.  It
	# constructs the broker envelope consumed by the closed type switch.
	0x2908D6: ("movs", "r1, #0x18"),
	0x2908DE: ("strb", "r1, [r0, #1]"),
	0x2908E2: ("strb", "r7, [r0, #2]"),
	# The separate framed-session task does not accept class 7: its lower
	# branch admits 3, 5, 17, 19 and 20 before the class-0x40 cases.
	0x237BDE: ("ldrb", "r0, [r4, #3]"),
	0x237BE6: ("subs", "r0, r0, #3"),
	0x237BEC: ("subs", "r0, #2"),
	0x237BF2: ("subs", "r0, #0xc"),
	0x237BF8: ("subs", "r0, #2"),
	0x237BFA: ("cmp", "r0, #1"),
}

LITERALS = {
	0x2B2ECC: 0x00005978,
}


def decode_instruction(image: bytes, pc: int) -> capstone.CsInsn:
	decoder = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
	decoder.detail = True
	offset = pc - FLASH_BASE
	decoded = list(decoder.disasm(image[offset:offset + 4], pc, count=1))
	if not decoded:
		raise ValueError(f"no Thumb instruction at {pc:#x}")
	return decoded[0]


def literal_value(image: bytes, instruction: capstone.CsInsn) -> int:
	operand = instruction.operands[1]
	if instruction.mnemonic != "ldr" or operand.type != capstone.arm.ARM_OP_MEM or \
			operand.mem.base != capstone.arm.ARM_REG_PC:
		raise ValueError(f"expected literal load at {instruction.address:#x}")
	address = ((instruction.address + 4) & ~3) + operand.mem.disp
	offset = address - FLASH_BASE
	raw = int.from_bytes(image[offset:offset + 4], "little")
	return ((raw & 0xffff) << 16) | (raw >> 16)


def verify_contract(data: bytes) -> dict[str, int]:
	# The repository image is already in the halfword order consumed by the
	# little-endian ARM disassembler.  Only 32-bit pool literals need their
	# two halfwords exchanged.
	image = data
	for pc, expected in ANCHORS.items():
		instruction = decode_instruction(image, pc)
		actual = instruction.mnemonic, instruction.op_str
		if actual != expected:
			raise ValueError(f"Thumb anchor {pc:#x}: expected {expected}, got {actual}")
	for pc, expected in LITERALS.items():
		actual = literal_value(image, decode_instruction(image, pc))
		if actual != expected:
			raise ValueError(
				f"literal {pc:#x}: expected {expected:#x}, got {actual:#x}")
	return {
		"message_class": 7,
		"primitive": 0x30,
		"header_octets": 4,
		"maximum_blob_octets": 0xAA,
		"mmi_event_literal": 0x5978,
	}


def main() -> None:
	parser = argparse.ArgumentParser()
	parser.add_argument("image", type=Path)
	args = parser.parse_args()
	result = verify_contract(args.image.read_bytes())
	print(
		"OK - NSE-8 class-7 primitive-0x30 copies a four-octet CBS header, "
		f"a bounded blob (max {result['maximum_blob_octets']}) and routes it to MMI")


if __name__ == "__main__":
	main()

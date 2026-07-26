#!/usr/bin/env python3
"""Verify reproducible NSE-3 v4.06 reset, peripheral and DSPIF boundaries.

This checker is deliberately firmware-specific.  It proves properties of the
identified normalized image; it does not assign semantics to MAD2 registers or
claim compatibility with a particular internal ROM revision.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

import capstone

try:
    from tools.mad2_static_census import analyze_image
    from tools.message_census import decode_image, immediate_target, literal_value
except ModuleNotFoundError:  # Direct execution from tools/.
    from mad2_static_census import analyze_image
    from message_census import decode_image, immediate_target, literal_value


FLASH_BASE = 0x200000
FLASH_SIZE = 0x100000
SRAM_BASE = 0x100000
SRAM_SIZE = 0x10000
EXPECTED_SHA1 = "5025a6ac3b4a13714211fde903f27f92cbb7c9b6"
VECTOR_SOURCE = 0x200180
KEYMAP_ADDRESS = 0x2BE8BC
KEYMAP = bytes.fromhex(
    "3e 3e 3e 3e 3e "
    "11 19 01 02 03 "
    "0e 17 04 05 06 "
    "0f 18 07 08 09 "
    "10 1a 0c 0a 0b"
)
SPECIAL_KEYMAP_ADDRESS = 0x2BE8D8
SPECIAL_KEYMAP = bytes.fromhex("3e 3e 3e 3e 0d")
EEPROM_ANCHORS = {
    # Two-byte word-address framing in the common serial-memory transaction.
    0x29CDD2: ("lsrs", "r0, r7, #8"),
    0x29CDE0: ("lsls", "r0, r7, #0x18"),
    0x29CDE2: ("lsrs", "r0, r0, #0x18"),
    # Low-level byte sender: GenIO signal 0x20, SDA mask 1, SCL mask 4.
    0x29E8C4: ("movs", "r4, #0x80"),
    0x29E8C8: ("adds", "r3, #0x20"),
    0x29E8CA: ("movs", "r0, #1"),
    0x29E8CC: ("movs", "r2, #4"),
    0x29E8D4: ("ldrb", "r1, [r3, #4]"),
    0x29E90E: ("orrs", "r5, r2"),
    0x29E936: ("bics", "r5, r2"),
}
SIMI_ANCHORS = {
    # Clock gate and initialization over the standard MAD2 SIMI window.
    0x290510: ("movs", "r0, #0x20"),
    0x290512: ("ldrb", "r1, [r4, #0xd]"),
    0x29051C: ("movs", "r1, #0x3d"),
    0x29051E: ("movs", "r0, #0x18"),
    0x29052A: ("movs", "r1, #0x3e"),
    0x29052C: ("movs", "r0, #0x1a"),
    0x290538: ("movs", "r0, #0x39"),
    0x29053A: ("movs", "r1, #0x32"),
    # Activation, TX FIFO staging/commit, RX drain, and IIR acknowledge.
    0x2900FA: ("movs", "r0, #0x80"),
    0x2901B0: ("strb", "r3, [r1, #5]"),
    0x2901B4: ("strb", "r3, [r1, #8]"),
    0x2901C2: ("strb", "r3, [r1]"),
    0x2901D0: ("strb", "r5, [r1, #8]"),
    0x2903DE: ("ldrb", "r0, [r5, #5]"),
    0x2903E4: ("ldrb", "r0, [r5]"),
    0x290462: ("ldrb", "r0, [r1]"),
    0x29046A: ("strb", "r0, [r1]"),
}
SIM_ATR_PPS_ANCHORS = {
    # The higher SIM manager accepts direct and inverse convention then walks
    # the interface-byte presence bits from T0/TDn.
    0x275240: ("ldrb", "r0, [r5, #1]"),
    0x275246: ("movs", "r1, #0x3b"),
    0x275248: ("subs", "r0, r0, r1"),
    0x27524E: ("subs", "r0, #4"),
    0x275254: ("ldrb", "r2, [r5, #2]"),
    0x275260: ("lsrs", "r3, r2, #5"),
    0x27527C: ("lsrs", "r2, r2, #1"),
    0x2752C2: ("mov", "r1, ip"),
    0x2752C4: ("cmp", "r0, r1"),
    # Ordinary TA1 values, including the lab card's 0x05, select a default
    # PPS request.  TA1=0x94 has its own independently checksummed request.
    0x2752D2: ("mov", "r0, sp"),
    0x2752D4: ("ldrb", "r0, [r0, #2]"),
    0x2752D6: ("movs", "r1, #0x11"),
    0x2752DE: ("subs", "r0, #0x83"),
    0x2752E4: ("movs", "r0, #1"),
    0x2752E8: ("movs", "r0, #0xff"),
    0x2752EA: ("strb", "r0, [r5, #0x18]"),
    0x2752EC: ("strb", "r6, [r5, #0x19]"),
    0x2752EE: ("strb", "r0, [r5, #0x1a]"),
    0x2752F2: ("movs", "r0, #2"),
    0x2752FC: ("strb", "r0, [r5, #0x18]"),
    0x2752FE: ("movs", "r0, #0x10"),
    0x275300: ("strb", "r0, [r5, #0x19]"),
    0x275302: ("movs", "r0, #0x94"),
    0x275304: ("strb", "r0, [r5, #0x1a]"),
    0x275306: ("movs", "r0, #0x7b"),
    0x275308: ("strb", "r0, [r5, #0x1b]"),
    # The parsed ATR is reached organically from the SIM manager loop.
    0x275C14: ("bl", "#0x27520a"),
}
SIM_T0_STATUS_ANCHORS = {
    # The SIM manager copies SW1/SW2 from the received T=0 object and
    # classifies the standards-defined families used by the lab card.
    0x2758F4: ("ldrb", "r0, [r1, #1]"),
    0x2758FA: ("ldrb", "r3, [r1, #2]"),
    0x275900: ("cmp", "r0, #0x92"),
    0x275906: ("movs", "r1, #0x67"),
    0x27590E: ("subs", "r0, #8"),
    0x275914: ("subs", "r0, #0x21"),
    0x27591A: ("subs", "r0, #1"),
    0x275922: ("movs", "r1, #0x93"),
    0x27592A: ("subs", "r0, #1"),
    0x275930: ("subs", "r0, #4"),
    0x275936: ("subs", "r0, #7"),
    # SW2 is further decoded for the 0x94 and 0x98 error families.
    0x275954: ("subs", "r0, r3, #2"),
    0x27595A: ("subs", "r0, #2"),
    0x275960: ("subs", "r0, #4"),
    0x275966: ("subs", "r0, #8"),
    0x27596C: ("subs", "r0, #0x30"),
    0x275972: ("subs", "r0, #0x10"),
}
DSPIF_ANCHORS = {
    # Doorbell write after a shared-memory request is accepted.
    0x28564C: ("ldr", "r1, [pc, #0x3b8]"),
    0x28564E: ("movs", "r0, #4"),
    0x285650: ("strh", "r0, [r1]"),
    # MCU-to-DSP ring construction, wrapping and producer commit.
    0x2856D2: ("ldr", "r3, [pc, #0x34c]"),
    0x2856D4: ("movs", "r2, #0xa4"),
    0x2856E6: ("cmp", "r5, r1"),
    0x2856EA: ("adds", "r5, r3, #0"),
    0x28572A: ("strh", "r0, [r2]"),
    # DSP-to-MCU occupancy and record consumption.
    0x28577A: ("ldr", "r0, [pc, #0x3c0]"),
    0x28577E: ("ldr", "r0, [pc, #0x3c0]"),
    0x28578C: ("adds", "r0, #0x64"),
    0x2857C8: ("ldr", "r4, [pc, #0x378]"),
    0x2857F6: ("strh", "r1, [r6, r0]"),
    # Cursor-pair free-space calculation and deterministic initialization.
    0x2858A0: ("ldr", "r0, [pc, #0x2a4]"),
    0x2858A2: ("ldrh", "r2, [r0]"),
    0x2858A4: ("ldrh", "r1, [r0, #2]"),
    0x285A42: ("ldr", "r0, [pc, #0x104]"),
    0x285A44: ("strh", "r1, [r0]"),
    0x285A46: ("ldr", "r3, [pc, #0xf8]"),
    0x285A48: ("movs", "r2, #0x80"),
    0x285A4A: ("strh", "r2, [r3]"),
    0x285A4C: ("strh", "r2, [r3, #2]"),
    0x285A4E: ("strh", "r1, [r0, #2]"),
}
DSPIF_LITERALS = {
    0x28564C: 0x30000,
    0x2856D2: 0x10000,
    0x28577A: 0x101CA,
    0x28577E: 0x101C8,
    0x2857C8: 0x10100,
    0x2858A0: 0x100A4,
    0x285A42: 0x100A4,
    0x285A46: 0x101C8,
}
RADIO_PACKET_ANCHORS = {
    # The normal L1 search path allocates a 0x48-byte queue object and emits
    # an independently constructed type-0x1a, length-68 DSP packet.
    0x20FB7C: ("movs", "r0, #0x48"),
    0x20FB84: ("movs", "r0, #2"),
    0x20FB8A: ("strh", "r0, [r1]"),
    0x20FB92: ("movs", "r0, #0x44"),
    0x20FB94: ("strb", "r0, [r1, #2]"),
    0x20FB96: ("movs", "r0, #0x1a"),
    0x20FB98: ("strb", "r0, [r1, #3]"),
    # A second control path constructs the same wire packet rather than
    # borrowing a static NSE-8 fixture.
    0x216F74: ("movs", "r0, #0x48"),
    0x216F7E: ("movs", "r2, #0x48"),
    0x216F84: ("movs", "r0, #2"),
    0x216F86: ("strh", "r0, [r4]"),
    0x216F88: ("movs", "r0, #0x44"),
    0x216F8A: ("strb", "r0, [r4, #2]"),
    0x216F8C: ("movs", "r0, #0x1a"),
    0x216F8E: ("strb", "r0, [r4, #3]"),
    # The task-side queue pump passes object +3 (type followed by body) and
    # the firmware-supplied length to the generic DSPIF packet writer.
    0x298C78: ("ldrb", "r1, [r5, #2]"),
    0x298C82: ("ldrb", "r0, [r5, #2]"),
    0x298C84: ("adds", "r1, r5, #3"),
    0x298C86: ("bl", "#0x285746"),
    # DSPIF RX materializes the received length at object +2 and type at +3.
    0x2857E6: ("ldr", "r0, [r5]"),
    0x2857E8: ("strb", "r7, [r0, #2]"),
    0x2857EA: ("ldr", "r0, [r5]"),
    0x2857EE: ("strb", "r1, [r0, #3]"),
    # In the mode-6 bitmap builder, channel N is converted to N-1, divided by
    # eight, and addressed backwards from object byte 69.  The low three bits
    # select the bit within that byte.  Since the DSP wire begins at object
    # byte 3 and payload excludes the type byte, ARFCN 1 is payload[65] bit 0.
    0x20FC64: ("ldr", "r0, [sp, #0x34]"),
    0x20FC66: ("subs", "r0, r0, #1"),
    0x20FC68: ("asrs", "r1, r0, #2"),
    0x20FC6E: ("asrs", "r2, r1, #3"),
    0x20FC72: ("subs", "r1, r1, r2"),
    0x20FC76: ("movs", "r2, #0x45"),
    0x20FC7E: ("movs", "r3, #7"),
    0x20FC80: ("bics", "r1, r3"),
    0x20FC82: ("subs", "r0, r0, r1"),
    0x20FC84: ("movs", "r1, #1"),
    0x20FC86: ("lsls", "r1, r0"),
    0x20FC8A: ("strb", "r1, [r2, r0]"),
    # CHANNEL_CONFIGURE is likewise constructed as a queue object, not inferred
    # from the other products.  Its wire packet is type 0x02, length 20.
    0x20D000: ("movs", "r0, #0x18"),
    0x20D010: ("movs", "r0, #0x14"),
    0x20D012: ("strb", "r0, [r4, #2]"),
    0x20D018: ("movs", "r1, #2"),
    0x20D01A: ("strb", "r1, [r4, #3]"),
    # The first body byte is a controller-selected operation.  The constructor
    # seeds 4; direct callers overwrite it with the evidenced 6/7 variants.
    0x20D01C: ("movs", "r0, #4"),
    0x20D01E: ("strb", "r0, [r4, #4]"),
    0x20D1D4: ("movs", "r0, #7"),
    0x20D1DA: ("ldr", "r0, [sp, #0xc]"),
    0x20D1DC: ("strb", "r7, [r0, #4]"),
    0x20D1E0: ("movs", "r0, #6"),
    0x20D1E6: ("strb", "r0, [r1, #4]"),
}
RADIO_REPORT_DISPATCH_ANCHORS = {
    # Type 0x80 is handled directly.  Types 0x83..0x8f use a bounded jump
    # table; the holes 0x85/0x8d/0x8e deliberately fall through.
    0x2A2108: ("movs", "r1, #0x80"),
    0x2A210A: ("subs", "r1, r0, r1"),
    0x2A210E: ("beq", "#0x2a21aa"),
    0x2A2110: ("subs", "r1, #3"),
    0x2A2112: ("cmp", "r1, #0xc"),
    0x2A2114: ("bhi", "#0x2a21bc"),
    0x2A21AA: ("adds", "r0, r4, #0"),
    0x2A21AC: ("bl", "#0x2800b0"),
    0x2A2156: ("bl", "#0x2803c8"),
    0x2A215E: ("bl", "#0x2804c0"),
    0x2A2166: ("bl", "#0x28054e"),
    0x2A216E: ("bl", "#0x2803f8"),
    0x2A2176: ("bl", "#0x2804f4"),
    0x2A217E: ("bl", "#0x280464"),
    0x2A2186: ("bl", "#0x28042e"),
    0x2A2194: ("bl", "#0x27fa34"),
    0x2A219C: ("bl", "#0x280398"),
    0x2A21A4: ("bl", "#0x27fd40"),
    # The later classifier owns the broker/service families separately from
    # the radio jump-table handlers.
    0x2A21BC: ("movs", "r1, #0x8d"),
    0x2A21C0: ("cmp", "r1, #1"),
    0x2A21C4: ("subs", "r1, #8"),
    0x2A21CA: ("subs", "r1, #6"),
    0x2A21D0: ("movs", "r1, #0xf0"),
    0x2A21D4: ("cmp", "r1, #0x70"),
}
RADIO_REPORT_HANDLER_ANCHORS = {
    # Fixed copies/posts constrain the important report object geometries.
    # Type 0x8b preserves a 0xa8-byte object (four-byte queue envelope plus
    # the 166-byte DSP packet), matching forty four-byte result records.
    0x28054E: ("push", "{r4, lr}"),
    0x280558: ("movs", "r1, #0xa8"),
    0x28055A: ("bl", "#0x276912"),
    # Type 0x88 consumes its timing/report body fields through object +0x0d.
    0x28047C: ("ldrb", "r1, [r4, #6]"),
    0x280486: ("ldrb", "r0, [r4, #7]"),
    0x28048C: ("ldrb", "r0, [r4, #9]"),
    0x280496: ("ldrb", "r1, [r4, #0xb]"),
    0x2804A0: ("ldrb", "r0, [r4, #4]"),
    0x2804A4: ("ldrb", "r0, [r4, #0xd]"),
    # The task-side ALL_RSSI_RESULTS case accepts controller state 4 only for
    # the active type-0x1a search, and also accepts states 6 and 7.
    0x21732E: ("movs", "r0, #0x6b"),
    0x21736C: ("bl", "#0x215a48"),
    0x217336: ("ldrb", "r0, [r6, #5]"),
    0x217338: ("cmp", "r0, #4"),
    0x21733C: ("ldrb", "r1, [r6]"),
    0x21733E: ("cmp", "r1, #0x1a"),
    0x217342: ("cmp", "r0, #6"),
    0x217346: ("cmp", "r0, #7"),
    0x21734C: ("bl", "#0x213fbc"),
    # NSE-3's internal candidate records are product-local 0x44-byte objects.
    # This must not be confused with the shared four-byte DSP result records
    # or NSE-8's independently recovered 0x48-byte internal stride.
    0x21403A: ("movs", "r0, #0x22"),
    0x214080: ("adds", "r4, #0x44"),
    # CHANNEL_CHANGED_CNF is gated by controller state, then copied as a
    # fixed eight-byte object and posted.  This handler does not inspect a
    # confirmation payload byte before advancing controller state to three.
    0x2804F8: ("ldr", "r0, [pc, #0x12c]"),
    0x2804FA: ("ldrb", "r1, [r0, #7]"),
    0x2804FC: ("cmp", "r1, #1"),
    0x280508: ("cmp", "r1, #7"),
    0x280532: ("ldr", "r0, [pc, #0xec]"),
    0x280538: ("movs", "r1, #8"),
    0x28053A: ("bl", "#0x276912"),
    0x280548: ("movs", "r1, #3"),
    0x28054A: ("strb", "r1, [r0]"),
    # Task-side statuses 0x1393/0x1394 keep channel-change confirmation and
    # RA_INFO as separate controller events.
    0x211C68: ("ldr", "r1, [pc, #0x27c]"),
    0x211CAE: ("ldr", "r1, [pc, #0x23c]"),
    0x211DA4: ("ldr", "r0, [sp, #0x18]"),
    0x211DA6: ("movs", "r1, #8"),
    0x211DA8: ("bl", "#0x27693c"),
    0x211D70: ("ldr", "r0, [sp, #0x18]"),
    0x211D72: ("movs", "r1, #8"),
    0x211D74: ("bl", "#0x27693c"),
}
RADIO_REPORT_HANDLER_LITERALS = {
    0x211C68: 0x1393,
    0x211CAE: 0x1394,
}
RADIO_REPORT_JUMP_TABLE_ADDRESS = 0x2A2120
RADIO_REPORT_JUMP_TABLE = {
    0x83: 0x2A21A2,
    0x84: 0x2A219A,
    0x85: 0x2A21BC,
    0x86: 0x2A2192,
    0x87: 0x2A2184,
    0x88: 0x2A217C,
    0x89: 0x2A2174,
    0x8A: 0x2A216C,
    0x8B: 0x2A2164,
    0x8C: 0x2A215C,
    0x8D: 0x2A21BC,
    0x8E: 0x2A21BC,
    0x8F: 0x2A2154,
}
DSP_PARAMETER_08_ANCHORS = {
    # Generic DSP parameter writer: selector r0 is bounded to 0x00..0x2e and
    # dispatched through the adjacent table.
    0x285B7C: ("push", "{r4, r5, r6, r7, lr}"),
    0x285B86: ("adds", "r4, r0, #0"),
    0x285BA2: ("cmp", "r0, #0x2e"),
    0x285BA6: ("adr", "r1, #8"),
    0x285BA8: ("lsls", "r0, r0, #2"),
    0x285BAA: ("ldr", "r0, [r1, r0]"),
    0x285BAC: ("mov", "pc, r0"),
    # Selector 0x08 preserves the low twelve input bits, sets bit 15, mirrors
    # the encoded word in SRAM, then reaches the shared-cell publication.
    0x285DE8: ("ldr", "r0, [pc, #0x280]"),
    0x285DEA: ("lsls", "r1, r6, #0x14"),
    0x285DEC: ("lsrs", "r1, r1, #0x14"),
    0x285DEE: ("orrs", "r0, r1"),
    0x285DF0: ("ldr", "r1, [pc, #0x27c]"),
    0x285DE4: ("strh", "r0, [r1]"),
    0x285D22: ("ldr", "r2, [pc, #0x108]"),
    0x285E30: ("ldrh", "r1, [r2]"),
    0x285E3C: ("strh", "r0, [r2]"),
    # One service-controlled mode routine submits table-derived selector 8/9
    # pairs. This is a caller boundary, not an organic call-state claim.
    0x2391E8: ("ldr", "r1, [pc, #0x378]"),
    0x2391F0: ("ldrh", "r1, [r4]"),
    0x2391F2: ("movs", "r0, #8"),
    0x2391F4: ("movs", "r2, #1"),
    0x2391F6: ("bl", "#0x285b7c"),
    # The normal parameter-state updater compares shadow/live words and
    # submits changed slots through a selector table whose first entry is 8.
    0x283D5E: ("ldrh", "r1, [r5]"),
    0x283D60: ("mov", "r0, r8"),
    0x283D62: ("ldrh", "r0, [r0]"),
    0x283D64: ("cmp", "r0, r1"),
    0x283D68: ("ldr", "r0, [pc, #0x2b8]"),
    0x283D6A: ("ldrb", "r0, [r0]"),
    0x283D6C: ("movs", "r2, #1"),
    0x283D6E: ("bl", "#0x285b7c"),
}
DSP_PARAMETER_08_LITERALS = {
    0x285DE8: 0xFFFF8000,
    0x285DF0: 0x10B972,
    0x285D22: 0x100A8,
    0x2391E8: 0x2B986C,
    0x283D68: 0x2B85EC,
}
DSP_PARAMETER_JUMP_TABLE_ADDRESS = 0x285BB0
DSP_PARAMETER_08_MODE_TABLE_ADDRESS = 0x2B986C
DSP_PARAMETER_08_MODE_VALUES = [0x0600] * 9
DSP_PARAMETER_SELECTOR_TABLE_ADDRESS = 0x2B85EC
DSP_PARAMETER_SELECTOR_TABLE = bytes(
    [0x08, 0x09, 0x1B, 0x25, 0x20, 0x21, 0x22, 0x23, 0x24, 0x28, 0x2D]
)
DSP_BOOTSTRAP_ANCHORS = {
    # Shared bootstrap/header setup.
    0x2858FC: ("push", "{r4, r5, r6, r7, lr}"),
    0x285904: ("ldr", "r5, [pc, #0x118]"),
    0x285906: ("strh", "r0, [r5]"),
    0x285908: ("strh", "r0, [r5, #2]"),
    0x28592A: ("ldr", "r1, [pc, #0x22c]"),
    # 512-word staging loop: sequential shared destination, flash stride 0x20.
    0x285970: ("ldr", "r2, [pc, #0x1ec]"),
    0x285972: ("ldr", "r7, [pc, #0x1f0]"),
    0x285976: ("ldr", "r5, [pc, #0x1f0]"),
    0x28597A: ("movs", "r0, #1"),
    0x28597C: ("lsls", "r0, r0, #9"),
    0x28597E: ("ldrh", "r6, [r5]"),
    0x285980: ("strh", "r6, [r1]"),
    0x285982: ("adds", "r1, #2"),
    0x285984: ("adds", "r5, #0x20"),
    0x28598A: ("bne", "#0x28597e"),
    # Alternating zero request and non-zero wait across the two cells.
    0x28598C: ("lsrs", "r0, r3, #1"),
    0x285990: ("strh", "r4, [r2]"),
    0x285992: ("ldrh", "r0, [r2, #2]"),
    0x28599A: ("strh", "r4, [r2, #2]"),
    0x28599E: ("ldrh", "r0, [r2]"),
    0x2859A4: ("adds", "r3, #1"),
    0x2859A6: ("cmp", "r3, #0x3f"),
    0x2859A8: ("blo", "#0x28597a"),
    # Final 510 sampled words and two explicit 0xffff terminators.
    0x2859AA: ("movs", "r0, #0xff"),
    0x2859AC: ("adds", "r0, #0xff"),
    0x2859AE: ("ldrh", "r6, [r5]"),
    0x2859B0: ("strh", "r6, [r1]"),
    0x2859B2: ("adds", "r1, #2"),
    0x2859B4: ("adds", "r5, #0x20"),
    0x2859BA: ("bne", "#0x2859ae"),
    0x2859BC: ("ldr", "r0, [pc, #0x1ac]"),
    0x2859BE: ("strh", "r0, [r1]"),
    0x2859C0: ("adds", "r1, #2"),
    0x2859C2: ("strh", "r0, [r1]"),
    # Final alternating exchange followed by an external publication wait.
    0x2859C8: ("lsrs", "r0, r3, #1"),
    0x2859CC: ("strh", "r4, [r2]"),
    0x2859CE: ("ldrh", "r0, [r2, #2]"),
    0x2859D6: ("strh", "r4, [r2, #2]"),
    0x2859D8: ("ldrh", "r0, [r2]"),
    0x2859DE: ("ldrh", "r0, [r1, #2]"),
    0x2859E0: ("cmp", "r0, #0"),
    0x2859E2: ("beq", "#0x2859de"),
    0x2859FE: ("pop", "{r4, r5, r6, r7, pc}"),
}
DSP_BOOTSTRAP_LITERALS = {
    0x285904: 0x10000,
    0x28592A: 0x100F6,
    0x285970: 0x100FE,
    0x285972: 0x10200,
    0x285976: 0x200040,
    0x2859BC: 0xFFFF,
}
DSP_BOOTSTRAP_STREAM_SHA1 = "f708ffd71e430f41c47f12e18128cf4deffb5845"
DSP_BOOTSTRAP_RESULT_ANCHORS = {
    # Capture the two final shared publications in the DSPIF state object.
    0x2859E4: ("ldr", "r0, [pc, #0xa8]"),
    0x2859E6: ("ldrh", "r2, [r1, #2]"),
    0x2859E8: ("strh", "r2, [r0, #0xc]"),
    0x2859EA: ("ldrh", "r1, [r1]"),
    0x2859EC: ("strh", "r1, [r0, #0xa]"),
    # The normal initialization sequence has one direct transfer call.
    0x2973F0: ("bl", "#0x2858fc"),
    # One diagnostic formatter emits the captured 0x10000 word as Bxx.
    0x28E976: ("ldr", "r1, [pc, #0x35c]"),
    0x28E978: ("ldrh", "r0, [r1]"),
    0x28E97A: ("lsrs", "r0, r0, #8"),
    0x28E97C: ("lsls", "r0, r0, #0x1c"),
    0x28E97E: ("lsrs", "r0, r0, #0x1c"),
    0x28E980: ("adds", "r0, #0x37"),
    0x28E984: ("ldrh", "r0, [r1]"),
    0x28E986: ("lsrs", "r0, r0, #4"),
    0x28E990: ("ldrh", "r0, [r1]"),
    0x28E992: ("lsls", "r0, r0, #0x1c"),
    # A later path requires the same captured word to equal 0x0b06.
    0x298E82: ("ldr", "r0, [pc, #0x48]"),
    0x298E84: ("ldrh", "r1, [r0]"),
    0x298E86: ("ldr", "r0, [pc, #0x48]"),
    0x298E88: ("cmp", "r1, r0"),
    0x298E8A: ("beq", "#0x298e8e"),
    0x298E8C: ("b", "#0x298b36"),
}
DSP_BOOTSTRAP_RESULT_LITERALS = {
    0x2859E4: 0x10B970,
    0x28E976: 0x10B97A,
    0x298E82: 0x10B97A,
    0x298E86: 0x0B06,
}
COBBA_ID_SERVICE_ANCHORS = {
    # The service handler passes request byte 9 through as formatter selector.
    0x2382E4: ("ldrb", "r0, [r4, #9]"),
    0x2382E6: ("adds", "r1, r5, #0"),
    0x2382E8: ("movs", "r2, #0x32"),
    0x2382EA: ("bl", "#0x28e8f6"),
    0x238312: ("ldrb", "r1, [r4, #9]"),
    0x238314: ("strb", "r1, [r0, #9]"),
    # Formatter dispatch reaches the captured-word branch at selector 0x0d.
    0x28E93C: ("movs", "r0, #0xb"),
    0x28E93E: ("subs", "r0, r7, r0"),
    0x28E944: ("subs", "r0, #1"),
    0x28E948: ("beq", "#0x28e9a0"),
    0x28E94A: ("subs", "r0, #1"),
    0x28E94E: ("beq", "#0x28e976"),
    # Adjacent selector 0x0c reads MAD2's ASIC-version byte.
    0x28E9A0: ("ldr", "r0, [pc, #0x334]"),
    0x28E9A2: ("ldrb", "r2, [r0]"),
    # Selectors 0x09 and 0x03 use separate RAM and flash-indirect sources.
    0x28E914: ("cmp", "r7, #9"),
    0x28E918: ("beq", "#0x28ea16"),
    0x28E928: ("subs", "r0, #1"),
    0x28E92A: ("cmp", "r0, #0"),
    0x28E92C: ("beq", "#0x28ea1e"),
    0x28EA16: ("ldr", "r5, [pc, #0x2c8]"),
    0x28EA1E: ("ldr", "r0, [pc, #0x2c8]"),
    0x28EA72: ("ldr", "r5, [r0]"),
}
COBBA_ID_SERVICE_LITERALS = {
    0x28E9A0: 0x20000,
    0x28EA16: 0x10BCF0,
    0x28EA1E: 0x2AB52C,
}
EXPECTED_CENSUS = {
    "literal_seeds": 225,
    "resolved_accesses": 548,
    "offsets": [
        0x00, 0x01, 0x02, 0x03, 0x04, 0x06, 0x08, 0x09,
        0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11,
        0x12, 0x15, 0x16, 0x18, 0x19, 0x1A, 0x1B, 0x1C,
        0x1E, 0x20, 0x22, 0x24, 0x28, 0x29, 0x33, 0x36,
        0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E,
        0x3F,
    ],
}

ENTRY_ANCHORS = {
    0x200040: ("mov", "r1, #0x200000"),
    0x200044: ("ldr", "r0, [r1]"),
    0x200058: ("mov", "r1, #0x40000"),
    0x20005C: ("str", "r0, [r1]"),
    0x2000B8: ("ldr", "r0, [pc, #0xa8]"),
    0x2000BC: ("mov", "r1, #0"),
    0x2000C0: ("ldm", "r0, {r2, r3, r4, r5, r6, r7, r8, sb}"),
    0x2000C4: ("stm", "r1, {r2, r3, r4, r5, r6, r7, r8, sb}"),
    0x2000E4: ("add", "r0, pc, #1"),
    0x2000E8: ("bx", "r0"),
}


def swap16(data: bytes) -> bytes:
    if len(data) % 2:
        raise ValueError("swap16 input must contain an even number of bytes")
    result = bytearray(data)
    result[0::2], result[1::2] = data[1::2], data[0::2]
    return bytes(result)


def decode_thumb_anchors(data: bytes, anchors: dict[int, tuple[str, str]]) -> None:
    decoder = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
    physical = swap16(data)
    for pc, expected in anchors.items():
        offset = pc - FLASH_BASE
        decoded = list(decoder.disasm(physical[offset : offset + 4], pc, count=1))
        actual = (decoded[0].mnemonic, decoded[0].op_str) if decoded else None
        if actual != expected:
            raise ValueError(f"Thumb anchor {pc:#x}: expected {expected}, got {actual}")


def verify_thumb_literals(data: bytes, literals: dict[int, int]) -> None:
    physical = swap16(data)
    decoder = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
    decoder.detail = True
    for pc, expected in literals.items():
        offset = pc - FLASH_BASE
        decoded = list(decoder.disasm(physical[offset : offset + 4], pc, count=1))
        actual = literal_value(decoded[0] if decoded else None, physical, FLASH_BASE)
        if actual != expected:
            raise ValueError(
                f"Thumb literal {pc:#x}: expected {expected:#x}, got "
                f"{actual if actual is None else hex(actual)}"
            )


def verify_identity(data: bytes) -> dict:
    digest = hashlib.sha1(data).hexdigest()
    if len(data) != FLASH_SIZE:
        raise ValueError(f"expected {FLASH_SIZE:#x}-byte flash, got {len(data):#x}")
    if digest != EXPECTED_SHA1:
        raise ValueError(f"expected NSE-3 v4.06 SHA-1 {EXPECTED_SHA1}, got {digest}")
    return {"size": len(data), "sha1": digest}


def verify_reset_boundary(data: bytes) -> dict:
    if len(data) < 0x180:
        raise ValueError("image is too short for the NSE-3 reset boundary")
    decoder = capstone.Cs(
        capstone.CS_ARCH_ARM, capstone.CS_MODE_ARM | capstone.CS_MODE_BIG_ENDIAN
    )
    decoded = {
        insn.address: (insn.mnemonic, insn.op_str)
        for insn in decoder.disasm(data[0x40:0xEC], FLASH_BASE + 0x40)
    }
    for pc, expected in ENTRY_ANCHORS.items():
        if decoded.get(pc) != expected:
            raise ValueError(
                f"reset anchor {pc:#x}: expected {expected}, got {decoded.get(pc)}"
            )

    literals = {
        "vector_source": struct.unpack_from(">I", data, 0x168)[0],
        "irq_fiq_stack": struct.unpack_from(">I", data, 0x16C)[0],
        "supervisor_stack": struct.unpack_from(">I", data, 0x170)[0],
        "abort_stack": struct.unpack_from(">I", data, 0x174)[0],
        "system_stack": struct.unpack_from(">I", data, 0x178)[0],
    }
    if literals["vector_source"] != VECTOR_SOURCE:
        raise ValueError(
            f"expected vector source {VECTOR_SOURCE:#x}, "
            f"got {literals['vector_source']:#x}"
        )
    for name, address in literals.items():
        if name != "vector_source" and not SRAM_BASE <= address < SRAM_BASE + SRAM_SIZE:
            raise ValueError(f"{name} {address:#x} lies outside 64 KiB NSE-3 SRAM")
    return {
        "entry": FLASH_BASE + 0x40,
        "vector_destination": 0,
        "thumb_transition": FLASH_BASE + 0xE4,
        "literals": literals,
    }


def verify_mad2_census(data: bytes) -> dict:
    accesses, coverage = analyze_image(swap16(data))
    offsets = sorted({access["offset"] for access in accesses})
    actual = {
        "literal_seeds": coverage["literal_seeds"],
        "resolved_accesses": coverage["resolved_accesses"],
        "offsets": offsets,
    }
    if actual != EXPECTED_CENSUS:
        raise ValueError(f"MAD2 census changed: expected {EXPECTED_CENSUS}, got {actual}")
    actual["maximum_offset"] = offsets[-1]
    return actual


def verify_keypad_boundary(data: bytes) -> dict:
    offset = KEYMAP_ADDRESS - FLASH_BASE
    special_offset = SPECIAL_KEYMAP_ADDRESS - FLASH_BASE
    if data[offset : offset + len(KEYMAP)] != KEYMAP:
        raise ValueError(f"normal 5x5 keypad table changed at {KEYMAP_ADDRESS:#x}")
    if data[special_offset : special_offset + len(SPECIAL_KEYMAP)] != SPECIAL_KEYMAP:
        raise ValueError(f"special keypad table changed at {SPECIAL_KEYMAP_ADDRESS:#x}")
    return {
        "normal_table": KEYMAP_ADDRESS,
        "indexing": "drive_row_times_5_plus_sense_column",
        "volume_down": {"drive_row": 1, "sense_column": 0, "keycode": 0x11},
        "volume_up": {"drive_row": 4, "sense_column": 0, "keycode": 0x10},
        "special_table": SPECIAL_KEYMAP_ADDRESS,
        "power_keycode": 0x0D,
    }


def verify_eeprom_boundary(data: bytes) -> dict:
    decode_thumb_anchors(data, EEPROM_ANCHORS)
    return {
        "transaction": 0x29CD88,
        "byte_sender": 0x29E8BC,
        "genio_signal": 0x20020,
        "genio_direction": 0x20024,
        "sda_bit": 0,
        "scl_bit": 2,
        "word_address_bytes": 2,
        "device_geometry_source": "NSE-3 parts list: 8 KiB serial EEPROM",
    }


def verify_simi_boundary(data: bytes) -> dict:
    decode_thumb_anchors(data, SIMI_ANCHORS)
    decode_thumb_anchors(data, SIM_ATR_PPS_ANCHORS)
    decode_thumb_anchors(data, SIM_T0_STATUS_ANCHORS)
    return {
        "driver_extent": {"start": 0x28FF84, "end": 0x2905F4},
        "clock_gate": {"register": 0x2000D, "mask": 0x20},
        "register_window": {"start": 0x20036, "end": 0x2003F},
        "control": 0x20039,
        "activation_mask": 0x80,
        "tx_data": 0x20036,
        "rx_data": 0x20037,
        "interrupt_identification": 0x20038,
        "rx_count": 0x2003C,
        "rx_fifo_control": 0x2003D,
        "tx_fifo_control": 0x2003E,
        "tx_count": 0x2003F,
        "atr_contract": {
            "conventions": [0x3B, 0x3F],
            "interface_bytes": "parsed_from_T0_TDn_presence_bits",
        },
        "pps_contract": {
            "ordinary_ta1": [0xFF, 0x00, 0xFF],
            "ta1_94": [0xFF, 0x10, 0x94, 0x7B],
        },
        "lab_card_atr": [0x3B, 0x10, 0x05],
        "lab_card_atr_pps_compatible": True,
        "t0_status_families": [0x67, 0x6F, 0x90, 0x91, 0x92, 0x93, 0x94, 0x98, 0x9F],
        "lab_card_explicit_status_families": [0x67, 0x90, 0x94, 0x98, 0x9F],
        "unsupported_instruction_status": {
            "status": [0x6D, 0x00],
            "firmware_path": "generic_command_error",
        },
        "subscriber_filesystem_profile": "removable_lab_fixture_not_nse3_identity",
        "initial_apdu_sequence": "requires_organic_boot_trace",
    }


def verify_dspif_boundary(data: bytes) -> dict:
    decode_thumb_anchors(data, DSPIF_ANCHORS)
    verify_thumb_literals(data, DSPIF_LITERALS)
    return {
        "shared_window": {"start": 0x10000, "end": 0x10FFF},
        "doorbell": 0x30000,
        "mcu_to_dsp": {
            "ring_start_byte": 0x000,
            "ring_end_byte_exclusive": 0x0A4,
            "producer_byte": 0x0A4,
            "consumer_byte": 0x0A6,
            "cursor_unit": "word",
        },
        "dsp_to_mcu": {
            "ring_start_byte": 0x100,
            "ring_end_byte_exclusive": 0x1C8,
            "producer_byte": 0x1C8,
            "consumer_byte": 0x1CA,
            "cursor_unit": "word",
        },
        "cursor_initialization": {
            "mcu_to_dsp": 0,
            "dsp_to_mcu": 0x80,
        },
        "transport_component": "generic_nokia_dspif",
        "internal_dsp_image": "missing",
        "bootstrap_reply_semantics": "not_established",
    }


def verify_radio_packet_boundary(data: bytes) -> dict:
    decode_thumb_anchors(data, RADIO_PACKET_ANCHORS)
    decode_thumb_anchors(data, RADIO_REPORT_DISPATCH_ANCHORS)
    decode_thumb_anchors(data, RADIO_REPORT_HANDLER_ANCHORS)
    verify_thumb_literals(data, RADIO_REPORT_HANDLER_LITERALS)
    table_offset = RADIO_REPORT_JUMP_TABLE_ADDRESS - FLASH_BASE
    table = {
        packet_type: struct.unpack_from(
            ">I", data, table_offset + (packet_type - 0x83) * 4
        )[0]
        for packet_type in RADIO_REPORT_JUMP_TABLE
    }
    if table != RADIO_REPORT_JUMP_TABLE:
        raise ValueError(
            f"NSE-3 radio report jump table changed: expected "
            f"{RADIO_REPORT_JUMP_TABLE}, got {table}"
        )
    return {
        "transport": {
            "tx_queue_pump": 0x298C82,
            "tx_writer": 0x285746,
            "object_length_offset": 2,
            "wire_start_offset": 3,
            "rx_parser": 0x285794,
            "rx_type_offset": 3,
        },
        "search_list": {
            "type": 0x1A,
            "wire_length": 68,
            "object_allocation": 0x48,
            "constructors": [0x20FAEC, 0x216F72],
            "representation": "bitmap_shaped",
            "arfcn_bit_numbering": {
                "arfcn_1": {"payload_byte": 65, "bit": 0},
                "formula": "payload[65-floor((arfcn-1)/8)] bit ((arfcn-1)%8)",
            },
            "nse8_search_bitmap_boundary_compatible": True,
        },
        "channel_configure": {
            "type": 0x02,
            "wire_length": 20,
            "object_allocation": 0x18,
            "constructor": 0x20CFFA,
            "operation_byte": 0,
            "evidenced_operation_values": [4, 6, 7],
        },
        "report_dispatch": {
            "routine": 0x2A20DC,
            "jump_table": RADIO_REPORT_JUMP_TABLE_ADDRESS,
            "radio_handlers": {
                "0x80": 0x2800B0,
                "0x83": 0x27FD40,
                "0x84": 0x280398,
                "0x86": 0x27FA34,
                "0x87": 0x28042E,
                "0x88": 0x280464,
                "0x89": 0x2804F4,
                "0x8a": 0x2803F8,
                "0x8b": 0x28054E,
                "0x8c": 0x2804C0,
                "0x8f": 0x2803C8,
            },
            "separate_service_families": ["0x8d", "0x8e", "0x95", "0x9b", "0x70..0x7f"],
            "all_rssi_results": {
                "type": 0x8B,
                "wire_length": 166,
                "queue_object_bytes": 0xA8,
                "record_count": 40,
                "record_bytes": 4,
            },
            "search_lifecycle": {
                "all_rssi_task_case": 0x21732E,
                "active_search_type": 0x1A,
                "accepted_controller_states": [4, 6, 7],
                "measurement_consumer": 0x213FBC,
                "nse3_internal_candidate_stride": 0x44,
                "nse8_internal_candidate_stride": 0x48,
                "internal_candidate_layout_shared": False,
            },
            "channel_change": {
                "confirmation_type": 0x89,
                "confirmation_handler": 0x2804F4,
                "task_status": 0x1393,
                "ra_info_type": 0x84,
                "ra_info_task_status": 0x1394,
                "task_object_bytes": 8,
                "confirmation_payload_read_by_handler": False,
                "controller_state_after_confirmation": 3,
            },
        },
        "peer_profile": "disabled_pending_bootstrap_and_full_report_lifecycle",
    }


def verify_dsp_parameter_08_boundary(data: bytes) -> dict:
    decode_thumb_anchors(data, DSP_PARAMETER_08_ANCHORS)
    verify_thumb_literals(data, DSP_PARAMETER_08_LITERALS)
    table_offset = DSP_PARAMETER_JUMP_TABLE_ADDRESS - FLASH_BASE
    selector_08_target = struct.unpack_from(
        ">I", data, table_offset + 8 * 4
    )[0]
    if selector_08_target != 0x285DE8:
        raise ValueError(
            "NSE-3 DSP parameter selector 8 target changed: expected "
            f"0x285de8, got {selector_08_target:#x}"
        )
    mode_offset = DSP_PARAMETER_08_MODE_TABLE_ADDRESS - FLASH_BASE
    mode_values = [
        struct.unpack_from(">H", data, mode_offset + index * 4)[0]
        for index in range(len(DSP_PARAMETER_08_MODE_VALUES))
    ]
    if mode_values != DSP_PARAMETER_08_MODE_VALUES:
        raise ValueError(
            f"NSE-3 DSP parameter 8 mode table changed: expected "
            f"{DSP_PARAMETER_08_MODE_VALUES}, got {mode_values}"
        )
    selector_offset = DSP_PARAMETER_SELECTOR_TABLE_ADDRESS - FLASH_BASE
    selectors = data[
        selector_offset : selector_offset + len(DSP_PARAMETER_SELECTOR_TABLE)
    ]
    if selectors != DSP_PARAMETER_SELECTOR_TABLE:
        raise ValueError(
            "NSE-3 DSP parameter state selector table changed: expected "
            f"{DSP_PARAMETER_SELECTOR_TABLE.hex()}, got {selectors.hex()}"
        )
    return {
        "writer": 0x285B7C,
        "selector": 0x08,
        "selector_target": selector_08_target,
        "input_encoding": "0x8000 | (value & 0x0fff)",
        "shared_publication": 0x100A8,
        "sram_mirror": 0x10B972,
        "service_mode_caller": 0x2391BC,
        "service_mode_values": mode_values,
        "state_delta_updater": 0x283D5E,
        "state_selector_table": list(selectors),
        "selector_08_state_slot": 0,
        "organic_answer_value": "not_established",
        "organic_end_value": "not_established",
        "speech_role": "not_established_from_selector_number",
    }


def extract_dsp_bootstrap_stream(data: bytes) -> bytes:
    source_first = 0x200040
    source_words = 63 * 512 + 510
    stream = bytearray()
    for index in range(source_words):
        offset = source_first - FLASH_BASE + index * 0x20
        stream.extend(data[offset : offset + 2])
    stream.extend(b"\xff\xff\xff\xff")
    return bytes(stream)


def verify_dsp_bootstrap_boundary(data: bytes) -> dict:
    decode_thumb_anchors(data, DSP_BOOTSTRAP_ANCHORS)
    verify_thumb_literals(data, DSP_BOOTSTRAP_LITERALS)
    decode_thumb_anchors(data, DSP_BOOTSTRAP_RESULT_ANCHORS)
    verify_thumb_literals(data, DSP_BOOTSTRAP_RESULT_LITERALS)
    decode_thumb_anchors(data, COBBA_ID_SERVICE_ANCHORS)
    verify_thumb_literals(data, COBBA_ID_SERVICE_LITERALS)
    physical = swap16(data)
    instructions = decode_image(physical, FLASH_BASE)
    direct_callers = [
        insn.address
        for insn in instructions
        if insn
        and insn.mnemonic in ("bl", "blx")
        and immediate_target(insn) == 0x2858FC
    ]
    if direct_callers != [0x2973F0]:
        raise ValueError(
            "NSE-3 DSP bootstrap direct callers changed: expected "
            f"[0x2973f0], got {[hex(address) for address in direct_callers]}"
        )
    result_literal_references = {
        address: [
            insn.address
            for insn in instructions
            if insn and literal_value(insn, physical, FLASH_BASE) == address
        ]
        for address in (0x10B97A, 0x10B97C)
    }
    expected_result_references = {
        0x10B97A: [0x28E976, 0x298E82],
        0x10B97C: [],
    }
    if result_literal_references != expected_result_references:
        raise ValueError(
            "NSE-3 DSP result literal references changed: expected "
            f"{expected_result_references}, got {result_literal_references}"
        )
    stream = extract_dsp_bootstrap_stream(data)
    stream_sha1 = hashlib.sha1(stream).hexdigest()
    if stream_sha1 != DSP_BOOTSTRAP_STREAM_SHA1:
        raise ValueError(
            "NSE-3 DSP bootstrap stream changed: expected "
            f"{DSP_BOOTSTRAP_STREAM_SHA1}, got {stream_sha1}"
        )
    return {
        "routine": {"start": 0x2858FC, "end": 0x285A00},
        "sampled_flash": {
            "first_halfword": 0x200040,
            "last_halfword": 0x2FFFE0,
            "stride_bytes": 0x20,
            "source_words": 32766,
        },
        "staging": {
            "shared_start": 0x10200,
            "shared_end_exclusive": 0x10600,
            "words_per_block": 512,
            "full_source_blocks": 63,
            "tail_source_words": 510,
            "tail_words": [0xFFFF, 0xFFFF],
            "transfer_blocks": 64,
            "total_staged_words": 32768,
            "stream_sha1": stream_sha1,
        },
        "synchronization": {
            "cells": [0x100FE, 0x10100],
            "selection": "block_index_bit_0",
            "request_value": 0,
            "wait_condition": "opposite_cell_nonzero",
            "post_transfer_publication_wait": 0x10002,
            "reply_meaning": "not_established",
        },
        "result_capture": {
            "direct_callers": direct_callers,
            "shared_0x10000": {
                "storage": 0x10B97A,
                "direct_literal_references": result_literal_references[0x10B97A],
                "exact_comparison": 0x0B06,
                "comparison_pc": 0x298E88,
                "service_render": "B06",
                "render_pc": 0x28E976,
            },
            "shared_0x10002": {
                "storage": 0x10B97C,
                "direct_literal_references": result_literal_references[0x10B97C],
                "value_constraint": "not_established",
            },
            "current_hle_ready_0x0001_satisfies_comparison": False,
            "service_projection": {
                "selector": 0x0D,
                "role": "COBBA identification",
                "corroboration": "Nokia 6110 service protocol: 0xc8/0x0d Get COBBA",
                "dsp_side_meaning": "not_established",
            },
            "service_identity_sources": {
                "0x03_dsp_external_software": {
                    "source": "flash_indirect",
                    "pointer_table": 0x2AB52C,
                },
                "0x09_dsp_internal_software": {
                    "source": "runtime_ram",
                    "address": 0x10BCF0,
                },
                "0x0c_system_asic": {
                    "source": "mad2_register",
                    "address": 0x20000,
                },
                "0x0d_cobba": {
                    "source": "bootstrap_captured_shared_word",
                    "shared_address": 0x10000,
                    "captured_address": 0x10B97A,
                    "render": "B06",
                },
                "namespaces_are_interchangeable": False,
            },
        },
        "claims": {
            "stream_is_dsp_code": "not_established",
            "dsp_destination": "not_established",
            "response_values": "nonzero_only",
            "internal_dsp_image": "missing",
        },
    }


def verify(data: bytes) -> dict:
    return {
        "identity": verify_identity(data),
        "reset_boundary": verify_reset_boundary(data),
        "keypad_boundary": verify_keypad_boundary(data),
        "eeprom_boundary": verify_eeprom_boundary(data),
        "simi_boundary": verify_simi_boundary(data),
        "dspif_transport_boundary": verify_dspif_boundary(data),
        "radio_packet_boundary": verify_radio_packet_boundary(data),
        "dsp_parameter_08_boundary": verify_dsp_parameter_08_boundary(data),
        "dsp_bootstrap_transfer_boundary": verify_dsp_bootstrap_boundary(data),
        "mad2_direct_access_census": verify_mad2_census(data),
        "claims": {
            "rom3_compatibility": "candidate_not_proven",
            "register_semantics": "not_assigned",
            "boot_promotion": False,
        },
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("flash", type=Path)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()
    result = verify(args.flash.read_bytes())
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(result, indent=2) + "\n")
    print(
        "verified NSE-3 v4.06 static boundary: "
        f"entry={result['reset_boundary']['entry']:#x}, "
        f"MAD2-sites={result['mad2_direct_access_census']['resolved_accesses']}, "
        f"DSP-transfer-blocks="
        f"{result['dsp_bootstrap_transfer_boundary']['staging']['transfer_blocks']}, "
        "boot-promotion=no"
    )


if __name__ == "__main__":
    main()

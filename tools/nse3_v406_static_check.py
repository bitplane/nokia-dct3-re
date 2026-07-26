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
    from tools.message_census import (
        cpu_byte,
        decode_image,
        effective_u32,
        extract_calls,
        immediate_target,
        literal_value,
    )
except ModuleNotFoundError:  # Direct execution from tools/.
    from mad2_static_census import analyze_image
    from message_census import (
        cpu_byte,
        decode_image,
        effective_u32,
        extract_calls,
        immediate_target,
        literal_value,
    )


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
SIM_APDU_CONSTRUCTOR_ANCHORS = {
    # Contiguous GSM SIM command constructors all author CLA A0 and an
    # instruction byte before submitting the command object through 0x286b6a.
    0x286C5C: ("movs", "r1, #0xa0"),
    0x286C60: ("movs", "r1, #0x20"),
    0x286CDC: ("movs", "r0, #0xa0"),
    0x286CE0: ("movs", "r0, #0xdc"),
    0x286D34: ("movs", "r0, #0xa0"),
    0x286D38: ("movs", "r0, #0xd6"),
    0x286D88: ("movs", "r0, #0xa0"),
    0x286D8C: ("movs", "r0, #0x2c"),
    0x286E06: ("movs", "r2, #0xa0"),
    0x286E0A: ("movs", "r2, #0xf2"),
    0x286E6A: ("movs", "r1, #0xa0"),
    0x286E6E: ("movs", "r1, #0x14"),
    0x286EB8: ("movs", "r2, #0xa0"),
    0x286EBC: ("movs", "r2, #0x10"),
    0x286F08: ("movs", "r2, #0xa0"),
    0x286F0C: ("movs", "r2, #0x12"),
    0x286F48: ("movs", "r2, #0xa0"),
    0x286F4C: ("movs", "r2, #0xa4"),
    0x286FA8: ("movs", "r2, #0xa0"),
    0x286FAC: ("movs", "r2, #0xa4"),
    0x286FFE: ("movs", "r1, #0xa0"),
    0x287002: ("movs", "r1, #0x88"),
    0x2870C0: ("movs", "r1, #0xa0"),
    0x2870C4: ("movs", "r1, #0xb2"),
    0x2870F6: ("movs", "r1, #0xa0"),
    0x2870FA: ("movs", "r1, #0xb0"),
    0x287148: ("movs", "r3, #0xa0"),
    0x28714C: ("movs", "r3, #0x32"),
    0x287194: ("movs", "r2, #0xa0"),
    0x287198: ("movs", "r2, #0xc0"),
    0x2871D6: ("movs", "r1, #0xa0"),
    0x2871DA: ("movs", "r1, #0x28"),
    0x28723E: ("movs", "r1, #0xa0"),
    0x287242: ("movs", "r1, #0x26"),
    0x287398: ("movs", "r0, #0xa0"),
    0x28739C: ("movs", "r0, #0xc2"),
    0x28753E: ("movs", "r1, #0xa0"),
    0x287542: ("movs", "r1, #0x24"),
    0x286C9E: ("bl", "#0x286b6a"),
    0x287592: ("bl", "#0x286b6a"),
}
SIM_APDU_CONSTRUCTORS = [
    {"address": 0x286C40, "instruction": 0x20, "name": "VERIFY_CHV"},
    {"address": 0x286CB6, "instruction": 0xDC, "name": "UPDATE_RECORD"},
    {"address": 0x286D14, "instruction": 0xD6, "name": "UPDATE_BINARY"},
    {"address": 0x286D6A, "instruction": 0x2C, "name": "UNBLOCK_CHV"},
    {"address": 0x286DF0, "instruction": 0xF2, "name": "STATUS"},
    {"address": 0x286E4C, "instruction": 0x14, "name": "TERMINAL_PROFILE"},
    {"address": 0x286E9E, "instruction": 0x10, "name": "TERMINAL_RESPONSE"},
    {"address": 0x286EF0, "instruction": 0x12, "name": "FETCH"},
    {"address": 0x286F30, "instruction": 0xA4, "name": "SELECT"},
    {"address": 0x286F90, "instruction": 0xA4, "name": "SELECT"},
    {"address": 0x286FE4, "instruction": 0x88, "name": "RUN_GSM_ALGORITHM"},
    {"address": 0x2870A4, "instruction": 0xB2, "name": "READ_RECORD"},
    {"address": 0x2870DC, "instruction": 0xB0, "name": "READ_BINARY"},
    {"address": 0x287130, "instruction": 0x32, "name": "INCREASE"},
    {"address": 0x28717E, "instruction": 0xC0, "name": "GET_RESPONSE"},
    {"address": 0x2871BC, "instruction": 0x28, "name": "ENABLE_CHV"},
    {"address": 0x287224, "instruction": 0x26, "name": "DISABLE_CHV"},
    {"address": 0x287310, "instruction": 0xC2, "name": "ENVELOPE"},
    {"address": 0x287520, "instruction": 0x24, "name": "CHANGE_CHV"},
]
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
EXTERNAL_SERVICE_TRANSPORT_ANCHORS = {
    # DSP reports 0x8d/0x8e remain outside the radio table and are posted to
    # task 9 through the ordinary queue API.
    0x2A21FA: ("adds", "r0, r4, #0"),
    0x2A21FC: ("bl", "#0x273e3e"),
    0x273E3E: ("push", "{lr}"),
    0x273E40: ("adds", "r1, r0, #0"),
    0x273E42: ("movs", "r0, #9"),
    0x273E44: ("bl", "#0x25fc98"),
    # Task 9 recognizes link families 0x1e/0x1c and gives type 0x8e to its
    # service-frame parser rather than a radio handler.
    0x273B2C: ("push", "{r4, r5, r6, lr}"),
    0x273B36: ("ldrb", "r1, [r0]"),
    0x273B38: ("cmp", "r1, #0x1e"),
    0x273B3C: ("cmp", "r1, #0x1c"),
    0x273B40: ("ldrb", "r1, [r0, #3]"),
    0x273B42: ("cmp", "r1, #0x8d"),
    0x273B46: ("cmp", "r1, #0x8e"),
    0x273B4A: ("bl", "#0x273368"),
    # Startup authors a seven-byte discovery frame: caller-supplied link
    # family, ff, 00, class d0, word 1, and byte 6 set to 1.
    0x273878: ("push", "{r4, r5, lr}"),
    0x27387C: ("movs", "r0, #7"),
    0x27387E: ("bl", "#0x260abc"),
    0x27388C: ("strb", "r5, [r4]"),
    0x27388E: ("movs", "r0, #0xff"),
    0x273890: ("strb", "r0, [r4, #1]"),
    0x273892: ("movs", "r0, #0"),
    0x273894: ("strb", "r0, [r4, #2]"),
    0x273896: ("movs", "r0, #0xd0"),
    0x273898: ("strb", "r0, [r4, #3]"),
    0x27389A: ("movs", "r0, #1"),
    0x27389C: ("strh", "r0, [r4, #4]"),
    0x27389E: ("strb", "r0, [r4, #6]"),
    0x2738D8: ("movs", "r0, #0x1e"),
    0x2738DA: ("bl", "#0x273878"),
}
NSE3_TASK_9_ENTRY_POINTER = 0x2B751C
NSE3_TASK_9_ENTRY = 0x273B2D
DSP_PARAMETER_08_ANCHORS = {
    # A bounded message dispatcher maps 0x076f..0x0778 through this exact
    # ten-entry table.  Three entries are paired controller-flag setters and
    # clearers that immediately run the parameter-state updater.
    0x257F34: ("lsls", "r0, r0, #2"),
    0x257F36: ("ldr", "r1, [pc, #0x3cc]"),
    0x257F38: ("subs", "r1, r0, r1"),
    0x257F3A: ("adr", "r0, #4"),
    0x257F3C: ("ldr", "r0, [r0, r1]"),
    0x257F3E: ("mov", "pc, r0"),
    # Message 0x076f/0x0770 sets/clears controller flag 0x02.
    0x2910AE: ("push", "{lr}"),
    0x2910B0: ("ldr", "r0, [pc, #0x328]"),
    0x2910B6: ("ldr", "r1, [pc, #0x31c]"),
    0x2910B8: ("movs", "r0, #2"),
    0x2910BC: ("orrs", "r0, r2"),
    0x2910C0: ("bl", "#0x283560"),
    0x2910C6: ("push", "{lr}"),
    0x2910C8: ("ldr", "r0, [pc, #0x314]"),
    0x2910CE: ("ldr", "r1, [pc, #0x304]"),
    0x2910D0: ("movs", "r0, #0xfd"),
    0x2910D4: ("ands", "r0, r2"),
    0x2910D8: ("bl", "#0x283560"),
    # Message 0x0773/0x0774 sets/clears controller flag 0x04.
    0x2910DE: ("push", "{lr}"),
    0x2910E0: ("ldr", "r0, [pc, #0x300]"),
    0x2910E6: ("ldr", "r1, [pc, #0x2ec]"),
    0x2910E8: ("movs", "r0, #4"),
    0x2910EC: ("orrs", "r0, r2"),
    0x2910F0: ("bl", "#0x283560"),
    0x2910F6: ("push", "{lr}"),
    0x2910F8: ("ldr", "r0, [pc, #0x2ec]"),
    0x2910FE: ("ldr", "r1, [pc, #0x2d4]"),
    0x291100: ("movs", "r0, #0xfb"),
    0x291104: ("ands", "r0, r2"),
    0x291108: ("bl", "#0x283560"),
    # Message 0x0777/0x0778 sets/clears controller flag 0x10.
    0x291334: ("push", "{lr}"),
    0x291336: ("ldr", "r0, [pc, #0x128]"),
    0x29133C: ("ldr", "r1, [pc, #0x94]"),
    0x29133E: ("movs", "r0, #0x10"),
    0x291342: ("orrs", "r0, r2"),
    0x291346: ("bl", "#0x283560"),
    0x29134C: ("push", "{lr}"),
    0x29134E: ("ldr", "r0, [pc, #0x114]"),
    0x291354: ("ldr", "r1, [pc, #0x7c]"),
    0x291356: ("movs", "r0, #0xef"),
    0x29135A: ("ands", "r0, r2"),
    0x29135E: ("bl", "#0x283560"),
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
    # The normal parameter-state updater constructs a live parameter block at
    # 0x10c020 from controller state, then compares it with the shadow block
    # at 0x10c008.  The first halfword is selector 8.
    0x2835A4: ("ldr", "r5, [pc, #0x3b4]"),
    0x2837BE: ("ldr", "r0, [pc, #0x39c]"),
    0x2837C0: ("ldrh", "r0, [r0]"),
    0x2837C2: ("ldrh", "r1, [r5]"),
    0x2837C4: ("ands", "r0, r1"),
    0x2837C6: ("strh", "r0, [r5]"),
    0x2837D8: ("ldr", "r2, [pc, #0x3a4]"),
    0x2837DE: ("ldrh", "r1, [r2, r1]"),
    0x2837E0: ("orrs", "r1, r0"),
    0x2837E2: ("strh", "r1, [r5]"),
    0x283AC4: ("ldr", "r0, [pc, #0x34c]"),
    0x283AC6: ("ldrh", "r0, [r0]"),
    0x283AC8: ("ldrh", "r1, [r5]"),
    0x283ACA: ("ands", "r0, r1"),
    0x283ACC: ("strh", "r0, [r5]"),
    0x283AE8: ("ldr", "r2, [pc, #0x32c]"),
    0x283AEE: ("ldrh", "r0, [r2, r0]"),
    0x283AF0: ("orrs", "r0, r1"),
    0x283AF2: ("strh", "r0, [r5]"),
    0x283B0E: ("ldr", "r0, [pc, #0x228]"),
    0x283B10: ("mov", "r8, r0"),
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
    0x257F36: 0x1DBC,
    0x2910B0: 0x7809,
    0x2910B6: 0x10AE9F,
    0x2910C8: 0x780A,
    0x2910CE: 0x10AE9F,
    0x2910E0: 0x7807,
    0x2910E6: 0x10AE9F,
    0x2910F8: 0x7808,
    0x2910FE: 0x10AE9F,
    0x291336: 0x780D,
    0x29133C: 0x10AE9F,
    0x29134E: 0x780E,
    0x291354: 0x10AE9F,
    0x285DE8: 0xFFFF8000,
    0x285DF0: 0x10B972,
    0x285D22: 0x100A8,
    0x2391E8: 0x2B986C,
    0x2835A4: 0x10C020,
    0x2837BE: 0x2B8608,
    0x2837D8: 0x2B85F8,
    0x283AC4: 0x2B861C,
    0x283AE8: 0x2B860C,
    0x283B0E: 0x10C008,
    0x283D68: 0x2B85EC,
}
DSP_PARAMETER_JUMP_TABLE_ADDRESS = 0x285BB0
DSP_PARAMETER_08_MODE_TABLE_ADDRESS = 0x2B986C
DSP_PARAMETER_08_MODE_VALUES = [0x0600] * 9
DSP_PARAMETER_SELECTOR_TABLE_ADDRESS = 0x2B85EC
DSP_PARAMETER_SELECTOR_TABLE = bytes(
    [0x08, 0x09, 0x1B, 0x25, 0x20, 0x21, 0x22, 0x23, 0x24, 0x28, 0x2D]
)
DSP_PARAMETER_EVENT_TABLE_ADDRESS = 0x257F40
DSP_PARAMETER_EVENT_BASE = 0x076F
DSP_PARAMETER_EVENT_TARGETS = [
    0x25912C,
    0x259126,
    0x25911C,
    0x259110,
    0x259106,
    0x259100,
    0x259442,
    0x259442,
    0x2590FA,
    0x2590F4,
]
DSP_PARAMETER_EVENT_PRODUCER_ANCHORS = {
    # Exact relocated counterparts of the independently reviewed NSE-8 event
    # construction APIs.
    0x29E556: ("lsls", "r0, r0, #0x10"),
    0x29E558: ("push", "{r0, r1, r2, r3}"),
    0x29E55A: ("push", "{r4, r7, lr}"),
    0x29E566: ("ldrh", "r0, [r7]"),
    0x29E568: ("strh", "r0, [r4]"),
    0x29E604: ("lsls", "r0, r0, #0x10"),
    0x29E606: ("push", "{r0, r1, r2, r3}"),
    0x29E608: ("push", "{r4, r5, r7, lr}"),
    0x29E60C: ("ldrh", "r0, [r7]"),
    0x29E612: ("cmp", "r0, #0xdc"),
    0x29E616: ("ldrh", "r0, [r7]"),
    0x29E618: ("bl", "#0x29e5e8"),
}
DSP_PARAMETER_UNRESOLVED_EVENT_CALLS = [
    0x25A87E,
]
DSP_PARAMETER_RUNTIME_EVENT_ANCHORS = {
    # Four runtime-built posts read the event stored by the common constructor.
    0x2516B4: ("strh", "r6, [r1, #0x14]"),
    0x251852: ("bl", "#0x251690"),
    0x25186E: ("bl", "#0x251822"),
    0x25187C: ("bl", "#0x251690"),
    0x25340C: ("ldrh", "r2, [r4, #0x14]"),
    0x25340E: ("bl", "#0x251690"),
    0x26DBE4: ("adds", "r7, r0, #0"),
    0x26DC9C: ("adds", "r2, r7, #0"),
    0x26DC9E: ("bl", "#0x251690"),
    # The fifth post reads the completion event stored by the sole extended
    # constructor call.
    0x251834: ("strh", "r3, [r0, #0x16]"),
    0x25186A: ("movs", "r3, #0x4d"),
    0x25186C: ("lsls", "r3, r3, #3"),
    # One independent callsite has a two-value local construction.
    0x25A1E4: ("movs", "r0, #0xe3"),
    0x25A1E6: ("lsls", "r0, r0, #2"),
    0x25A1EA: ("ldr", "r0, [pc, #0x300]"),
    # The final classified call indexes two 16-byte records for every possible
    # value of a byte-sized mode selector.
    0x2A3458: ("ldrb", "r1, [r1]"),
    0x2A345A: ("lsls", "r1, r1, #1"),
    0x2A345C: ("adds", "r0, r0, r1"),
    0x2A345E: ("lsls", "r0, r0, #4"),
    0x2A3464: ("ldrh", "r5, [r4, #4]"),
    # Startup clears the whole ordinary SRAM arena, then applies a counted
    # copy table.  This lets the verifier distinguish zero-initialized runtime
    # records from fixed data copied out of flash.
    0x2000F4: ("ldr", "r0, [pc, #0x7c]"),
    0x2000F6: ("ldr", "r1, [pc, #0x80]"),
    0x200108: ("cmp", "r1, #0x2f"),
    0x20011C: ("ldr", "r0, [pc, #0x5c]"),
    0x200130: ("ldr", "r1, [r0, #4]"),
    0x200144: ("ldrb", "r4, [r0]"),
    0x200148: ("strb", "r4, [r1]"),
    # The remaining call's SRAM-table event is copied from a 28-byte
    # registration descriptor by this sole table constructor.
    0x25A4F8: ("ldr", "r6, [pc, #0x370]"),
    0x25A4FE: ("ldrb", "r1, [r4, #0x19]"),
    0x25A510: ("ldrh", "r1, [r2, #0x12]"),
    0x25A512: ("strh", "r1, [r4, #0x12]"),
    0x25A5B2: ("cmp", "r5, #0x50"),
    # One runtime descriptor is intentionally left unresolved.  Its 28-byte
    # allocation is returned from the free-list payload at header+8 without a
    # clear, and the r1==0 branch does not write descriptor[0x12].  The other
    # two branches explicitly store 0x13cf or 0x13ce.
    0x256DD2: ("movs", "r0, #0x1c"),
    0x256DD4: ("bl", "#0x260abc"),
    0x256E08: ("cmp", "r1, #0"),
    0x256E0C: ("movs", "r0, #0x5c"),
    0x256E18: ("strh", "r0, [r4, #0x12]"),
    0x256E20: ("strh", "r0, [r4, #0x12]"),
    0x260C68: ("str", "r0, [r4, #4]"),
    0x260CAC: ("movs", "r0, #8"),
    0x260CAE: ("adds", "r0, r0, r4"),
    # The unresolved publisher has two genuinely different backing formats.
    # Group mode zero indexes 24-byte object records and reads event +0x0e;
    # nonzero mode follows the registered 28-byte SRAM record chain and reads
    # event +0x12.
    0x25A7F4: ("ldrb", "r0, [r6, #9]"),
    0x25A7FA: ("ldr", "r1, [r6]"),
    0x25A800: ("ldrb", "r3, [r6, #6]"),
    0x25A80E: ("ldrh", "r3, [r0, #0xe]"),
    0x25A83C: ("ldrh", "r1, [r0, #0x12]"),
    # The object emitter preserves its r0 input in fp; all direct entries
    # below supply 0xff, which becomes event 0x0389's second argument.
    0x25A7DE: ("mov", "fp, r0"),
    0x25ADA2: ("movs", "r0, #0xff"),
    0x25ADA4: ("bl", "#0x25a7d0"),
    0x25AE88: ("movs", "r0, #0xff"),
    0x25AE8A: ("bl", "#0x25a7d0"),
    0x25B074: ("movs", "r0, #0xff"),
    0x25B076: ("bl", "#0x25a7d0"),
    0x25B0B0: ("movs", "r0, #0xff"),
    0x25B0B2: ("bl", "#0x25a7d0"),
    # Two runtime descriptors explicitly store event 0x0389.  Their complete
    # stack template gives field +0x0c as 0x13/0x10 and clears flag +0x18.
    0x22D654: ("movs", "r0, #0"),
    0x22D658: ("str", "r0, [sp, #0x10]"),
    0x22D65E: ("strh", "r1, [r0, #0x1a]"),
    0x22D668: ("strb", "r0, [r1]"),
    0x22D678: ("movs", "r0, #0x13"),
    0x22D67A: ("str", "r0, [sp, #0x14]"),
    0x22D686: ("bl", "#0x25a4f0"),
    0x22D692: ("movs", "r0, #0x10"),
    0x22D694: ("str", "r0, [sp, #0x14]"),
    0x22D6A0: ("bl", "#0x25a4f0"),
    # The reused-heap descriptor's event can remain stale on one branch, but
    # field +0x0c is overwritten by the zero-extended byte loop index.
    0x256DEC: ("movs", "r7, #0"),
    0x256E26: ("str", "r7, [r4, #0xc]"),
    0x256E32: ("adds", "r0, r7, #1"),
    0x256E34: ("lsls", "r0, r0, #0x18"),
    0x256E36: ("lsrs", "r7, r0, #0x18"),
    # Runtime descriptor 0x278792 is the organic bridge into the sole
    # event-fed object constructor: +0x0c is ROM catalogue 0x2b01d8 and
    # flag +0x18 is the producer's 0x40 gate.
    0x278770: ("str", "r0, [sp, #8]"),
    0x278772: ("str", "r4, [sp, #0xc]"),
    0x278776: ("ldr", "r0, [pc, #0x34c]"),
    0x278778: ("str", "r0, [sp, #0x14]"),
    0x278780: ("ldr", "r0, [pc, #0x344]"),
    0x278782: ("strh", "r0, [r1, #0x1a]"),
    0x278788: ("movs", "r1, #0x40"),
    0x27878A: ("strb", "r1, [r0]"),
    0x278792: ("bl", "#0x25a4f0"),
    # The final runtime descriptor enumerates PPM TEXT children.  It skips
    # value 0x33, stops at zero, and stores every other child word at +0x0c.
    0x28B5E8: ("ldr", "r0, [r4]"),
    0x28B5EA: ("cmp", "r0, #0x33"),
    0x28B5EE: ("cmp", "r0, #0"),
    0x28B5F2: ("str", "r0, [sp, #0x24]"),
    0x28B628: ("bl", "#0x25a4f0"),
    # Firmware PPM root validation and NEXT/TEXT traversal primitives.
    0x2A0BF2: ("ldr", "r0, [pc, #0x188]"),
    0x2A0BFA: ("ldr", "r1, [r0]"),
    0x2A0BFC: ("ldr", "r2, [pc, #0x180]"),
    0x2A0C4E: ("ldr", "r0, [r4]"),
    0x2A0C50: ("movs", "r1, #0x2c"),
    0x2A0C5A: ("ldr", "r2, [r1, #8]"),
    0x2A0CC6: ("ldr", "r0, [r3, #4]"),
    0x2A0CCA: ("lsrs", "r0, r0, #2"),
    0x2A0CCC: ("lsls", "r0, r0, #2"),
    # The allocator chooses among eight runtime-built size classes.
    0x260AF8: ("ldr", "r0, [r1, #0x18]"),
    0x260B04: ("ldrh", "r1, [r1, #2]"),
    0x260B06: ("cmp", "r1, r6"),
    0x260B10: ("cmp", "r4, #8"),
    # One exact 28-byte owner fills every payload byte from serial EEPROM
    # address 0x40, then releases it to the same general allocator.
    0x28EDF8: ("movs", "r0, #0x1c"),
    0x28EDFA: ("bl", "#0x260abc"),
    0x28EE06: ("movs", "r0, #0x40"),
    0x28EE08: ("adds", "r1, r5, #0"),
    0x28EE0A: ("movs", "r2, #0x1c"),
    0x28EE0C: ("bl", "#0x29cf2a"),
    0x28EF08: ("adds", "r0, r5, #0"),
    0x28EF0A: ("bl", "#0x26069c"),
    # The paired field writer first preserves the complete EEPROM block.  An
    # inbound type-0xcb message supplies its byte-9 selector and byte-10
    # payload directly; selector 4 replaces offsets 17..20, including the
    # stale event halfword at offsets 18..19, before writing all 28 bytes.
    0x23821E: ("ldrb", "r0, [r4, #8]"),
    0x238220: ("cmp", "r0, #0xcb"),
    0x238224: ("ldrb", "r0, [r4, #9]"),
    0x238226: ("movs", "r1, #0xa"),
    0x238228: ("adds", "r1, r1, r4"),
    0x23822A: ("bl", "#0x28ecec"),
    0x23A03C: ("movs", "r0, #0xc9"),
    0x23A046: ("subs", "r0, #1"),
    0x23A048: ("cmp", "r0, #1"),
    0x23A04A: ("bhi", "#0x23a04e"),
    0x23A04C: ("b", "#0x23a18a"),
    0x23A18A: ("adds", "r0, r4, #0"),
    0x23A18C: ("bl", "#0x238218"),
    0x28ECF4: ("movs", "r0, #0x40"),
    0x28ECF8: ("movs", "r2, #0x1c"),
    0x28ECFA: ("bl", "#0x29cf2a"),
    0x28ED12: ("subs", "r0, #1"),
    0x28ED16: ("beq", "#0x28ed46"),
    0x28ED5A: ("strb", "r0, [r2, #0x11]"),
    0x28ED64: ("cmp", "r1, #4"),
    0x28EDC6: ("movs", "r0, #0x40"),
    0x28EDCA: ("movs", "r2, #0x1c"),
    0x28EDCC: ("bl", "#0x29ce12"),
    # Event 0x0389 is the sole dispatcher case that reaches the remaining
    # runtime object constructor.  Its packed arguments are copied into the
    # runtime cell before the case loads cell[0] as the object input.
    0x27BA10: ("movs", "r1, #0xe1"),
    0x27BA12: ("lsls", "r1, r1, #2"),
    0x27BA1C: ("subs", "r1, #3"),
    0x27BA22: ("b", "#0x27c174"),
    0x27C174: ("ldr", "r0, [pc, #0x2a0]"),
    0x27C176: ("ldr", "r0, [r0]"),
    0x27C17C: ("bl", "#0x25b0cc"),
    0x29DFB8: ("ldr", "r1, [pc, #0x190]"),
    0x29DFBC: ("str", "r2, [r1]"),
}
DSP_PARAMETER_MODE_EVENT_TABLE_ADDRESS = 0x2B43F0
DSP_PARAMETER_RUNTIME_RECORD_TABLE_ADDRESS = 0x1061A4
DSP_PARAMETER_OBJECT_GROUP_TABLE_ADDRESS = 0x106A64
DSP_PARAMETER_RUNTIME_OBJECT_CELL_ADDRESS = 0x10B284
DSP_PARAMETER_UNRESOLVED_OBJECT_CONSTRUCTORS = [0x27C17C]
DSP_PARAMETER_RUNTIME_OBJECT_EVENT = 0x0389
DSP_PARAMETER_RUNTIME_OBJECT_EVENT_PRODUCERS = {
    0x231660: [0x0000, 0x0000],
    0x25A8FA: [None, None],
    0x25B044: [None, 0x0000],
}
DSP_PARAMETER_OBJECT_EMITTER_ADDRESS = 0x25A7D0
DSP_PARAMETER_OBJECT_EMITTER_CALLS = {
    0x25ADA4: 0x00FF,
    0x25AE8A: 0x00FF,
    0x25B076: 0x00FF,
    0x25B0B2: 0x00FF,
}
DSP_PARAMETER_RUNTIME_OBJECT_EVENT_BOUNDED_ARGUMENTS = {
    0x231660: [0x0000, 0x0000],
    0x25A8FA: [None, 0x00FF],
    0x25B044: [None, 0x0000],
}
DSP_PARAMETER_FIXED_OBJECT_VALUE_CENSUS = {
    "records": 124,
    "unique_values": 32,
    "rom_catalogue_values": 8,
    "emitter_eligible_records": 31,
    "emitter_eligible_rom_catalogue_values": 0,
    "dispatch_bit_eligible_records": 0,
}
DSP_PARAMETER_EXPLICIT_OBJECT_EVENT_DESCRIPTORS = {
    0x22D686: {"value": 0x0013, "flags": 0x00, "event": 0x0389},
    0x22D6A0: {"value": 0x0010, "flags": 0x00, "event": 0x0389},
}
DSP_PARAMETER_RUNTIME_OBJECT_INSTALLER = {
    "registration_callsite": 0x278792,
    "stored_event": 0x0387,
    "value": 0x2B01D8,
    "flags": 0x40,
    "producer_callsite": 0x25B044,
    "constructor_event": 0x0389,
    "constructor_callsite": 0x27C17C,
}
DSP_PARAMETER_RUNTIME_OBJECT_CATALOGUE = {
    "address": 0x2B01D8,
    "records": 0x2B00E8,
    "record_count": 9,
    "events": [
        0x00DC,
        0x05E0,
        0x05E0,
        0x05E0,
        0x0387,
        0x05E0,
        0x05E0,
        0x0387,
        0x01F4,
    ],
    "values": [0, 0x47, 0x0D, 0x0C, 0x2B00DC, 0x6D, 0x0B, 0x2B043C, 0],
    "record_flags": [0, 8, 8, 8, 8, 8, 8, 8, 8],
}
DSP_PARAMETER_PPM_ROOT_POINTER_CELL = 0x2BEAE8
DSP_PARAMETER_PPM_ROOT = 0x2C0000
DSP_PARAMETER_PPM_TOP_LEVEL_NODES = [
    {"address": 0x2C002C, "length": 0x0234, "tag": 0x4C504353},
    {"address": 0x2C0260, "length": 0x01B4, "tag": 0x47534D43},
    {"address": 0x2C0414, "length": 0x28BC, "tag": 0x464F4E54},
    {"address": 0x2C2CD0, "length": 0x35998, "tag": 0x54455854},
]
DSP_PARAMETER_PPM_TEXT_CHILDREN = [
    {"address": 0x2C2CE4, "value": 0x33, "length": 0x023E},
    {"address": 0x2C2F24, "value": 0x01, "length": 0x5043},
    {"address": 0x2C7F68, "value": 0x02, "length": 0x5A4A},
    {"address": 0x2CD9B4, "value": 0x0D, "length": 0x52DA},
    {"address": 0x2D2C90, "value": 0x0F, "length": 0x52D3},
    {"address": 0x2D7F64, "value": 0x18, "length": 0x513E},
    {"address": 0x2DD0A4, "value": 0x1A, "length": 0xA290},
    {"address": 0x2E7334, "value": 0x1B, "length": 0xB6A8},
    {"address": 0x2F29DC, "value": 0x13, "length": 0x5C79},
    {"address": 0x2F8658, "value": 0x00, "length": 0x0010},
]
DSP_PARAMETER_PPM_DESCRIPTOR_VALUES = [
    0x01,
    0x02,
    0x0D,
    0x0F,
    0x18,
    0x1A,
    0x1B,
    0x13,
]
DSP_PARAMETER_UNRESOLVED_RUNTIME_VALUE_CALLS = []
DSP_PARAMETER_ALLOCATOR_CENSUS = {
    "calls": 1105,
    "resolved_sizes": 924,
    "runtime_sizes": 181,
    "exact_28_byte_calls": [
        0x214958,
        0x21522E,
        0x240922,
        0x256D4E,
        0x256DD4,
        0x27ACDE,
        0x27CD80,
        0x27E128,
        0x28EDFA,
    ],
}
DSP_PARAMETER_STALE_EVENT_REUSE_OWNER = {
    "allocation_callsite": 0x28EDFA,
    "allocation_size": 0x1C,
    "eeprom_read_callsite": 0x28EE0C,
    "eeprom_address": 0x40,
    "eeprom_length": 0x1C,
    "release_callsite": 0x28EF0A,
    "payload_event_offset": 0x12,
    "payload_event_value": "eeprom_data",
    "eeprom_field_writer": {
        "request_handler": 0x238218,
        "request_type": 0xCB,
        "selector_message_offset": 9,
        "payload_message_offset": 10,
        "writer": 0x28ECEC,
        "field_selector": 4,
        "field_block_offsets": [0x11, 0x12, 0x13, 0x14],
        "event_offsets": [0x12, 0x13],
        "write_callsite": 0x28EDCC,
    },
    "payload_event_mutability": "runtime_request_writable",
}
DSP_PARAMETER_EEPROM_WRITE_CENSUS = {
    "calls": 44,
    "resolved_addresses": 32,
    "runtime_addresses": 12,
    "event_byte_range": [0x52, 0x54],
    "resolved_overlapping_calls": [
        {
            "callsite": 0x28EDCC,
            "address": 0x40,
            "length": 0x1C,
        },
    ],
    "field_writer_direct_calls": [0x23822A],
}
NSE3_COPY_TABLE_ADDRESS = 0x2A5008
DSP_PARAMETER_RUNTIME_DESCRIPTOR_EVENTS = {
    0x221640: [0x0C44],
    0x22168A: [0x0C44],
    0x221C34: [0x0C4A],
    0x221C80: [0x0C4A],
    0x2258CC: [0x0C4A],
    0x22D686: [0x0389],
    0x22D6A0: [0x0389],
    0x256D96: [0x00DC],
    0x25A604: [0x00DC],
    0x265094: list(range(8)) + [0x0C60],
    0x27018C: [0x01C2],
    0x2701A8: [0x01C2],
    0x270286: [0x01C2],
    0x2702D2: [0x01C2],
    0x270610: [0x01B0],
    0x270D70: [0x01A7],
    0x270DDC: [0x01A7],
    0x278454: [0x0322],
    0x27851C: [0x00DC, 0x0230],
    0x27876A: [0x0230],
    0x278792: [0x0387],
    0x2787B8: [0x05E0],
    # 0x27ad0c forwards r3 through 0x27acc0.  Four callers are fixed and the
    # fifth supplies its loop index r4, bounded to 0..4.
    0x27AD0C: list(range(5)) + [0x0387, 0x0394, 0x1964, 0x1965],
    0x27CDF0: [0x0FA4],
    0x27E11E: [0x0AB5],
    # Both calls begin with the complete 28-byte template at 0x2bc75c.
    0x2847A8: [0x0000],
    0x2847F0: [0x0000],
    0x28B5DA: [0x03A2],
    0x28B628: [0x03A5, 0x03FB],
}
DSP_PARAMETER_UNRESOLVED_RUNTIME_DESCRIPTORS = [0x256E2E]
DSP_PARAMETER_UNRESOLVED_RUNTIME_DESCRIPTOR_REASON = {
    "callsite": 0x256E2E,
    "allocator": 0x260ABC,
    "allocation_size": 0x1C,
    "explicit_events": [0x13CE, 0x13CF],
    "unwritten_event_branch": 0x256E0C,
    "allocator_clears_payload": False,
    "value_field_source": "zero_extended_byte_loop_index",
    "value_field_can_be_rom_address": False,
}
DSP_BOOTSTRAP_ANCHORS = {
    # Shared bootstrap/header setup.
    0x2858FC: ("push", "{r4, r5, r6, r7, lr}"),
    0x285904: ("ldr", "r5, [pc, #0x118]"),
    0x285906: ("strh", "r0, [r5]"),
    0x285908: ("strh", "r0, [r5, #2]"),
    0x28592A: ("ldr", "r1, [pc, #0x22c]"),
    # Seven fixed shared header/control halfwords.
    0x28592C: ("movs", "r0, #0xff"),
    0x28592E: ("adds", "r0, #1"),
    0x285930: ("strh", "r0, [r1]"),
    0x285934: ("movs", "r0, #3"),
    0x285936: ("lsls", "r0, r0, #8"),
    0x285938: ("strh", "r0, [r1]"),
    0x28593C: ("strh", "r4, [r1]"),
    0x285940: ("movs", "r0, #1"),
    0x285942: ("lsls", "r0, r0, #0xf"),
    0x285944: ("strh", "r0, [r1]"),
    0x285948: ("movs", "r0, #1"),
    0x28594A: ("strh", "r0, [r1]"),
    0x28594E: ("strh", "r0, [r1]"),
    0x285952: ("lsls", "r0, r0, #9"),
    0x285954: ("strh", "r0, [r1]"),
    # MAD2 byte 0x20002 bit 0 is asserted before staging.
    0x285956: ("bl", "#0x29ae76"),
    0x28595A: ("ldr", "r1, [pc, #0x200]"),
    0x28595C: ("movs", "r0, #1"),
    0x28595E: ("ldrb", "r2, [r1]"),
    0x285960: ("orrs", "r0, r2"),
    0x285962: ("strb", "r0, [r1]"),
    0x285964: ("bl", "#0x29ae90"),
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
    # The same MAD2 bit is released after both result captures.
    0x2859EE: ("bl", "#0x29ae76"),
    0x2859F2: ("movs", "r0, #0xfe"),
    0x2859F4: ("ldrb", "r1, [r5, #2]"),
    0x2859F6: ("ands", "r0, r1"),
    0x2859F8: ("strb", "r0, [r5, #2]"),
    0x2859FA: ("bl", "#0x29ae90"),
    0x2859FE: ("pop", "{r4, r5, r6, r7, pc}"),
}
DSP_BOOTSTRAP_LITERALS = {
    0x285904: 0x10000,
    0x28592A: 0x100F6,
    0x28595A: 0x20002,
    0x285970: 0x100FE,
    0x285972: 0x10200,
    0x285976: 0x200040,
    0x2859BC: 0xFFFF,
    0x2859C6: 0x20000,
}
DSP_BOOTSTRAP_HEADER = [
    {"address": 0x100F6, "value": 0x0100},
    {"address": 0x100F8, "value": 0x0300},
    {"address": 0x100FA, "value": 0x0000},
    {"address": 0x100FC, "value": 0x8000},
    {"address": 0x100FE, "value": 0x0001},
    {"address": 0x10100, "value": 0x0001},
    {"address": 0x10102, "value": 0x0200},
]
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
DSP_BOOTSTRAP_STATE_BASE = 0x10B970
DSP_BOOTSTRAP_STATE_BASE_REFERENCES = [
    0x28574A,
    0x285842,
    0x2859E4,
    0x285A50,
    0x285B1C,
    0x285D0C,
    0x285D2C,
    0x285D4C,
    0x285D6E,
    0x285E9E,
    0x285F3C,
    0x297104,
    0x297246,
]
DSP_BOOTSTRAP_POST_ANCHORS = {
    # The wrapper preserves the pre-transfer MAD2 byte in r4.
    0x2973C8: ("bl", "#0x297356"),
    0x2973CC: ("adds", "r4, r0, #0"),
    0x2973F0: ("bl", "#0x2858fc"),
    # Its low bit selects only an EEPROM configuration update. Neither
    # captured publication nor a bootstrap return code is tested here.
    0x2973F4: ("lsrs", "r0, r4, #1"),
    0x2973F6: ("blo", "#0x2973fe"),
    0x2973F8: ("movs", "r0, #0"),
    0x2973FA: ("bl", "#0x28ef38"),
    0x2973FE: ("bl", "#0x260252"),
    # The conditional helper reads EEPROM word 0x74 and writes only if the
    # requested zero differs.
    0x28EF3E: ("strh", "r0, [r1]"),
    0x28EF40: ("movs", "r0, #0x74"),
    0x28EF46: ("bl", "#0x29cf2a"),
    0x28EF52: ("cmp", "r0, r1"),
    0x28EF54: ("beq", "#0x28ef62"),
    0x28EF56: ("movs", "r0, #0x74"),
    0x28EF5C: ("bl", "#0x29ce12"),
    # The following initialization is unconditional and does not receive a
    # bootstrap result argument.
    0x260252: ("push", "{r4, r5, r6, lr}"),
    0x260254: ("ldr", "r1, [pc, #0x44]"),
    0x260258: ("strb", "r0, [r1]"),
    0x26025A: ("ldr", "r4, [pc, #0x1e4]"),
    0x260260: ("ldr", "r0, [pc, #0x300]"),
}
DSP_BOOTSTRAP_POST_LITERALS = {
    0x297358: 0x20001,
    0x260254: 0x2000C,
    0x26025A: 0x100020,
    0x260260: 0x10C284,
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
DSP_EXTERNAL_SOFTWARE_POINTER_TABLE = 0x2AB52C
DSP_EXTERNAL_SOFTWARE_STRING = 0x286098
DSP_EXTERNAL_SOFTWARE_BYTES = (
    b" 25.3.531 \n"
    b"17-Dec-97\n"
    b"NSE-3Nx\n"
    b"(c) NMP.\x00"
)
DSP_INTERNAL_SOFTWARE_ANCHORS = {
    # Startup clears the first byte of both independently formatted buffers.
    0x237BF2: ("ldr", "r0, [pc, #0x270]"),
    0x237BF4: ("strb", "r7, [r0]"),
    0x237BF6: ("ldr", "r0, [pc, #0x270]"),
    0x237BF8: ("strb", "r7, [r0]"),
    # The inbound handler accepts two report types and converts byte 0x0b
    # into a one-character ASCII identity for selector 9.
    0x237D66: ("ldrb", "r1, [r4, #8]"),
    0x237D8E: ("cmp", "r1, #0xc8"),
    0x237D90: ("beq", "#0x237dbc"),
    0x237D92: ("cmp", "r1, #0xa"),
    0x237D94: ("beq", "#0x237dbc"),
    0x237DBE: ("ldrb", "r1, [r4, #0xb]"),
    0x237DC0: ("adds", "r1, #0x30"),
    0x237DC2: ("strb", "r1, [r0, #4]"),
    0x237DC8: ("strb", "r0, [r1, #5]"),
    0x237DCA: ("movs", "r0, #9"),
    0x237DCE: ("bl", "#0x28ead2"),
    # Generic setter selector 9 copies at most ten bytes to 0x10bcf0.
    0x28EAD4: ("adds", "r5, r1, #0"),
    0x28EAD6: ("adds", "r6, r0, #0"),
    0x28EAF2: ("subs", "r0, #2"),
    0x28EAFC: ("cmp", "r7, #0xa"),
    0x28EB02: ("ldr", "r6, [pc, #0x2d8]"),
    0x28EB04: ("movs", "r0, #0xc"),
    0x28EB0C: ("bl", "#0x2a44fc"),
    0x28EB14: ("strb", "r0, [r1, #0xc]"),
}
DSP_INTERNAL_SOFTWARE_LITERALS = {
    0x237BF2: 0x10BCF0,
    0x237BF6: 0x10BCFC,
    0x28EA16: 0x10BCF0,
    0x28EB02: 0x10BCE4,
}
DSP_INTERNAL_SOFTWARE_OWNER_ANCHORS = {
    # DSPIF RX creates the ordinary queue envelope directly from a DSP-to-MCU
    # ring record: fixed source 0x18, destination task 2, ring length, then
    # ring type/family.  Thus family 0x74 is supplied across DSPIF rather than
    # by the bootstrap responder or external-service peer.
    0x2857DC: ("movs", "r1, #0x18"),
    0x2857DE: ("strb", "r1, [r0]"),
    0x2857E2: ("movs", "r1, #2"),
    0x2857E4: ("strb", "r1, [r0, #1]"),
    0x2857E6: ("ldr", "r0, [r5]"),
    0x2857E8: ("strb", "r7, [r0, #2]"),
    0x2857EA: ("ldr", "r0, [r5]"),
    0x2857EC: ("mov", "r1, r8"),
    0x2857EE: ("strb", "r1, [r0, #3]"),
    # The radio/DSPIF owner polls the DSP ring and submits that same object to
    # its normal router.  The route wrapper does not rewrite the envelope.
    0x2A2208: ("bl", "#0x25fd9c"),
    0x2A2216: ("bl", "#0x28580a"),
    0x2A221A: ("adds", "r4, r0, #0"),
    0x2A2222: ("adds", "r0, r4, #0"),
    0x2A2224: ("bl", "#0x2a1f52"),
    0x2A1F52: ("push", "{lr}"),
    0x2A1F54: ("adds", "r1, r0, #0"),
    0x2A1F56: ("movs", "r0, #0xd"),
    0x2A1F58: ("lsls", "r0, r0, #9"),
    0x2A1F5A: ("bl", "#0x2a1ec6"),
    # Task 2 entry and its object loop.
    0x23A5CE: ("push", "{r4, r5, r6, lr}"),
    0x23A5D0: ("bl", "#0x237a7a"),
    0x23A5D8: ("movs", "r6, #1"),
    # Object family 0x74 is dispatched to 0x237d60 except subcommand 0x32.
    0x23A5FE: ("ldrb", "r0, [r4, #3]"),
    0x23A624: ("subs", "r0, #0x32"),
    0x23A632: ("ldrb", "r0, [r4, #8]"),
    0x23A634: ("cmp", "r0, #0x32"),
    0x23A636: ("beq", "#0x23a640"),
    0x23A638: ("adds", "r0, r4, #0"),
    0x23A63A: ("bl", "#0x237d60"),
    0x23A63E: ("b", "#0x23a6a8"),
    # Identity reports retain result 1, skip fallback reporting and release
    # the received object without constructing an acknowledgement.
    0x23A6A8: ("cmp", "r6, #0xff"),
    0x23A6AA: ("bne", "#0x23a6b2"),
    0x23A6B2: ("adds", "r0, r4, #0"),
    0x23A6B4: ("bl", "#0x26069c"),
}
NSE3_TASK_TABLE = 0x2B74B0
NSE3_TASK_RECORD_SIZE = 12
NSE3_TASK_2_ENTRY_POINTER = 0x2B74C8
NSE3_TASK_2_ENTRY = 0x23A5CF
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


def verify_sim_apdu_boundary(data: bytes) -> dict:
    decode_thumb_anchors(data, SIM_APDU_CONSTRUCTOR_ANCHORS)
    return {
        "class": 0xA0,
        "constructor_extent": {"start": 0x286C40, "end": 0x2875A8},
        "common_submit": 0x286B6A,
        "constructors": SIM_APDU_CONSTRUCTORS,
        "unique_instructions": sorted(
            {constructor["instruction"] for constructor in SIM_APDU_CONSTRUCTORS}
        ),
        "sequence": "data_driven_requires_organic_boot_trace",
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
    decode_thumb_anchors(data, EXTERNAL_SERVICE_TRANSPORT_ANCHORS)
    verify_thumb_literals(data, RADIO_REPORT_HANDLER_LITERALS)
    physical = swap16(data)
    task_9_entry = effective_u32(
        physical, NSE3_TASK_9_ENTRY_POINTER - FLASH_BASE)
    if task_9_entry != NSE3_TASK_9_ENTRY:
        raise ValueError(
            "NSE-3 task-9 entry changed: expected "
            f"{NSE3_TASK_9_ENTRY:#x}, got {task_9_entry!r}"
        )
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
        "external_service_transport": {
            "dsp_report_types": [0x8D, 0x8E],
            "owner_task": 9,
            "task_entry": task_9_entry,
            "accepted_link_families": [0x1C, 0x1E],
            "discovery_frame": {
                "length": 7,
                "link_family": 0x1E,
                "destination": 0xFF,
                "source": 0x00,
                "class": 0xD0,
                "control_word": 1,
                "control_byte": 1,
            },
            "transport_component": "generic_nokia_external_service",
            "application_registration_grammar": "not_established",
            "peer_enablement": False,
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
    event_table_offset = DSP_PARAMETER_EVENT_TABLE_ADDRESS - FLASH_BASE
    event_targets = list(
        struct.unpack_from(
            f">{len(DSP_PARAMETER_EVENT_TARGETS)}I", data, event_table_offset
        )
    )
    if event_targets != DSP_PARAMETER_EVENT_TARGETS:
        raise ValueError(
            "NSE-3 DSP parameter event table changed: expected "
            f"{DSP_PARAMETER_EVENT_TARGETS}, got {event_targets}"
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
        "state_live_block": 0x10C020,
        "state_shadow_block": 0x10C008,
        "state_selector_table": list(selectors),
        "selector_08_state_slot": 0,
        "selector_08_state_construction": {
            "mask_tables": [0x2B8608, 0x2B861C],
            "indexed_value_tables": [0x2B85F8, 0x2B860C],
            "operation": "(previous & selected_mask) | indexed_value",
            "inside_controller_updater": True,
        },
        "controller_event_dispatch": {
            "message_range": [
                DSP_PARAMETER_EVENT_BASE,
                DSP_PARAMETER_EVENT_BASE + len(event_targets) - 1,
            ],
            "table": DSP_PARAMETER_EVENT_TABLE_ADDRESS,
            "targets": event_targets,
            "paired_flag_transitions": [
                {"set": 0x076F, "clear": 0x0770, "mask": 0x02},
                {"set": 0x0773, "clear": 0x0774, "mask": 0x04},
                {"set": 0x0777, "clear": 0x0778, "mask": 0x10},
            ],
            "flag_cell": 0x10AE9F,
            "speech_semantics": "not_established",
        },
        "organic_answer_value": "not_established",
        "organic_end_value": "not_established",
        "speech_role": "not_established_from_selector_number",
    }


def verify_dsp_parameter_event_producers(data: bytes) -> dict:
    decode_thumb_anchors(data, DSP_PARAMETER_EVENT_PRODUCER_ANCHORS)
    decode_thumb_anchors(data, DSP_PARAMETER_RUNTIME_EVENT_ANCHORS)
    physical = swap16(data)
    instructions = decode_image(physical, FLASH_BASE)
    profile = {
        "apis": [
            {
                "address": 0x29E604,
                "name": "generic_event_generate",
                "kind": "packed_event",
                "arguments": {"packed_event": "r0"},
            },
            {
                "address": 0x29E556,
                "name": "task5_render_post",
                "kind": "packed_event",
                "arguments": {"packed_event": "r0", "descriptor": "r1"},
            },
        ]
    }
    calls = extract_calls(profile, instructions, physical, FLASH_BASE)
    resolved = [
        call
        for call in calls
        if call["arguments"].get("packed_event") is not None
    ]
    target_events = set(
        range(DSP_PARAMETER_EVENT_BASE, DSP_PARAMETER_EVENT_BASE + 10)
    )
    matching = [
        call
        for call in resolved
        if (call["arguments"]["packed_event"] & 0x1FFF) in target_events
    ]
    if len(calls) != 946 or len(resolved) != 938:
        raise ValueError(
            "NSE-3 packed-event call census changed: expected 946/938 "
            f"total/resolved, got {len(calls)}/{len(resolved)}"
        )
    if matching:
        raise ValueError(
            "NSE-3 recovered a direct parameter-event producer unexpectedly: "
            f"{matching}"
        )
    runtime_calls = [
        call["callsite"]
        for call in calls
        if call["arguments"].get("packed_event") is None
    ]
    expected_runtime_calls = [
        0x2524CE,
        0x252C3E,
        0x252E7C,
        0x252F76,
        0x253552,
        0x25A1F8,
        0x25A87E,
        0x2A3472,
    ]
    if runtime_calls != expected_runtime_calls:
        raise ValueError(
            "NSE-3 runtime-built packed-event callsites changed: expected "
            f"{expected_runtime_calls}, got {runtime_calls}"
        )

    constructor_profile = {
        "apis": [
            {
                "address": 0x251690,
                "name": "stored_event_constructor",
                "kind": "constructor",
                "arguments": {"event": "r2"},
            },
            {
                "address": 0x251822,
                "name": "extended_stored_event_constructor",
                "kind": "constructor",
                "arguments": {"event": "r2", "completion_event": "r3"},
            },
            {
                "address": 0x26DBE2,
                "name": "stored_event_dispatch",
                "kind": "dispatcher",
                "arguments": {"event": "r0"},
            },
        ]
    }
    constructor_calls = extract_calls(
        constructor_profile, instructions, physical, FLASH_BASE
    )
    direct_constructor_events = {
        call["callsite"]: call["arguments"]["event"]
        for call in constructor_calls
        if call["api_address"] == 0x251690
    }
    expected_direct_constructor_events = {
        0x251852: None,  # forwarded by the sole extended constructor below
        0x25187C: 0x0578,
        0x25340E: None,  # reloads the existing field; does not enlarge its set
        0x256444: 0x0BBE,
        0x25654A: 0x0BC0,
        0x2573E8: 0x0BBE,
        0x257442: 0x0BBE,
        0x26DC9E: None,  # forwarded by the bounded dispatcher below
        0x2711F2: 0x08A1,
        0x281FA0: 0x083F,
        0x284A1C: 0x08DD,
        0x291CF6: 0x09CA,
        0x299D94: 0x06A8,
        0x2A1572: 0x0348,
        0x2A15AE: 0x0348,
        0x2A375E: 0x0A7A,
    }
    if direct_constructor_events != expected_direct_constructor_events:
        raise ValueError(
            "NSE-3 stored-event constructor census changed: expected "
            f"{expected_direct_constructor_events}, got {direct_constructor_events}"
        )
    extended_calls = [
        call for call in constructor_calls if call["api_address"] == 0x251822
    ]
    if len(extended_calls) != 1 or extended_calls[0]["callsite"] != 0x25186E:
        raise ValueError(
            "NSE-3 extended stored-event constructor call topology changed"
        )
    extended_arguments = extended_calls[0]["arguments"]
    if extended_arguments != {"event": 0x0578, "completion_event": 0x0268}:
        raise ValueError(
            "NSE-3 extended stored-event constructor values changed: "
            f"{extended_arguments}"
        )
    dispatch_events = {
        call["callsite"]: call["arguments"]["event"]
        for call in constructor_calls
        if call["api_address"] == 0x26DBE2
    }
    expected_dispatch_events = {
        0x26DD28: 0x0B55,
        0x26DD74: 0x0B55,
        0x26DE68: 0x0B57,
        0x26DE86: 0x0B56,
        0x26DF1E: 0x0B61,
        0x26E10C: 0x0B5D,
        0x26E1A8: 0x0B5E,
        0x26E7D6: 0x0B55,
        0x26E8B0: 0x0B58,
    }
    if dispatch_events != expected_dispatch_events:
        raise ValueError(
            "NSE-3 stored-event dispatcher inputs changed: expected "
            f"{expected_dispatch_events}, got {dispatch_events}"
        )
    stored_events = {
        value
        for value in direct_constructor_events.values()
        if value is not None
    } | set(dispatch_events.values()) | {extended_arguments["event"]}
    if {value & 0x1FFF for value in stored_events} & target_events:
        raise ValueError(
            "NSE-3 stored-event constructors can publish a parameter event"
        )
    if extended_arguments["completion_event"] in target_events:
        raise ValueError(
            "NSE-3 completion event can publish a parameter event"
        )

    fixed_local_events = {0x038C, 0x038E}
    if fixed_local_events & target_events:
        raise ValueError("NSE-3 fixed local event overlaps parameter events")

    mode_table_events = []
    for selector in range(256):
        for side in range(2):
            record = selector * 2 + side
            offset = (
                DSP_PARAMETER_MODE_EVENT_TABLE_ADDRESS
                - FLASH_BASE
                + record * 0x10
                + 4
            )
            mode_table_events.append(
                int.from_bytes(physical[offset : offset + 2], "little")
            )
    if {value & 0x1FFF for value in mode_table_events} & target_events:
        raise ValueError(
            "NSE-3 byte-indexed mode event table contains a parameter event"
        )

    # Reconstruct the startup copy image.  The firmware first clears
    # 0x100020..0x10c507, so addresses absent from this table begin as zero.
    initial_sram: dict[int, int] = {}
    copy_cursor = NSE3_COPY_TABLE_ADDRESS
    copy_records = 0
    runtime_table_copied = False
    object_group_table_copied = False
    runtime_object_cell_copied = False
    while True:
        offset = copy_cursor - FLASH_BASE
        size = (
            (int.from_bytes(physical[offset : offset + 4], "little") << 16)
            | (int.from_bytes(physical[offset : offset + 4], "little") >> 16)
        ) & 0xFFFFFFFF
        if size == 0:
            break
        destination_raw = int.from_bytes(
            physical[offset + 4 : offset + 8], "little"
        )
        destination = (
            (destination_raw << 16) | (destination_raw >> 16)
        ) & 0xFFFFFFFF
        source = copy_cursor + 8
        if (
            destination
            <= DSP_PARAMETER_RUNTIME_RECORD_TABLE_ADDRESS
            < destination + size
        ):
            runtime_table_copied = True
        if (
            destination
            <= DSP_PARAMETER_OBJECT_GROUP_TABLE_ADDRESS
            < destination + size
        ):
            object_group_table_copied = True
        if (
            destination
            <= DSP_PARAMETER_RUNTIME_OBJECT_CELL_ADDRESS
            < destination + size
        ):
            runtime_object_cell_copied = True
        for index in range(size):
            # Byte lanes cross within each flash halfword on this image.
            source_offset = (source - FLASH_BASE + index) ^ 1
            initial_sram[destination + index] = physical[source_offset]
        copy_cursor = source + ((size + 3) & ~3)
        copy_records += 1
        if copy_records > 512:
            raise ValueError("NSE-3 startup copy table did not terminate")
    if copy_records != 109 or copy_cursor != 0x2A586C:
        raise ValueError(
            "NSE-3 startup copy table changed: expected 109 records ending "
            f"at 0x2a586c, got {copy_records} ending at {copy_cursor:#x}"
        )
    if runtime_table_copied:
        raise ValueError(
            "NSE-3 runtime event-record table unexpectedly has a flash image"
        )
    if object_group_table_copied:
        raise ValueError(
            "NSE-3 object-group table unexpectedly has a flash image"
        )
    if runtime_object_cell_copied:
        raise ValueError(
            "NSE-3 runtime object cell unexpectedly has a flash image"
        )

    registration_profile = {
        "apis": [
            {
                "address": 0x25A4F0,
                "name": "runtime_record_register",
                "kind": "constructor",
                "arguments": {"descriptor": "r2"},
            }
        ]
    }
    registrations = extract_calls(
        registration_profile, instructions, physical, FLASH_BASE
    )
    fixed_registration_events = []
    runtime_registration_calls = []
    fixed_rom_descriptors = 0
    fixed_sram_descriptors = 0
    for call in registrations:
        descriptor = call["arguments"]["descriptor"]
        if descriptor is None:
            runtime_registration_calls.append(call["callsite"])
            continue
        if FLASH_BASE <= descriptor < FLASH_BASE + len(physical):
            event_offset = descriptor - FLASH_BASE + 0x12
            event = int.from_bytes(
                physical[event_offset : event_offset + 2], "little"
            )
            fixed_rom_descriptors += 1
        elif SRAM_BASE <= descriptor < SRAM_BASE + SRAM_SIZE:
            low = initial_sram.get(descriptor + 0x12, 0)
            high = initial_sram.get(descriptor + 0x13, 0)
            event = low | (high << 8)
            fixed_sram_descriptors += 1
        else:
            raise ValueError(
                f"NSE-3 registration descriptor outside flash/SRAM: "
                f"{descriptor:#x}"
            )
        fixed_registration_events.append(event)
    if (
        len(registrations) != 116
        or fixed_rom_descriptors != 81
        or fixed_sram_descriptors != 5
        or len(runtime_registration_calls) != 30
    ):
        raise ValueError(
            "NSE-3 runtime-record registration census changed: expected "
            "116 total, 81 ROM, 5 startup-SRAM and 30 runtime descriptors; "
            f"got {len(registrations)}, {fixed_rom_descriptors}, "
            f"{fixed_sram_descriptors}, {len(runtime_registration_calls)}"
        )
    expected_runtime_registration_calls = sorted(
        [
            *DSP_PARAMETER_RUNTIME_DESCRIPTOR_EVENTS,
            *DSP_PARAMETER_UNRESOLVED_RUNTIME_DESCRIPTORS,
        ]
    )
    if runtime_registration_calls != expected_runtime_registration_calls:
        raise ValueError(
            "NSE-3 runtime registration callsites changed: expected "
            f"{expected_runtime_registration_calls}, got "
            f"{runtime_registration_calls}"
        )
    if {value & 0x1FFF for value in fixed_registration_events} & target_events:
        raise ValueError(
            "NSE-3 fixed runtime-record registration can publish a "
            "parameter event"
        )
    classified_runtime_events = {
        value & 0x1FFF
        for values in DSP_PARAMETER_RUNTIME_DESCRIPTOR_EVENTS.values()
        for value in values
    }
    if classified_runtime_events & target_events:
        raise ValueError(
            "NSE-3 bounded runtime descriptor can publish a parameter event"
        )

    object_constructor_profile = {
        "apis": [
            {
                "address": 0x25B0CC,
                "name": "object_group_constructor",
                "kind": "constructor",
                "arguments": {"object": "r0"},
            }
        ]
    }
    object_constructors = extract_calls(
        object_constructor_profile, instructions, physical, FLASH_BASE
    )
    fixed_object_addresses = set()
    unresolved_object_constructors = []
    for call in object_constructors:
        object_address = call["arguments"]["object"]
        if object_address is None:
            unresolved_object_constructors.append(call["callsite"])
        elif FLASH_BASE <= object_address < FLASH_BASE + len(physical):
            fixed_object_addresses.add(object_address)
        else:
            raise ValueError(
                "NSE-3 object constructor input outside fixed ROM or runtime: "
                f"{object_address:#x}"
            )
    if (
        len(object_constructors) != 45
        or len(fixed_object_addresses) != 35
        or unresolved_object_constructors
            != DSP_PARAMETER_UNRESOLVED_OBJECT_CONSTRUCTORS
    ):
        raise ValueError(
            "NSE-3 object constructor census changed: expected 45 calls, "
            "35 unique fixed objects and one unresolved call; got "
            f"{len(object_constructors)}, {len(fixed_object_addresses)}, "
            f"{unresolved_object_constructors}"
        )

    fixed_object_events = []
    fixed_object_values = []
    fixed_object_rom_values = []
    emitter_eligible_records = 0
    emitter_eligible_rom_values = 0
    dispatch_bit_eligible_records = 0
    for object_address in sorted(fixed_object_addresses):
        object_offset = object_address - FLASH_BASE
        records_raw = int.from_bytes(
            physical[object_offset : object_offset + 4], "little"
        )
        records = (
            (records_raw << 16) | (records_raw >> 16)
        ) & 0xFFFFFFFF
        # Byte-addressed flash lanes cross inside each halfword in this image.
        record_count = physical[(object_offset + 4) ^ 1]
        records_end = records + record_count * 0x18
        if not (
            FLASH_BASE <= records
            and records_end <= FLASH_BASE + len(physical)
        ):
            raise ValueError(
                f"NSE-3 fixed object record range is invalid: "
                f"{object_address:#x} -> {records:#x} count {record_count}"
            )
        object_flags = physical[(object_offset + 6) ^ 1]
        for record in range(record_count):
            record_offset = records - FLASH_BASE + record * 0x18
            event_offset = record_offset + 0x0E
            fixed_object_events.append(
                int.from_bytes(
                    physical[event_offset : event_offset + 2], "little"
                )
            )
            value_raw = int.from_bytes(
                physical[record_offset + 8 : record_offset + 12], "little"
            )
            value = (
                (value_raw << 16) | (value_raw >> 16)
            ) & 0xFFFFFFFF
            fixed_object_values.append(value)
            value_is_rom = (
                FLASH_BASE <= value < FLASH_BASE + len(physical)
            )
            if value_is_rom:
                fixed_object_rom_values.append(value)
            record_flags = physical[(record_offset + 0x14) ^ 1]
            emitter_eligible = bool(
                (record_flags & 0x20) or (object_flags & 0x10)
            )
            if emitter_eligible:
                emitter_eligible_records += 1
                if value_is_rom:
                    emitter_eligible_rom_values += 1
            if record_flags & 0x40:
                dispatch_bit_eligible_records += 1
    if len(fixed_object_events) != 124:
        raise ValueError(
            "NSE-3 fixed object event census changed: expected 124, got "
            f"{len(fixed_object_events)}"
        )
    if {value & 0x1FFF for value in fixed_object_events} & target_events:
        raise ValueError(
            "NSE-3 fixed object catalogue can publish a parameter event"
        )
    fixed_object_value_census = {
        "records": len(fixed_object_values),
        "unique_values": len(set(fixed_object_values)),
        "rom_catalogue_values": len(fixed_object_rom_values),
        "emitter_eligible_records": emitter_eligible_records,
        "emitter_eligible_rom_catalogue_values":
            emitter_eligible_rom_values,
        "dispatch_bit_eligible_records": dispatch_bit_eligible_records,
    }
    if fixed_object_value_census != DSP_PARAMETER_FIXED_OBJECT_VALUE_CENSUS:
        raise ValueError(
            "NSE-3 fixed object value census changed: expected "
            f"{DSP_PARAMETER_FIXED_OBJECT_VALUE_CENSUS}, got "
            f"{fixed_object_value_census}"
        )

    runtime_catalogue = DSP_PARAMETER_RUNTIME_OBJECT_CATALOGUE
    runtime_object_offset = runtime_catalogue["address"] - FLASH_BASE
    runtime_records_raw = int.from_bytes(
        physical[runtime_object_offset : runtime_object_offset + 4], "little"
    )
    runtime_records = (
        (runtime_records_raw << 16) | (runtime_records_raw >> 16)
    ) & 0xFFFFFFFF
    runtime_record_count = physical[(runtime_object_offset + 4) ^ 1]
    runtime_events = []
    runtime_values = []
    runtime_flags = []
    for record in range(runtime_record_count):
        record_offset = (
            runtime_records - FLASH_BASE + record * 0x18
        )
        runtime_events.append(
            int.from_bytes(
                physical[record_offset + 0x0E : record_offset + 0x10],
                "little",
            )
        )
        value_raw = int.from_bytes(
            physical[record_offset + 8 : record_offset + 12], "little"
        )
        runtime_values.append(
            ((value_raw << 16) | (value_raw >> 16)) & 0xFFFFFFFF
        )
        runtime_flags.append(physical[(record_offset + 0x14) ^ 1])
    recovered_runtime_catalogue = {
        "address": runtime_catalogue["address"],
        "records": runtime_records,
        "record_count": runtime_record_count,
        "events": runtime_events,
        "values": runtime_values,
        "record_flags": runtime_flags,
    }
    if recovered_runtime_catalogue != runtime_catalogue:
        raise ValueError(
            "NSE-3 event-fed object catalogue changed: expected "
            f"{runtime_catalogue}, got {recovered_runtime_catalogue}"
        )
    if {value & 0x1FFF for value in runtime_events} & target_events:
        raise ValueError(
            "NSE-3 event-fed object catalogue can publish a parameter event"
        )

    def effective_word(address: int) -> int:
        offset = address - FLASH_BASE
        raw = int.from_bytes(physical[offset : offset + 4], "little")
        return ((raw << 16) | (raw >> 16)) & 0xFFFFFFFF

    ppm_root = effective_word(DSP_PARAMETER_PPM_ROOT_POINTER_CELL)
    if (
        ppm_root != DSP_PARAMETER_PPM_ROOT
        or effective_word(ppm_root) != 0x50504D00
    ):
        raise ValueError(
            "NSE-3 PPM root changed: expected "
            f"{DSP_PARAMETER_PPM_ROOT:#x}, got {ppm_root:#x}"
        )
    ppm_nodes = []
    node = ppm_root + 0x2C
    for _ in range(32):
        length = effective_word(node + 4)
        tag = effective_word(node + 8)
        ppm_nodes.append(
            {"address": node, "length": length, "tag": tag}
        )
        if tag == 0x54455854:
            break
        if length == 0:
            raise ValueError("NSE-3 PPM top-level walk reached zero length")
        node += length
    if ppm_nodes != DSP_PARAMETER_PPM_TOP_LEVEL_NODES:
        raise ValueError(
            "NSE-3 PPM top-level nodes changed: expected "
            f"{DSP_PARAMETER_PPM_TOP_LEVEL_NODES}, got {ppm_nodes}"
        )

    ppm_text_children = []
    child = node + 0x14
    for _ in range(256):
        value = effective_word(child)
        length = effective_word(child + 4)
        ppm_text_children.append(
            {"address": child, "value": value, "length": length}
        )
        if value == 0:
            break
        if length == 0:
            raise ValueError("NSE-3 PPM TEXT walk reached zero length")
        child += (length + 3) & ~3
    if ppm_text_children != DSP_PARAMETER_PPM_TEXT_CHILDREN:
        raise ValueError(
            "NSE-3 PPM TEXT children changed: expected "
            f"{DSP_PARAMETER_PPM_TEXT_CHILDREN}, got {ppm_text_children}"
        )
    ppm_descriptor_values = [
        child["value"]
        for child in ppm_text_children
        if child["value"] not in (0, 0x33)
    ]
    if ppm_descriptor_values != DSP_PARAMETER_PPM_DESCRIPTOR_VALUES:
        raise ValueError(
            "NSE-3 PPM descriptor values changed: expected "
            f"{DSP_PARAMETER_PPM_DESCRIPTOR_VALUES}, got "
            f"{ppm_descriptor_values}"
        )
    if any(
        FLASH_BASE <= value < FLASH_BASE + len(physical)
        for value in ppm_descriptor_values
    ):
        raise ValueError(
            "NSE-3 PPM descriptor value can name a ROM catalogue"
        )

    allocator_profile = {
        "apis": [
            {
                "address": 0x260ABC,
                "name": "general_allocator",
                "kind": "call",
                "arguments": {"size": "r0"},
            }
        ]
    }
    allocator_calls = extract_calls(
        allocator_profile, instructions, physical, FLASH_BASE
    )
    resolved_allocator_calls = [
        call
        for call in allocator_calls
        if call["arguments"]["size"] is not None
    ]
    exact_28_byte_calls = [
        call["callsite"]
        for call in allocator_calls
        if call["arguments"]["size"] == 0x1C
    ]
    allocator_census = {
        "calls": len(allocator_calls),
        "resolved_sizes": len(resolved_allocator_calls),
        "runtime_sizes": len(allocator_calls) - len(resolved_allocator_calls),
        "exact_28_byte_calls": exact_28_byte_calls,
    }
    if allocator_census != DSP_PARAMETER_ALLOCATOR_CENSUS:
        raise ValueError(
            "NSE-3 allocator census changed: expected "
            f"{DSP_PARAMETER_ALLOCATOR_CENSUS}, got {allocator_census}"
        )

    eeprom_write_profile = {
        "apis": [
            {
                "address": 0x29CE12,
                "name": "serial_eeprom_write",
                "kind": "call",
                "arguments": {
                    "address": "r0",
                    "length": "r2",
                },
            }
        ]
    }
    eeprom_write_calls = extract_calls(
        eeprom_write_profile, instructions, physical, FLASH_BASE
    )
    resolved_eeprom_write_calls = [
        call
        for call in eeprom_write_calls
        if call["arguments"]["address"] is not None
    ]
    event_range_start = 0x52
    event_range_end = 0x54
    resolved_overlapping_eeprom_writes = [
        {
            "callsite": call["callsite"],
            "address": call["arguments"]["address"],
            "length": call["arguments"]["length"],
        }
        for call in resolved_eeprom_write_calls
        if call["arguments"]["length"] is not None
        and call["arguments"]["address"] < event_range_end
        and call["arguments"]["address"] + call["arguments"]["length"]
        > event_range_start
    ]
    eeprom_write_census = {
        "calls": len(eeprom_write_calls),
        "resolved_addresses": len(resolved_eeprom_write_calls),
        "runtime_addresses":
            len(eeprom_write_calls) - len(resolved_eeprom_write_calls),
        "event_byte_range": [event_range_start, event_range_end],
        "resolved_overlapping_calls": resolved_overlapping_eeprom_writes,
    }
    eeprom_field_writer_profile = {
        "apis": [
            {
                "address": 0x28ECEC,
                "name": "eeprom_field_writer",
                "kind": "call",
                "arguments": {},
            }
        ]
    }
    eeprom_field_writer_calls = extract_calls(
        eeprom_field_writer_profile, instructions, physical, FLASH_BASE
    )
    eeprom_write_census["field_writer_direct_calls"] = [
        call["callsite"] for call in eeprom_field_writer_calls
    ]
    if eeprom_write_census != DSP_PARAMETER_EEPROM_WRITE_CENSUS:
        raise ValueError(
            "NSE-3 EEPROM write census changed: expected "
            f"{DSP_PARAMETER_EEPROM_WRITE_CENSUS}, got "
            f"{eeprom_write_census}"
        )

    object_event_profile = {
        "apis": [
            {
                "address": 0x29E604,
                "name": "runtime_object_event_generate",
                "kind": "packed_event",
                "arguments": {
                    "packed_event": "r0",
                    "arg1": "r1",
                    "arg2": "r2",
                },
            }
        ]
    }
    object_event_calls = [
        call
        for call in extract_calls(
            object_event_profile, instructions, physical, FLASH_BASE
        )
        if (
            call["arguments"]["packed_event"] is not None
            and (
                call["arguments"]["packed_event"] & 0x1FFF
            ) == DSP_PARAMETER_RUNTIME_OBJECT_EVENT
        )
    ]
    object_event_producers = {
        call["callsite"]: call["arguments"]["argument_words"]
        for call in object_event_calls
    }
    if object_event_producers != DSP_PARAMETER_RUNTIME_OBJECT_EVENT_PRODUCERS:
        raise ValueError(
            "NSE-3 runtime object event producers changed: expected "
            f"{DSP_PARAMETER_RUNTIME_OBJECT_EVENT_PRODUCERS}, got "
            f"{object_event_producers}"
        )

    object_emitter_profile = {
        "apis": [
            {
                "address": DSP_PARAMETER_OBJECT_EMITTER_ADDRESS,
                "name": "runtime_object_emit",
                "kind": "call",
                "arguments": {"input": "r0"},
            }
        ]
    }
    object_emitter_calls = {
        call["callsite"]: call["arguments"]["input"]
        for call in extract_calls(
            object_emitter_profile, instructions, physical, FLASH_BASE
        )
    }
    if object_emitter_calls != DSP_PARAMETER_OBJECT_EMITTER_CALLS:
        raise ValueError(
            "NSE-3 runtime object emitter entries changed: expected "
            f"{DSP_PARAMETER_OBJECT_EMITTER_CALLS}, got "
            f"{object_emitter_calls}"
        )
    emitter_pointer = DSP_PARAMETER_OBJECT_EMITTER_ADDRESS | 1
    if physical.find(emitter_pointer.to_bytes(4, "little")) != -1:
        raise ValueError(
            "NSE-3 runtime object emitter acquired a stored Thumb pointer"
        )

    unresolved = DSP_PARAMETER_UNRESOLVED_EVENT_CALLS
    return {
        "event_apis": {
            "task5_render_post": 0x29E556,
            "generic_event_generate": 0x29E604,
        },
        "direct_calls": len(calls),
        "statically_resolved_calls": len(resolved),
        "resolved_parameter_event_producers": [],
        "runtime_built_calls": runtime_calls,
        "classified_runtime_built_calls": {
            "stored_event_field": [
                0x2524CE,
                0x252C3E,
                0x252E7C,
                0x252F76,
            ],
            "stored_completion_event": [0x253552],
            "fixed_local_values": [0x25A1F8],
            "byte_domain_mode_table": [0x2A3472],
        },
        "remaining_unresolved_calls": unresolved,
        "remaining_runtime_record_population": {
            "constructor": 0x25A4F0,
            "table": DSP_PARAMETER_RUNTIME_RECORD_TABLE_ADDRESS,
            "capacity": 80,
            "direct_calls": len(registrations),
            "fixed_rom_descriptors": fixed_rom_descriptors,
            "startup_sram_descriptors": fixed_sram_descriptors,
            "runtime_descriptors": runtime_registration_calls,
            "bounded_runtime_descriptor_events":
                DSP_PARAMETER_RUNTIME_DESCRIPTOR_EVENTS,
            "unresolved_runtime_descriptors":
                DSP_PARAMETER_UNRESOLVED_RUNTIME_DESCRIPTORS,
            "unresolved_runtime_descriptor_reason":
                DSP_PARAMETER_UNRESOLVED_RUNTIME_DESCRIPTOR_REASON,
            "fixed_descriptors_exclude_parameter_events": True,
            "bounded_runtime_descriptors_exclude_parameter_events": True,
            "startup_copy_records": copy_records,
            "table_zero_initialized": True,
            "object_records": {
                "group_table": DSP_PARAMETER_OBJECT_GROUP_TABLE_ADDRESS,
                "group_table_zero_initialized": True,
                "record_stride": 0x18,
                "event_offset": 0x0E,
                "constructor": 0x25B0CC,
                "constructor_calls": len(object_constructors),
                "unique_fixed_objects": len(fixed_object_addresses),
                "fixed_events": len(fixed_object_events),
                "fixed_objects_exclude_parameter_events": True,
                "runtime_object_cell":
                    DSP_PARAMETER_RUNTIME_OBJECT_CELL_ADDRESS,
                "runtime_object_cell_zero_initialized": True,
                "unresolved_constructors":
                    DSP_PARAMETER_UNRESOLVED_OBJECT_CONSTRUCTORS,
                "constructor_event": DSP_PARAMETER_RUNTIME_OBJECT_EVENT,
                "constructor_event_producers": object_event_producers,
                "constructor_event_bounded_arguments":
                    DSP_PARAMETER_RUNTIME_OBJECT_EVENT_BOUNDED_ARGUMENTS,
                "object_emitter_calls": object_emitter_calls,
                "object_emitter_has_stored_pointer": False,
                "fixed_object_value_census": fixed_object_value_census,
                "explicit_object_event_descriptors":
                    DSP_PARAMETER_EXPLICIT_OBJECT_EVENT_DESCRIPTORS,
                "runtime_object_installer":
                    DSP_PARAMETER_RUNTIME_OBJECT_INSTALLER,
                "runtime_object_catalogue": recovered_runtime_catalogue,
                "ppm_root": ppm_root,
                "ppm_top_level_nodes": ppm_nodes,
                "ppm_text_children": ppm_text_children,
                "ppm_descriptor_values": ppm_descriptor_values,
                "unresolved_runtime_value_calls":
                    DSP_PARAMETER_UNRESOLVED_RUNTIME_VALUE_CALLS,
                "allocator_census": allocator_census,
                "stale_event_reuse_owner":
                    DSP_PARAMETER_STALE_EVENT_REUSE_OWNER,
                "eeprom_write_census": eeprom_write_census,
            },
        },
        "runtime_built_calls_exclude_parameter_events": False,
        "producer_absence_proven": False,
        "next_evidence":
            "trace_256e2e_unwritten_payload_and_27c17c_runtime_object",
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
    decode_thumb_anchors(data, DSP_BOOTSTRAP_POST_ANCHORS)
    verify_thumb_literals(data, DSP_BOOTSTRAP_POST_LITERALS)
    decode_thumb_anchors(data, DSP_INTERNAL_SOFTWARE_ANCHORS)
    verify_thumb_literals(data, DSP_INTERNAL_SOFTWARE_LITERALS)
    decode_thumb_anchors(data, DSP_INTERNAL_SOFTWARE_OWNER_ANCHORS)
    physical = swap16(data)
    external_software_pointer = effective_u32(
        physical, DSP_EXTERNAL_SOFTWARE_POINTER_TABLE - FLASH_BASE)
    if external_software_pointer != DSP_EXTERNAL_SOFTWARE_STRING:
        raise ValueError(
            "NSE-3 DSP external-software pointer changed: expected "
            f"{DSP_EXTERNAL_SOFTWARE_STRING:#x}, got "
            f"{external_software_pointer!r}"
        )
    external_software_bytes = bytes(
        cpu_byte(physical, FLASH_BASE, external_software_pointer + index)
        for index in range(len(DSP_EXTERNAL_SOFTWARE_BYTES))
    )
    if external_software_bytes != DSP_EXTERNAL_SOFTWARE_BYTES:
        raise ValueError(
            "NSE-3 DSP external-software identity changed: expected "
            f"{DSP_EXTERNAL_SOFTWARE_BYTES!r}, got "
            f"{external_software_bytes!r}"
        )
    task_2_entry = effective_u32(
        physical, NSE3_TASK_2_ENTRY_POINTER - FLASH_BASE)
    if task_2_entry != NSE3_TASK_2_ENTRY:
        raise ValueError(
            "NSE-3 task-2 entry changed: expected "
            f"{NSE3_TASK_2_ENTRY:#x}, got {task_2_entry!r}"
        )
    instructions = decode_image(physical, FLASH_BASE)
    internal_software_handler_callers = [
        insn.address
        for insn in instructions
        if insn
        and insn.mnemonic in ("bl", "blx")
        and immediate_target(insn) == 0x237D60
    ]
    if internal_software_handler_callers != [0x23A63A]:
        raise ValueError(
            "NSE-3 DSP internal-software report-handler callers changed: "
            f"expected [0x23a63a], got "
            f"{[hex(address) for address in internal_software_handler_callers]}"
        )
    internal_software_setter_callers = [
        insn.address
        for insn in instructions
        if insn
        and insn.mnemonic in ("bl", "blx")
        and immediate_target(insn) == 0x28EAD2
    ]
    if internal_software_setter_callers != [0x237DCE]:
        raise ValueError(
            "NSE-3 DSP internal-software setter callers changed: expected "
            f"[0x237dce], got "
            f"{[hex(address) for address in internal_software_setter_callers]}"
        )
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
    post_callers = {
        target: [
            insn.address
            for insn in instructions
            if insn
            and insn.mnemonic in ("bl", "blx")
            and immediate_target(insn) == target
        ]
        for target in (0x297356, 0x28EF38, 0x260252)
    }
    expected_post_callers = {
        0x297356: [0x2973C8],
        0x28EF38: [0x2973FA],
        0x260252: [0x2973FE],
    }
    if post_callers != expected_post_callers:
        raise ValueError(
            "NSE-3 DSP post-transfer caller topology changed: expected "
            f"{expected_post_callers}, got {post_callers}"
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
    state_base_references = [
        insn.address
        for insn in instructions
        if insn
        and literal_value(insn, physical, FLASH_BASE) ==
            DSP_BOOTSTRAP_STATE_BASE
    ]
    if state_base_references != DSP_BOOTSTRAP_STATE_BASE_REFERENCES:
        raise ValueError(
            "NSE-3 DSP bootstrap state-object references changed: expected "
            f"{[hex(address) for address in DSP_BOOTSTRAP_STATE_BASE_REFERENCES]}, "
            f"got {[hex(address) for address in state_base_references]}"
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
            "shared_header": DSP_BOOTSTRAP_HEADER,
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
            "mad2_transfer_gate": {
                "byte_address": 0x20002,
                "bit": 0,
                "assert_before_staging": 0x285962,
                "release_after_result_capture": 0x2859F8,
                "physical_meaning": "not_established",
            },
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
                "state_object_base": DSP_BOOTSTRAP_STATE_BASE,
                "state_object_base_references": state_base_references,
                "direct_base_relative_accesses": {
                    "writes": [0x2859E8],
                    "reads": [],
                },
                "external_mcu_consumption": "none_direct",
                "value_constraint": "nonzero_required_for_bootstrap_return",
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
                    "string_address": external_software_pointer,
                    "raw": external_software_bytes.decode("ascii").rstrip("\x00"),
                    "revision": "25.3.531",
                    "date": "17-Dec-97",
                    "product": "NSE-3Nx",
                },
                "0x09_dsp_internal_software": {
                    "source": "runtime_ram",
                    "address": 0x10BCF0,
                    "startup_state": "empty_string",
                    "setter": 0x28EAD2,
                    "setter_direct_callers":
                        internal_software_setter_callers,
                    "inbound_handler": 0x237D60,
                    "inbound_handler_direct_callers":
                        internal_software_handler_callers,
                    "owner": {
                        "task": 2,
                        "task_table": NSE3_TASK_TABLE,
                        "record_size": NSE3_TASK_RECORD_SIZE,
                        "entry_pointer": NSE3_TASK_2_ENTRY_POINTER,
                        "entry": task_2_entry,
                        "object_family_offset": 3,
                        "object_family": 0x74,
                        "subcommand_offset": 8,
                        "excluded_subcommand": 0x32,
                    },
                    "transport": {
                        "physical_boundary": "dsp_to_mcu_shared_ring",
                        "materializer": 0x285794,
                        "queue_envelope": {
                            "source_offset": 0,
                            "source": 0x18,
                            "destination_offset": 1,
                            "destination_task": 2,
                            "length_offset": 2,
                            "family_offset": 3,
                        },
                        "ring_poll": 0x28580A,
                        "router_call": 0x2A2224,
                        "router_wrapper": 0x2A1F52,
                        "bootstrap_responder_is_source": False,
                        "external_service_peer_is_source": False,
                        "internal_dsp_producer_logic": "not_available",
                    },
                    "accepted_message_types": [0x0A, 0xC8],
                    "message_value_offset": 0x0B,
                    "rendered_value": "single_ascii_digit",
                    "acknowledgement": "none_object_released",
                    "exact_revision": "not_established",
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
        "post_transfer": {
            "wrapper": 0x2973C6,
            "pre_transfer_mad2_byte": 0x20001,
            "configuration_bit": 0,
            "bootstrap_return_value_tested": False,
            "captured_results_tested_before_continuation": False,
            "conditional_eeprom_sync": {
                "condition": "pre_transfer_mad2_0x20001_bit_0_clear",
                "helper": 0x28EF38,
                "word_address": 0x74,
                "requested_value": 0,
                "write_only_when_changed": True,
            },
            "unconditional_continuation": 0x260252,
            "direct_callers": post_callers,
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
        "sim_apdu_boundary": verify_sim_apdu_boundary(data),
        "dspif_transport_boundary": verify_dspif_boundary(data),
        "radio_packet_boundary": verify_radio_packet_boundary(data),
        "dsp_parameter_08_boundary": verify_dsp_parameter_08_boundary(data),
        "dsp_parameter_event_producers": verify_dsp_parameter_event_producers(
            data
        ),
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

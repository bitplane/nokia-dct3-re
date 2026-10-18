#!/usr/bin/env python3
"""Check bounded NSE-1 ROM4 codec-port sequences, not physical port semantics."""

import argparse
from pathlib import Path
import re

if __package__ in (None, ""):
    import sys
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.c54x_rom4_port_census import census


SEQUENCES = {
    "tone_phase_scale": (0xa5c7, (
        0x770e, 0x20c5, 0x7714, 0x0857, 0x2494, 0xf474, 0x80f8, 0x00fa)),
    "tone_phase_publication": (0xa668, (
        0x1184, 0x0193, 0xf030, 0xffff, 0x8194)),
    "rf_port3_window_set": (0x44a7, (
        0x74f8, 0x0008, 0x0003, 0xf040, 0x0005, 0xf495, 0x75f8, 0x0008, 0x0003)),
    "rf_port3_window_clear": (0x44b8, (
        0x74f8, 0x0008, 0x0003, 0xf030, 0xfffe, 0xf495, 0x75f8, 0x0008, 0x0003)),
    "rf_port3_initialization": (0x4567, (
        0x7682, 0x0000, 0xf495, 0x7582, 0x0003)),
    "host_parameter_interrupt_gate": (0x3772, (
        0x69f8, 0x0866, 0x0002, 0x4a08, 0x4a09, 0x4a0a,
        0x4a0b, 0x4a0c, 0x4a0d, 0x12f8, 0x0870, 0xf845, 0x37b2,
        0x10f8, 0x00ef, 0xf010, 0x0004, 0xf842, 0x37b2)),
    "host_parameter_consumer_and_completion": (0x3796, (
        0xf074, 0xa51b, 0x69f8, 0x0029, 0x0008,
        0x68f8, 0x0029, 0xfff7, 0x76f8, 0x0870, 0x0000)),
    "host_command_dispatch": (0x35c0, (
        0xf130, 0x1000, 0xf84c, 0x369a, 0xf130, 0x001c, 0xf84c, 0x362f,
        0x61f8, 0x06f3, 0x0400, 0xf830, 0x35d7,
        0xf130, 0x0100, 0xf84c, 0x8ec5, 0xf130, 0x0200, 0xf84c, 0x8ed3,
        0xf073, 0x35df, 0xf130, 0x0100, 0xf84c, 0xb7dc,
        0xf130, 0x0200, 0xf84c, 0xb787, 0xf130, 0x0400, 0xf84c, 0x8e89,
        0xf130, 0x0080, 0xf84c, 0x371d, 0xf130, 0x0002, 0xf84c, 0x3621,
        0xf130, 0x0001, 0xf84c, 0x362d, 0xf130, 0x0020, 0xf84c, 0x3764,
        0xf130, 0x0040, 0xf84c, 0x3764)),
    "rf_timer_table_generation": (0xa1a6, (
        0x7715, 0xa275, 0x7712, 0x1914, 0xe905, 0x771a, 0x0004,
        0xf072, 0xa1b3, 0x4815, 0x6d95, 0x8192, 0x7e92, 0xe901,
        0x8192, 0x7692, 0x3a98)),
    "rf_programming_table_pair": (0x36f7, (
        0x7593, 0x0031, 0xf495, 0xf495, 0x7593, 0x0032)),
    "rf_programming_uploaded_pair": (0xa22f, (
        0xf074,0xa228,0x76f8,0x1f30,0x0000,0x7212,0x197b,0x7215,
        0x197d,0x6d92,0xf495,0x7595,0x0031,0xf495,0xf495,0x7595,
        0x0032,0xf495,0xf495,0x7592,0x0004,0xec05,0xf495,0x7315,
        0x197d,0x7312,0x197b,0xfc00)),
    "compare_bank_disabled_positions": (0x4505, (
        0x7682,0x3a98,0xf495,0x7582,0x000f,0xec0e,0xf495,0xf495,
        0x7582,0x0004,0xec0e,0xf495,0xf495,0x7582,0x0009,0xec0e,
        0xf495,0xf495,0x7582,0x0005,0xec0e,0xf495,0xf495,0x7582,
        0x0007,0xec0e,0xf495,0xf495,0x7582,0x000a)),
    "queued_transfer_request_ack": (0x369a, (
        0xf074,0x2418,0x7682,0x1000,0xf495,0x7582,0x0001,0xec01,
        0xf495,0x7712,0x195c)),
    "serial_prepare_hook_defaults": (0x23e4, (
        0xfc00,0xf495,0xfc00,0xf495)),
    "serial_prepare_state": (0x9009, (
        0xf074,0x23e4,0xf7bb,0xf020,0x0100,0x7712,0x06bf,0x8092,
        0x8092,0xf020,0x0700,0x8092,0x8082,0x7712,0x06b2,0xf071,
        0x0004,0x8092,0x7712,0x06c3,0xec03,0x8092,0x7712,0x06f3,
        0x6882,0x0fff,0x6182,0x0200,0xf830,0x903d,0x6182,0x0100,
        0xf830,0x903d,0x74f8,0x000b,0x0036,0xf330,0x0080,0xe800,
        0x6182,0x0002,0xfd4d,0xe802,0x80f8,0x06b4,0xe801,0xfd30,
        0xe800,0xfe00,0x80f8,0x06c6,0x76f8,0x06b4,0x0006,0xe803,
        0xfe00,0x80f8,0x06c6)),
    "serial_restart_request_and_poll": (0x9129, (
        0x7482,0x0036,0x6882,0xffe1,0x6982,0x801c,0xf495,0x7582,
        0x0036,0xf495,0x7482,0x0000,0x6982,0x0010,0xf495,0x7582,
        0x0000,0xf074,0x9009,0x7712,0x0060,0x7482,0x0036,0x6182,
        0x4000,0xf820,0x913e,0x6882,0x7fff,0xf495,0x7582,0x0036)),
    "rf_port30_scan_control": (0x0a90, (
        0xec0d, 0xf495, 0xe800, 0xf495, 0x7583, 0x0030)),
    "rf_port30_resident_control": (0x30b3, (
        0xe801, 0xf495, 0x75f8, 0x0008, 0x0030)),
    "rf_port30_initialization": (0x4562, (
        0x7682, 0x0001, 0xf495, 0x7582, 0x0030)),
    "rf_port30_transition_control": (0x6974, (
        0xe901, 0xf495, 0x75f8, 0x000b, 0x0030)),
    "rf_port30_mode_control": (0xa17a, (
        0xe900, 0xf495, 0x75f8, 0x000b, 0x0030)),
    "rf_programming_overlay_upload_loop": (0x31d4, (
        0x47f8, 0x0013, 0x7f92, 0x6d93, 0x00f8, 0x0013)),
    "rf_programming_addend_and_transform_selection": (0x3ffb, (
        0xf074, 0x244a, 0x61f8, 0x06be, 0x0080, 0xf830, 0x4055,
        0x11f8, 0x216b, 0xf073, 0x400f, 0xf074, 0x244c,
        0x61f8, 0x06be, 0x0080, 0xf830, 0x4060, 0x11f8, 0x216c,
        0xf600, 0x61f8, 0x216a, 0x0002, 0xf930, 0x4049,
        0x61f8, 0x216a, 0x0004, 0xf930, 0x402e)),
    "rf_programming_scan_caller": (0x0a84, (
        0x1084, 0xf274, 0x3ffb, 0x7712, 0x0031)),
    "rf_programming_resident_hook_defaults": (0x244a, (
        0xfc00, 0xf495, 0xfc00, 0xf495)),
    "rf_programming_port_or_memory_destination": (0x401a, (
        0x60f8, 0x0012, 0x0031, 0xf820, 0x402a, 0xf495,
        0x75f8, 0x0008, 0x0031, 0xf495, 0xf495,
        0x75f8, 0x0009, 0x0032, 0x6dea, 0x0002,
        0xff20, 0x8092, 0x8292, 0xfc00)),
    "rf_programming_packed_accumulator": (0x403a, (
        0xf57a, 0x81f8, 0x0060, 0xf3e6, 0x0af8, 0x000b,
        0x11f8, 0x0060, 0xf767, 0xf1a0, 0x69f8, 0x000c,
        0x0030, 0xf640, 0xfc00)),
    "frame_entry_port21_set": (0x321e, (
        0x74f8, 0x0008, 0x0021,  # PORTR 21, AL
        0xf040, 0x0c00,          # OR #0c00, A
        0x7212, 0x00aa,          # MVDM aa, AR2
        0x75f8, 0x0008, 0x0021,  # PORTW AL, 21
    )),
    "frame_exit_port21_clear": (0x33f3, (
        0x74f8, 0x0008, 0x0021,  # PORTR 21, AL
        0xf030, 0xf7ff,          # AND #f7ff, A
        0x8a19, 0x8a1c,          # Restore saved registers; not port writes.
        0x75f8, 0x0008, 0x0021,  # PORTW AL, 21
    )),
    "port21_initialization": (0x454e, (
        0x7692, 0x0482,          # ST #0482, *AR2+
        0x7682, 0x1482,          # ST #1482, *AR2
        0xf495, 0x7582, 0x0021,  # NOP; PORTW *AR2, 21
        0xf495, 0xf495,
        0x758a, 0x0021,          # PORTW *AR2-, 21
        0xf495, 0xf495,
        0x7582, 0x0021,          # PORTW *AR2, 21
    )),
}

UPLOADED_COUNTER_BLOCKS = {
    "counter_reset": (0x0ac2, (0xe900, 0x81f8, 0x198d, 0x81f8,
                              0x1256, 0x81f8, 0x1344)),
    "result_limit": (0x0b00, (0x10f8, 0x1344, 0xf010, 0x01f4, 0xf842, 0x0ca3)),
    "candidate_counter_guard": (0x0b06, (0xe801, 0x0084, 0x8094,
                                         0xf010, 0x05f3, 0xf843, 0x0acf)),
}


def check_mcu_upload_counters(image):
    locations = {}
    bases = set()
    for name, (address, words) in UPLOADED_COUNTER_BLOCKS.items():
        pattern = b"".join(word.to_bytes(2, "big") for word in words)
        start = image.find(pattern)
        if start < 0 or start % 2 or image.find(pattern, start + 1) >= 0:
            raise ValueError(f"missing, unaligned or ambiguous MCU upload block: {name}")
        locations[name] = start
        bases.add(start - 2 * address)
    if len(bases) != 1:
        raise ValueError("MCU upload counter blocks do not share the captured program layout")
    return locations

# Each additional immediate port reader preserves/modifies specific bits and
# writes the result back. These are ROM observations, not register field names.
MASK_SEQUENCES = {
    "port21_3c6f": (0x3c6f, (0x74f8, 0x000b, 0x0021, 0xf340, 0x0005,
                            0xf330, 0xfffd, 0xf495, 0x75f8, 0x000b, 0x0021)),
    "port21_3c84": (0x3c84, (0x74f8, 0x000b, 0x0021, 0xf330, 0xfffb,
                            0xf340, 0x0003, 0xf495, 0x75f8, 0x000b, 0x0021)),
    "port21_4231": (0x4231, (0x74f8, 0x000b, 0x0021, 0xf340, 0x0a00,
                            0xf330, 0xfbff, 0xf495, 0x75f8, 0x000b, 0x0021)),
    "port21_4275": (0x4275, (0x74f8, 0x0008, 0x0021, 0xf030, 0xf7ff,
                            0xf040, 0x0600, 0xf495, 0x75f8, 0x0008, 0x0021)),
    "port21_43c2": (0x43c2, (0x74f8, 0x0008, 0x0021, 0xf040, 0x0140,
                            0xf030, 0xff7f, 0xf495, 0x75f8, 0x0008, 0x0021)),
    "port21_43ef": (0x43ef, (0x74f8, 0x0008, 0x0021, 0xf030, 0xfeff,
                            0xf040, 0x00c0, 0xf495, 0x75f8, 0x0008, 0x0021)),
    "port21_4473": (0x4473, (0x74f8, 0x0008, 0x0021, 0xf040, 0x0200,
                            0xf495, 0x75f8, 0x0008, 0x0021)),
}
SEQUENCES.update(MASK_SEQUENCES)
SEQUENCES.update({
    "receive_scan_filter_setup": (0x32f4, (
        0xf074, 0x23fe, 0x4a60, 0x4a61, 0x7712, 0x1a48,
        0xe723, 0x7719, 0x0010, 0xed00, 0x7715, 0x21a9,
        0x7710, 0x0002, 0x7716, 0x0080, 0x771a, 0x0007,
        0xf272, 0x3325, 0x7714, 0x0084)),
    "receive_holdoff_before_mode_dispatch": (0x3228, (
        0x7713,0x06ce,0x6c8a,0x32c5,0x7712,0x00ac,0x6082,0x0001,
        0xf830,0x32f4,0x6082,0x0002,0xf830,0x33b4)),
    "receive_holdoff_history_and_commit": (0x32c5, (
        0x7719,0x0010,0x771a,0x0007,0xf272,0x32d8,0x7714,0x0080,
        0x74d4,0x0027,0xf495,0x74d4,0x0027,0xf495,0x74d4,0x0027,
        0xf495,0x74d4,0x0027,0xf495,0x6e82,0x33f3,0x7312,0x00aa)),
    "receive_scan_filter_word_consumption": (0x330a, (
        0x74d6, 0x0027, 0xa43a, 0xa5be, 0xb336, 0xb0be,
        0x74d6, 0x0027, 0xb03a, 0xb3be, 0xb336, 0xb0be,
        0x74d6, 0x0027, 0xb03a, 0xb3be, 0xb336, 0xb0be,
        0x74d6, 0x0027, 0xb03a, 0xb3be, 0xb736, 0xd6e9,
        0xe4e9, 0x6ddc, 0x6ded, 0xfff9)),
    "receive_schedule_control_return": (0x995a, (
        0x11f8, 0x194a, 0xf230, 0x0001, 0xfc45,
        0x7713, 0x195b, 0x6983, 0x0102, 0x6883, 0xfff7,
        0xe800, 0x80f8, 0x194f, 0x80f8, 0x1951, 0xfc00)),
    "receive_acquisition_caller_bit15": (0x996b, (
        0xf074, 0x2472, 0xf230, 0x8000, 0xfe45,
        0x7712, 0x195b, 0x1082, 0xf030, 0x0800,
        0xf274, 0x4677, 0x6882, 0xf3ff)),
    "receive_acquisition_caller_bit14": (0x9988, (
        0xf074, 0x2472, 0xf330, 0x4000,
        0x7712, 0x195b, 0x1082, 0xf030, 0x0c00,
        0xf84d, 0x99a3, 0xf030, 0x0800,
        0xf274, 0x4677, 0x6982, 0x0800)),
    "receive_operational_flag_result_gate": (0x46e1, (
        0x10f8, 0x1956, 0xfe43, 0x7712, 0x198f,
        0x10f8, 0x198c, 0xfe47, 0x11f8, 0x1835,
        0x1092, 0xf465, 0xf330, 0x00f0, 0xf310, 0x0020,
        0x80f8, 0x1933, 0xf84c, 0x46f8, 0x5682,
        0x4ef8, 0x177e, 0x10f8, 0x1f24, 0xf010, 0x01f4,
        0xfc42, 0x69f8, 0x1949, 0x0080, 0xfc00)),
    "receive_operational_loop_selection": (0x313f, (
        0x61f8, 0x1949, 0x0080, 0xf830, 0x3153)),
    "receive_scan_gateway": (0x2420, (0xf073, 0x0a5c)),
    "receive_search_cursor_entry": (0x311a, (
        0x7214, 0x1983, 0x7713, 0x0008, 0x7184, 0x1867)),
    "receive_search_bitmap_scratch_copy": (0x3d97, (
        0x6f82, 0x0c58, 0x80f8, 0x03fd, 0x10da, 0xf030, 0x00ff,
        0x7714, 0x1200, 0x8094, 0xec1e, 0xe5ca)),
    "receive_search_bitmap_expansion": (0x48f7, (
        0xf074, 0x2428, 0x4a17, 0x7717, 0x0007, 0xe900,
        0xf274, 0x4909, 0x890e, 0x8910, 0x7717, 0x0017,
        0xf274, 0x4909, 0x7710, 0x01ff, 0x8a17, 0xfc00,
        0x771a, 0x000f, 0x1282, 0xf072, 0x4915, 0xf490,
        0xf495, 0x6d90, 0xff0c, 0x7093, 0x0010,
        0x07f8, 0x000e, 0x6e8f, 0x4909, 0x6d8a, 0xf495, 0xfc00)),
    "receive_search_entry_fallback_publication": (0x3144, (
        0x7713, 0x1200, 0x10f8, 0x1867, 0x8083,
        0x7715, 0x0002, 0x7713, 0x1200, 0xf274, 0x37ce,
        0x7714, 0x008a, 0xf073, 0x30f5)),
    "receive_scan_lifecycle_call": (0x3129, (
        0x76f8, 0x06fd, 0x0000, 0xf274, 0x3198,
        0x7712, 0x0003, 0xf074, 0x2420, 0x76f8,
        0x06fd, 0x0001)),
    "receive_operational_loop_initialization": (0x315c, (
        0x76f8, 0x1949, 0x0008, 0x76f8, 0x17a8, 0x0004,
        0x76f8, 0x1868, 0x0030, 0x76f8, 0x1869, 0x0040,
        0x76f8, 0x07fc, 0x0000)),
    "receive_operational_loop_control": (0x316b, (
        0xf074, 0x390a, 0x7710, 0x06e3, 0x6180, 0x0001,
        0xf830, 0x3000, 0x6180, 0x0002, 0xf820, 0x318a)),
    "receive_operational_loop_configuration_call": (0x318a, (
        0xf074, 0x5e62, 0xf073, 0x316b)),
    "receive_configuration_initialization_chain": (0x545d, (
        0xf274, 0x481a, 0x7712, 0x0912, 0xf074, 0x5a34,
        0xf074, 0x5514, 0xf074, 0x5929, 0xf074, 0x5475)),
    "receive_callback_indirect_dispatch": (0x5eed, (
        0x10f8, 0x07fb, 0xf4e3)),
    "receive_configuration_entry_gate": (0x5514, (
        0xf074, 0x23d0, 0x61f8, 0x1949, 0x0008, 0xf820, 0x58a7)),
    "receive_scheduler_selection_active_mode": (0x55b3, (
        0xe9f0, 0x19f8, 0x1836, 0x60f8, 0x000b, 0x00c0,
        0xf830, 0x55be, 0x76f8, 0x07fb, 0x98b7)),
    "receive_scheduler_selection_channel": (0x577d, (
        0x10f8, 0x1835, 0xf130, 0x00f0, 0xf210, 0x0020,
        0x76f8, 0x07fb, 0x98b7)),
    "receive_scheduler_selection_overrides": (0x578a, (
        0x60f8, 0x1835, 0x002f, 0xf820, 0x5792,
        0x76f8, 0x07fb, 0xa27d, 0x10f8, 0x1835,
        0xf030, 0x00fe, 0x60f8, 0x0008, 0x0020,
        0xf820, 0x579e, 0x76f8, 0x07fb, 0xa2cf)),
    "receive_schedule_index_dispatch": (0x98b7, (
        0xf074, 0x24ac, 0xe900, 0x10f8, 0x07fc,
        0xf000, 0x98c1, 0x7ef8, 0x000b, 0xf5e2,
        0x98c6, 0x98cb, 0x98d0, 0x98d5, 0x98f6)),
    "receive_schedule_indirect_call_list": (0x9903, (
        0x1095, 0x4a15, 0xf4e3, 0x8a15, 0xf495,
        0x6085, 0x00ff, 0xf820, 0x9903, 0xfc00)),
    "receive_filtered_sample_pointer_setup": (0x33b6, (
        0x7213, 0x00b1, 0x7719, 0x0010, 0x7715, 0x21a9,
        0x7710, 0x0002, 0xed00, 0x7714, 0x0084,
        0x7716, 0x0080, 0x771a, 0x0007)),
    "receive_filtered_sample_input": (0x33c7, (
        0x7712, 0x06d2, 0x74d6, 0x0027)),
    "receive_filtered_sample_pointer_commit": (0x33e3, (
        0x4092, 0xe4e9, 0x438a, 0xe6e9, 0xf074, 0x2406,
        0x7313, 0x00b1)),
    "receive_sample_pointer_configuration": (0x4420, (
        0xf130, 0x0002, 0xf84d, 0x4426, 0x7313, 0x00b1,
        0x7312, 0x00b0)),
    "receive_secondary_operation_writer": (0x9a99, (
        0xf074, 0x4c6f, 0x80f8, 0x1950)),
    "receive_primary_operation_writer": (0x9aca, (
        0x10f8, 0x195b, 0xf274, 0x4c6f, 0xf030, 0x0100,
        0x68f8, 0x195b, 0xfeff, 0x7712, 0x194f, 0x8082)),
    "receive_additional_primary_operation_writer": (0x9b15, (
        0xf074, 0x2476, 0x11f8, 0x194c, 0xfc4d,
        0x7713, 0x0000, 0x7714, 0xffff, 0x10f8, 0x195b,
        0xf274, 0x4c6f, 0xf030, 0x0100,
        0x68f8, 0x195b, 0xfeff, 0x80f8, 0x194f)),
    "receive_sample_progress_wait": (0x4c8b, (
        0xe87c, 0x4915, 0xf600, 0xf7bb, 0x6ff8, 0x00b1,
        0x0d20, 0xf84f, 0x4c98, 0xf6bb, 0xf4e1,
        0xf073, 0x4c8e)),
    "receive_final_sample_progress_wait": (0x4ca3, (
        0xf7bb, 0x6ff8, 0x00b1, 0x0e20, 0xf847, 0x4cad,
        0xf6bb, 0xf4e1, 0xf073, 0x4ca3)),
    "receive_decode_operation_entry": (0x4ef7, (
        0xf074, 0x23d0, 0x80f8, 0x121f, 0x81f8, 0x121e,
        0xf110, 0x0040, 0xf84c, 0x4f26)),
    "receive_short_block_decode_result": (0x4f18, (
        0xf074, 0x5441, 0xf84c, 0x50fd)),
    "receive_decode_primary_operation": (0x9b40, (
        0x10f8, 0x194f, 0xfc45, 0xe900, 0xf074, 0x4ef7,
        0x76f8, 0x194f, 0x0000)),
    "receive_decode_secondary_operation": (0x9be2, (
        0x10f8, 0x1950, 0xfc45, 0xe901, 0xf074, 0x4ef7,
        0xe900, 0x81f8, 0x1950)),
    "receive_decode_deferred_operation": (0x9dfd, (
        0x10f8, 0x11fd, 0xfc45, 0x60f8, 0x1836, 0x00c0,
        0xf820, 0x9e09, 0x60f8, 0x11ec, 0x0000,
        0xfc20, 0xe900, 0xf074, 0x4ef7)),
    "receive_ring_publisher_header": (0x37ec, (
        0x7719, 0x0064, 0x4815, 0x4914, 0xf508,
        0xf000, 0x0001, 0xf0ff, 0xf010, 0x0001, 0x81d2)),
    "receive_ring_publisher_commit": (0x37ff, (
        0x4912, 0xf310, 0x0800, 0xff20, 0x730b, 0x08e4,
        0xe801, 0xf495, 0x75f8, 0x0008, 0x0001)),
    "type80_short_block_publication": (0x4f1c, (
        0x7715, 0x000e, 0x7714, 0x0080, 0xf274, 0x37ce,
        0x7713, 0x125b)),
    "type80_long_block_publication": (0x50d5, (
        0x7715, 0x0021, 0x4bf8, 0x121f, 0x7713, 0x125b,
        0xf274, 0x37ce, 0x7714, 0x0080)),
    "host_ring_header_consumption": (0x3877, (
        0x7719, 0x0052, 0x81f8, 0x1987, 0x13d2,
        0x61f8, 0x06e1, 0x0008, 0xf230, 0x00ff,
        0xf3f8, 0xfc20)),
    "host_type2_payload_start": (0x3a09, (
        0x69f8, 0x06e3, 0x0080, 0x12d2, 0x6ff8,
        0x17a8, 0x0c98, 0xf030, 0x00ff, 0x7715,
        0x1863, 0x8095)),
    "host_type2_pending_mode_decode": (0x3a21, (
        0x7715, 0x1868, 0xf030, 0x00ff, 0x8095,
        0x12d2, 0x6f95, 0x0c98, 0xf030, 0x00ff, 0x8095)),
    "pending_to_active_mode_copy": (0x570f, (
        0x7712, 0x1863, 0x7713, 0x1830, 0xec07, 0xe589)),
    "rf_sample_queue_producer": (0x3357, (
        0x7719, 0x0048, 0x7213, 0x06b7, 0xf495,
        0x82d3, 0x83d3, 0x7313, 0x06b7)),
    "receive_reduction_selector": (0x3326, (
        0x60f8, 0x06fd, 0x0001, 0xf830, 0x3347)),
    "receive_control_comparison_and_call": (0x3376, (
        0x7713,0x06bc,0x6183,0x0002,0xfa30,0x33b0,0x6883,0xfffd,
        0x6983,0x0002,0x7713,0x0060,0x56f8,0x2196,0xf620,0xfa43,
        0x33a1,0x7712,0x00ad,0x7083,0x2180,0x61f8,0x06be,0x0080,
        0xf820,0x3397,0x61f8,0x195d,0x0004,0xf830,0x3397,0x7083,
        0x21c2,0xa201,0xf845,0x33b0,0xf062,0x0006,0x3c82,0xf273,
        0x33ac,0x4583,0xf487,0x56f8,0x2194,0xf620,0xf846,0x33b0,
        0xf062,0x0003,0x4082,0xf484,0xf843,0x33b0,0xf274,0x42f6,
        0x82f8,0x00ad)),
    "rf_sample_queue_empty_wait": (0x4798, (
        0x7719, 0x0048, 0x7711, 0x06b8, 0x10f8, 0x06b7,
        0x0881, 0xfa45, 0x479e, 0x10f8, 0x06b7,
        0x7214, 0x06b8)),
    "rf_adjacent_pair_products": (0x47e6, (
        0x7712, 0x1254, 0x7713, 0x1255, 0x308a, 0x218a,
        0x2082, 0x3083, 0xfa30, 0x47f3, 0x2d92, 0x2882,
        0xfc00, 0xfe00, 0xe554, 0xe510)),
    "rf_selection_helper_trampoline": (0x2523, (0xf073, 0x25fe)),
    "rf_selection_helper_status_paths": (0x25fe, (
        0x60f8, 0x000b, 0x0001, 0xf820, 0x2606, 0xf7bd,
        0xf6bd, 0xfc00, 0x60f8, 0x000b, 0x0002, 0xf820,
        0x260e, 0xf6bd, 0xf7bd, 0xfc00, 0x61f8, 0x0007,
        0x2000, 0xf820, 0x2615, 0xf6bd, 0xfc00)),
    "rf_result_normalization": (0x4778, (
        0xf48e, 0x7714, 0x0060, 0xf48f, 0xf477, 0xf48d,
        0x8084, 0x7719, 0x0000, 0xff45, 0x6b84, 0x0001,
        0xf78f, 0xfa30, 0x478a, 0xf677, 0xf58d, 0xf762,
        0x5685, 0xf48f, 0xfa30, 0x4791, 0xf477, 0xf48d,
        0xf462, 0xf600, 0xf468, 0xec0e, 0x1e84, 0xfe00,
        0x8084, 0x1084)),
    "rf_queue_copy_from_request": (0xa1b7, (
        0x7712, 0x1920, 0xec09, 0x7092, 0x216d)),
    "rf_queued_transfer_consumption": (0x3706, (
        0x7212, 0x197b, 0x7213, 0x197d, 0x6d92, 0xf495,
        0x7593, 0x0031, 0xf495, 0xf495, 0x7593, 0x0032,
        0x7313, 0x197d, 0xf495, 0x7592, 0x0004, 0xec03, 0xf495,
        0xf273, 0x36a9, 0x7312, 0x197b)),
    "fr_encoder_wrapper_gate": (0x9c4f, (
        0xf074, 0x247c, 0x60f8, 0x1836, 0x00c1, 0xfc20,
        0x61f8, 0x06bb, 0x0003, 0xf830, 0x9c9f)),
    "fr_encoder_pcm_and_codec": (0x9c91, (
        0xf274, 0xa0b8, 0x7712, 0x1536,
        0xf274, 0x85e0, 0x7714, 0x1536, 0xf074, 0x860c)),
    "fr_encoder_conditional_pcm_mute": (0xa0b8, (
        0xf074, 0x24c1, 0x61f8, 0x1f2b, 0x0001,
        0xfc20, 0xf071, 0x009f, 0x8092, 0xfc00)),
    "fr_encoder_mute_resident_hook": (0x24c1, (0xfc00, 0xf495)),
    "speech_control_shared_flag_copy": (0xa539, (
        0xf074, 0x24cf, 0x7714, 0x085e, 0x7194, 0x1f2b,
        0x1284, 0xf130, 0x00ff)),
    "speech_control_shared_flag_copy_duplicate": (0x27aa, (
        0x7714, 0x085e, 0x7194, 0x1f2b, 0x1284, 0xf130, 0x00ff)),
    "speech_control_flag_mask_set": (0xb885, (0x69f8, 0x1f2b, 0x1ca0)),
    "speech_control_flag_mask_clear": (0xb891, (0x68f8, 0x1f2b, 0xe35f)),
    "fr_decoder_wrapper_gate": (0x9cb9, (
        0xf074, 0x247c, 0x60f8, 0x1836, 0x00c1, 0xfc20)),
    "fr_decoder_pcm_destination": (0x9ce2, (
        0xf274, 0x8458, 0x7712, 0x1496, 0xf074, 0x94c9,
        0xf274, 0xab7c, 0x7712, 0x1496)),
    "fr_mode_publication": (0x579e, (
        0x60f8, 0x1869, 0x00c3, 0xf820, 0x57a9,
        0x76f8, 0x07fb, 0xa436, 0x76f8, 0x1836, 0x00c1)),
    "fr_pcm_ingress_pointer": (0xaa42, (
        0x7212, 0x167c, 0x7215, 0x06de, 0xf272, 0xaa4b,
        0x7710, 0x0001, 0xe538, 0x9ef5, 0xfe00, 0x7315, 0x06de)),
    "fr_pcm_egress_pointer": (0xaa5a, (
        0x7719, 0x0278, 0x7213, 0x06df, 0x771a, 0x009f,
        0xf272, 0xaa64, 0x7710, 0x0001, 0xe58d, 0xfe00,
        0x7313, 0x06df)),
    "resident_vector_page_setup": (0xff80, (0x771d, 0xffa8, 0xf073, 0xff85)),
    "serial_vector20_entry": (0xffd0, (0xf273, 0x3416, 0x4a06, 0x4a07)),
    "serial_direct_copy_branch": (0x3428, (
        0x61f8, 0x06be, 0x2000, 0xf820, 0x3433,
        0x10f8, 0x0020, 0x80f8, 0x0021, 0xf073, 0x358b)),
    "serial_accumulator_publication": (0x3589, (0xf47d, 0x8821, 0x8a19)),
    "tone_enable_cell_gate": (0xa598, (0x10f8, 0x0856, 0xf030, 0x0001,
                                      0xf845, 0xa5e2)),
    "codec_release_bsp": (0xa4a5, (0x7722, 0xc008, 0x7722, 0xc0c8)),
    "codec_demand_acquire_commit": (0xa4bb, (
        0x8a0a, 0x8a09, 0x8a08, 0x8a0d, 0x8a0c, 0x8a0b,
        0xf1a0, 0x81f8, 0x00ef)),
    "codec_demand_release_commit": (0xa4d9, (
        0x10f8, 0x00ef, 0xf350, 0xffff, 0xf280, 0x80f8, 0x00ef)),
    "codec_register8_activation": (0xa4b4, (
        0xe908, 0xf074, 0x4610, 0xf040, 0x0600, 0xf074, 0x45c2)),
    "codec_register8_deactivation": (0xa502, (
        0x7722, 0xc008, 0xe908, 0xf074, 0x4610,
        0xf030, 0x09ff, 0xf074, 0x45c2)),
    "codec_shared_control_dispatch": (0xa580, (
        0xf6bb, 0x12f8, 0x0854, 0xf1f4, 0x60f8, 0x000b, 0x0008,
        0xf820, 0xa594, 0x61f8, 0x00ef, 0x000f, 0xf830, 0xa592,
        0xf030, 0x09ff, 0xf073, 0xa594, 0xf040, 0x0600,
        0xf030, 0x0fff, 0xf074, 0x45c2)),
    "codec_shared_control_alternate_dispatch": (0x27f0, (
        0x12f8, 0x0854, 0xf1f4, 0x60f8, 0x000b, 0x0009,
        0xf820, 0x2805, 0xf030, 0x0fff, 0x80f8, 0x1f92,
        0x60f8, 0x19a2, 0x0000, 0xf830, 0xa594,
        0xf030, 0x0f0f, 0x1af8, 0x19a4, 0xf073, 0xa584)),
    "codec_register8_constant_setup": (0xb89c, (
        0xe908, 0xf074, 0xa484, 0xf020, 0x0602, 0xe908,
        0xf074, 0x45c2, 0xf020, 0x0944, 0xe909, 0xf074, 0x45c2)),
    "unresolved_data32_control": (0x3b48, (0x7732, 0xc00c)),
    "unresolved_data32_reset_release": (0x3b5c, (0x7732, 0xc008, 0x7732, 0xc0c8)),
})

READ_SITES = [0x321e, 0x33f3, 0x3c6f, 0x3c84, 0x4231,
              0x4275, 0x43c2, 0x43ef, 0x4473]
WRITE_SITES = [0x3225, 0x33fa, 0x3c77, 0x3c8c, 0x4239, 0x427d,
               0x43ca, 0x43f7, 0x4479, 0x4553, 0x4557, 0x455b]


def decode_host_dispatch_mask(image):
    """Union of checked ISR predicates, not a hardware acknowledgement mask."""
    address, words = SEQUENCES['host_command_dispatch']
    expected = b''.join(word.to_bytes(2, 'big') for word in words)
    if image[address * 2:(address + len(words)) * 2] != expected:
        raise ValueError('host command dispatcher differs from reviewed ROM')
    mask = 0
    for index in range(len(words) - 3):
        if words[index] == 0xf130 and words[index + 2] == 0xf84c:
            mask |= words[index + 1]
    return mask


def check(image: bytes) -> list[str]:
    if len(image) % 2:
        raise ValueError("C54x image must contain complete big-endian words")
    results = []
    for name, (address, expected) in SEQUENCES.items():
        raw = image[address * 2:(address + len(expected)) * 2]
        words = tuple(int.from_bytes(raw[i:i + 2], "big")
                      for i in range(0, len(raw), 2))
        if words != expected:
            raise ValueError(f"{name}: ROM words differ at {address:04x}")
        results.append(name)
    sites = census(image)
    if sites.get(("R", 0x21), []) != READ_SITES:
        raise ValueError("port21 reader census differs from reviewed nine sites")
    if sites.get(("W", 0x21), []) != WRITE_SITES:
        raise ValueError("port21 writer census differs from reviewed twelve sites")
    for port, expected in {0x30: [0x0a94, 0x30b5, 0x4565, 0x6976, 0xa17c],
                           0x31: [0x36f7, 0x370c, 0x4020, 0xa23a],
                           0x32: [0x36fb, 0x3710, 0x4025, 0xa23e]}.items():
        if sites.get(("R", port), []) or sites.get(("W", port), []) != expected:
            raise ValueError(f"port{port:02x} programming census differs from reviewed sites")
    return results


def check_live_serial_helpers(text):
    """Exclude an uploaded helper replacement, not infer reset completion."""
    rows = re.findall(r'rom4_serial_live_code: address=([0-9a-f]{4}) words=([0-9a-f,]+)', text)
    expected = {SEQUENCES[name][0]: SEQUENCES[name][1]
                for name in ('serial_prepare_hook_defaults', 'serial_prepare_state')}
    if text.count('rom4_serial_live_code:') != len(rows) or len(rows) != 2 or len({address for address, _ in rows}) != 2:
        raise ValueError('missing or duplicate live serial helpers')
    for address, raw in rows:
        if tuple(int(word, 16) for word in raw.split(',')) != expected.get(int(address, 16)):
            raise ValueError('live serial helper differs from checked resident code')
    return len(rows)


SCHEDULE_LISTS = {
    0xedf4: (0x99aa, 0x99c6, 0x99b3, 0x9a56, 0x9a7a, 0x9a71,
             0x9b15, 0x9be0, 0x9bee, 0x9d20, 0x9d3d, 0x00ff),
    0xee00: (0x990d, 0x99aa, 0x99c6, 0x99b3, 0x9a56, 0x9a71,
             0x9b15, 0x9bbb, 0x9c15, 0x9d20, 0x9d3d, 0x00ff),
    0xee0c: (0x990d, 0x99aa, 0x99c6, 0x99b3, 0x9a56, 0x9a71,
             0x9b15, 0x9504, 0x9c4f, 0x9d20, 0x9d3d, 0x00ff),
    0xee18: (0x990d, 0x99aa, 0x99c6, 0x99b3, 0x9a56, 0x9a71,
             0x9b15, 0x9a7a, 0x9a34, 0x9bbb, 0x9cb9, 0x9d20, 0x9d3d, 0x00ff),
    0xee26: (0x990d, 0x99aa, 0x99c6, 0x99b3, 0x9a56, 0x9a71,
             0x9b15, 0x9bbb, 0x9d20, 0x9d3d, 0x00ff),
    0xee31: (0x9a7a, 0x9a34, 0x9b59, 0x9be0, 0x9d20, 0x9d3d, 0x00ff),
    0xee38: (0x990d, 0x99d5, 0x9aa8, 0x9bbb, 0x9d20, 0x00ff),
    0xee3e: (0x990d, 0x99d5, 0x9aa8, 0x9bbb, 0x9d20, 0x9d3d, 0x00ff),
    0xee45: (0x990d, 0x99d5, 0x9a56, 0x9a71, 0x9aa8, 0x9bbb, 0x9d20, 0x9d3d, 0x00ff),
    0xee4e: (0x9aa8, 0x9b4a, 0x9b59, 0x9bbb, 0x9be0, 0x9d20, 0x9d3d, 0x9d49, 0x00ff),
    0xee57: (0x99aa, 0x99d5, 0x99b3, 0x9a56, 0x9a71, 0x9a7a, 0x9aa8,
             0x9b4a, 0x9a34, 0x9b59, 0x9bbb, 0x9be0, 0x9d20, 0x9d3d, 0x00ff),
}


def check_schedule_data(image: bytes) -> int:
    if len(image) != 0x20000:
        raise ValueError("schedule data requires a complete 64K-word data image")
    for address, expected in SCHEDULE_LISTS.items():
        raw = image[address * 2:(address + len(expected)) * 2]
        words = tuple(int.from_bytes(raw[i:i + 2], "big")
                      for i in range(0, len(raw), 2))
        if words != expected:
            raise ValueError(f"schedule data differs at {address:04x}")
    return len(SCHEDULE_LISTS)


def check_trace(text: str) -> None:
    records = re.findall(
        r"rom4_serial_audit: space=(data|io) direction=(read|write) "
        r"address=([0-9a-f]{4}) value=([0-9a-f]{4}) mask=ffff "
        r"pc=([0-9a-f]{4}) t=([0-9.]+)", text)
    if any(space == "data" and address == "0032"
           for space, _, address, _, _, _ in records):
        raise ValueError("unresolved data0032 access requires peripheral review")
    echo = [(direction, address, value, pc)
            for space, direction, address, value, pc, _ in records
            if space == "data" and address in ("0020", "0021")]
    if echo != [("write", "0021", "0aaa", "0e31"),
                ("read", "0020", "0aaa", "0e5d"),
                ("read", "0020", "0aaa", "3464"),
                ("write", "0021", "ffd5", "358b")]:
        raise ValueError("data-space boot echo/receive ISR missing, duplicated or changed")
    setup = [(direction, value, pc) for space, direction, address, value, pc, _ in records
             if space == "data" and address == "0022" and direction == "write"]
    if setup[:2] != [("write", "c008", "0e22"), ("write", "c0c8", "0e24")]:
        raise ValueError("missing native serial reset/release setup")
    io = [(direction, value, pc) for space, direction, address, value, pc, _ in records
          if space == "io" and address == "0021"]
    expected = [("write", "1482", "4555"), ("write", "1482", "4559"),
                ("write", "0482", "455d")]
    expected += [("read", "0482", "3221"), ("write", "0c82", "3228"),
                 ("read", "0c82", "33f6"), ("write", "0482", "33fd")] * 3
    if io[:len(expected)] != expected:
        raise ValueError("I/O control readback does not preserve initialization/frame masks")
    if "Lua error" in text:
        raise ValueError("Lua observation failed")


def check_restore(text: str, *, require_replay: bool = False) -> None:
    records = re.findall(
        r"rom4_codec_state: phase=(saved|restored) "
        r"(t=[0-9.]+ pc=[0-9a-f]{4} st0=[0-9a-f]{4} "
        r"st1=[0-9a-f]{4} sp=[0-9a-f]{4} io21=[0-9a-f]{4} bspc22=[0-9a-f]{4})", text)
    if len(records) != 2 or [phase for phase, _ in records] != ["saved", "restored"]:
        raise ValueError("expected one ordered native save/restore snapshot pair")
    if records[0][1] != records[1][1]:
        raise ValueError("native codec word/CPU/time did not restore exactly")
    if "Lua error" in text or "state_roundtrip: result=pass" not in text:
        raise ValueError("native restore harness failed")
    if require_replay or 'rom4_codec_replay:' in text:
        replay = re.findall(r'rom4_codec_replay: result=(pass|fail) reference_events=(\d+) restored_events=(\d+) window=([0-9.]+)', text)
        if (text.count('rom4_codec_replay:') != 1 or len(replay) != 1
                or replay[0][0] != 'pass' or int(replay[0][1]) <= 0
                or replay[0][1] != replay[0][2] or not 0 < float(replay[0][3]) < 1):
            raise ValueError('native DSP I/O continuation did not replay exactly')


def check_shared_codec_control(text: str) -> None:
    """Observed original-MCU packed control, not inferred COBBA bit meanings."""
    presses = re.findall(r"input-press: t=([0-9.]+) name=1\b", text)
    if len(presses) != 1:
        raise ValueError("packed codec-control provenance requires the physical-key phase boundary")
    limit = float(presses[0])
    patterns = (
        r"rom4_tone_access: owner=codec_shared_mcu direction=write address=0100a8 "
        r"value=84260000 mask=ffff0000 pc=272034 t=([0-9.]+)",
        r"rom4_tone_access: owner=codec_shared_dsp direction=read address=000854 "
        r"value=8426 mask=ffff pc=0027f2 t=([0-9.]+)",
        r"rom4_codec_control_call: hit=\d+ a=0000000026 b=0000000008 "
        r"sp=[0-9a-f]{4} return=a598 t=([0-9.]+)",
        r"rom4_codec_control_call: hit=\d+ a=0000000626 b=0000000008 "
        r"sp=[0-9a-f]{4} return=a4bb t=([0-9.]+)",
    )
    previous = -1.0
    for pattern in patterns:
        times = [float(time) for time in re.findall(pattern, text)
                 if previous < float(time) < limit]
        if not times:
            raise ValueError("missing ordered packed codec-control provenance: " + pattern)
        previous = min(times)


def check_tone(text: str) -> None:
    check_trace(text)
    if not re.search(r"rom4_tone_enable: imr=035f ifr=0020 bspc22=c8c8", text):
        raise ValueError("missing masked transmit-ready interrupt observation")
    press = re.findall(r"input-press: t=([0-9.]+) name=1\b", text)
    release = re.findall(r"input-release: t=([0-9.]+) name=1\b", text)
    if len(press) != 1 or len(release) != 1:
        raise ValueError("expected one physical numeric-key press/release")
    start, end = float(press[0]), float(release[0])
    calls = re.findall(
        r"rom4_codec_control_call: hit=(\d+) a=([0-9a-f]{10}) b=([0-9a-f]{10}) "
        r"sp=([0-9a-f]{4}) return=([0-9a-f]{4}) t=([0-9.]+)", text)
    if not any(a == "0000000626" and b == "0000000008" and ret == "a4bb"
               and start <= float(time) < end
               for _, a, b, _, ret, time in calls):
        raise ValueError("missing organic register-8 activation caller after physical key")
    check_shared_codec_control(text)
    events = re.findall(
        r"rom4_tone_access: owner=(mcu|dsp) direction=(read|write) "
        r"address=([0-9a-f]{6}) value=([0-9a-f]+) mask=([0-9a-f]+) "
        r"pc=([0-9a-f]{6}) t=([0-9.]+)", text)
    required = [("mcu", "write", "0100ac", "e10000", "ffff0000", "272034"),
                ("dsp", "read", "000856", "00e1", "ffff", "00a59a"),
                ("dsp", "write", "0000fe", "00e1", "ffff", "00a5de")]
    cursor = start
    for event in required:
        matches = [float(time) for *fields, time in events
                   if tuple(fields) == event and cursor <= float(time) < end]
        if not matches:
            raise ValueError("missing ordered organic tone command/initializer: " + str(event))
        cursor = matches[0]
    summaries = re.findall(
        r"rom4_tone_summary: tx_words=(\d+) rx_reads=(\d+) tone_reads=(\d+) "
        r"tone_copies=(\d+) t=([0-9.]+)", text)
    if len(summaries) != 1:
        raise ValueError("missing uncapped native tone summary")
    tx, rx, reads, copies, time = summaries[0]
    if (int(tx), int(rx)) != (2, 2) or int(reads) == 0 or int(copies) == 0 or float(time) < 11:
        raise ValueError("native tone/serial boundary changed; review actual sample activity")
    flag_events = re.findall(
        r"rom4_tone_access: owner=(mcu|speech_shared|speech_flags|speech_record) direction=(read|write) "
        r"address=([0-9a-f]{6}) value=([0-9a-f]+) mask=([0-9a-f]+) "
        r"pc=([0-9a-f]{6}) t=([0-9.]+)", text)
    entries = re.findall(
        r"rom4_speech_field_entry: input=df29 parameter=0000 caller=00275445 "
        r"pc=00271d70 source=([0-9a-f]{8}) index=09 t=([0-9.]+)", text)
    entries = [(int(source, 16), float(t)) for source, t in entries
               if 0x100000 <= int(source, 16) < 0x180000]
    if not entries:
        raise ValueError("missing original MCU speech-field caller")
    # 274cb8 is LDRH r0,[r4,#0x16]; the following STRH at 274cba owns
    # the lower-halfword publication at [r5,#0x12].
    record_writes = [float(t) for *fields, t in flag_events
                     if tuple(fields) == ("speech_record", "write", "10b2b8", "df29", "ffff", "274cba")
                     and float(t) <= entries[0][1]]
    if not record_writes:
        raise ValueError("missing preceding speech-record lower-halfword initialization")
    configuration = re.findall(
        r"rom4_speech_configuration_entry: selector=0210 destination=0010a6c4 "
        r"length=0004 caller=002894fd t=([0-9.]+)", text)
    if not any(float(t) <= min(record_writes) for t in configuration):
        raise ValueError("missing preceding speech EEPROM configuration read")
    cursor = entries[0][1]
    # Byte-pinned MVDK at 27ac is 7194,1f2b. The destination extension
    # is consumed before its source read, so both bus taps report PC 27ae.
    for expected, earliest in (
            (("mcu", "write", "0100bc", "df290000", "ffff0000", "272034"), 0.0),
            (("speech_shared", "read", "00085e", "df29", "ffff", "0027ae"), 0.0),
            (("speech_flags", "write", "001f2b", "df29", "ffff", "0027ae"), 0.0),
            (("speech_shared", "read", "00085e", "df29", "ffff", "0027ae"), start),
            (("speech_flags", "write", "001f2b", "df29", "ffff", "0027ae"), start)):
        matches = [float(t) for *fields, t in flag_events
                   if tuple(fields) == expected and float(t) >= max(cursor, earliest)]
        if not matches:
            raise ValueError("missing ordered shared speech-control copy: " + str(expected))
        cursor = matches[0]


MCU_SPEECH_FIELD_BYTES = {
    0x28950c: bytes.fromhex("002a6438"),
    0x2a6450: bytes.fromhex("002a610800030000"),
    0x2a6110: bytes.fromhex("0000030102100004"),
    0x28367c: bytes.fromhex("b5304877497d2200f005ff20"),
    0x28385c: bytes.fromhex("0010a6c4"),
    0x283878: bytes.fromhex("00000301"),
    0x274cb8: bytes.fromhex("8ae08268"),
    0x27500c: bytes.fromhex("0010a6b0"),
    0x275044: bytes.fromhex("0010b2a8"),
    0x27503c: bytes.fromhex("20087ce94308e00a"),
    0x275054: bytes.fromhex("20f77ce9400874e8"),
    0x28c308: bytes.fromhex("b5f04642b40446881c07"),
    0x28c320: bytes.fromhex("210e09f84001205f43c0430806000e06"),
    0x28c360: bytes.fromhex("06380e00f001fe72"),
    0x275426: bytes.fromhex("46442701886188684281d00c4649465072c848875dc088692200f7fcfc96"),
    0x275658: bytes.fromhex("002a5df8"),
    0x2a5df8: bytes.fromhex("08091b252021222324282d"),
    0x271d70: bytes.fromhex("b5f04643b4081c151c0e1c04"),
    0x271d94: bytes.fromhex("1c20282ed862a102008058084687"),
    0x271e44: bytes.fromhex("00271e9a"),
    0x271e9a: bytes.fromhex("4adee7e6"),
    0x271e6c: bytes.fromhex("1c30e0d6"),
    0x27201e: bytes.fromhex("2c2ed00946612900d005881146664031430104080c008010"),
    0x272036: bytes.fromhex("2d01d0012c2ed108200146415258498f20048008498f20027008"),
    0x272284: bytes.fromhex("0003000000020008"),
    0x271760: bytes.fromhex("49ad2004800849ad20027008"),
    0x271a18: bytes.fromhex("0003000000020008"),
    0x2721ce: bytes.fromhex("2004492c8008"),
    0x272214: bytes.fromhex("000100bc"),
}


def check_mcu_speech_field(image):
    """Original big-endian MCU bytes, not the Thumb swap16 analysis image."""
    for address, expected in MCU_SPEECH_FIELD_BYTES.items():
        offset = address - 0x200000
        if image[offset:offset + len(expected)] != expected:
            raise ValueError(f"MCU speech field contract differs at {address:08x}")
    return len(MCU_SPEECH_FIELD_BYTES)


def census_mcu_host_interface(image):
    """Reuse conservative literal-seeded analysis, not a writer-absence proof."""
    from tools.mad2_static_census import analyze_image
    if len(image) % 2:
        raise ValueError('MCU image requires complete big-endian halfwords')
    swapped = bytearray(image)
    swapped[0::2], swapped[1::2] = image[1::2], image[0::2]
    accesses, coverage = analyze_image(bytes(swapped), window_base=0x30000,
                                       window_size=4, constant_indexes=True)
    return {'schema': 1, 'window_base': 0x30000, 'window_size': 4,
            'scope': 'literal-seeded Thumb candidates; dynamic and table-driven writers excluded',
            'coverage': coverage, 'accesses': accesses}


def decode_mcu_speech_descriptor(image):
    """Decode the observed direct-index metadata path, not arbitrary descriptors."""
    def read(address, length):
        offset = address - 0x200000
        if offset < 0 or offset + length > len(image):
            raise ValueError("speech descriptor outside original MCU image")
        return image[offset:offset + length]

    catalogue = int.from_bytes(read(0x28950c, 4), "big")
    group = read(catalogue + 3 * 8, 8)
    table = int.from_bytes(group[:4], "big")
    count = int.from_bytes(group[4:6], "big")
    if count <= 1:
        raise ValueError("speech descriptor index absent from group")
    record = read(table + 8, 8)
    identifier = int.from_bytes(record[:4], "big")
    if identifier != 0x301:
        raise ValueError("speech descriptor direct-index identifier differs")
    return {"identifier": identifier, "offset": int.from_bytes(record[4:6], "big"),
            "length": int.from_bytes(record[6:8], "big"), "record_address": table + 8}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("--data-image", type=Path,
                        help="check receive scheduling lists in the separate DSP data image")
    parser.add_argument("--mcu-image", type=Path,
                        help="verify uploaded counter guards against original NSE-1 MCU flash")
    parser.add_argument("--trace", type=Path,
                        help="check a fresh four-second passive codec observer log")
    parser.add_argument("--restore-log", type=Path,
                        help="check exact native I/O-word restoration snapshots")
    parser.add_argument('--require-restore-replay', action='store_true',
                        help='also require matching nonempty post-restore DSP I/O traces')
    parser.add_argument("--tone-log", type=Path,
                        help="check physical numeric-key tone delivery and serial totals")
    parser.add_argument('--serial-helper-trace', type=Path,
                        help='check live helper readback at the serial restart; not a completion test')
    args = parser.parse_args()
    if args.require_restore_replay and args.restore_log is None:
        parser.error('--require-restore-replay requires --restore-log')
    try:
        image = args.image.read_bytes()
        results = check(image)
        dispatch_mask = decode_host_dispatch_mask(image)
        if args.mcu_image is not None:
            upload_locations = check_mcu_upload_counters(args.mcu_image.read_bytes())
            check_mcu_speech_field(args.mcu_image.read_bytes())
            descriptor = decode_mcu_speech_descriptor(args.mcu_image.read_bytes())
            if (descriptor["offset"], descriptor["length"]) != (0x210, 4):
                raise ValueError("speech descriptor storage contract differs")
        if args.data_image is not None:
            check_schedule_data(args.data_image.read_bytes())
        if args.trace is not None:
            check_trace(args.trace.read_text(errors="replace"))
        if args.restore_log is not None:
            check_restore(args.restore_log.read_text(errors="replace"), require_replay=args.require_restore_replay)
        if args.tone_log is not None:
            check_tone(args.tone_log.read_text(errors="replace"))
        if args.serial_helper_trace is not None:
            check_live_serial_helpers(args.serial_helper_trace.read_text(errors='replace'))
    except (OSError, ValueError) as error:
        parser.exit(1, f"FAIL: {error}\n")
    print("PASS: " + ", ".join(results))
    if args.mcu_image is not None:
        print("MCU upload counter blocks: PASS " + ", ".join(
            f"{name}=0x{offset:06x}" for name, offset in upload_locations.items()))
    if args.data_image is not None:
        print(f"DSP data-space receive scheduling lists: {len(SCHEDULE_LISTS)}/{len(SCHEDULE_LISTS)} PASS")
    print("Candidate immediate-port coverage: reads=9/9 writes=12/12; uploads excluded.")
    print(f"Reviewed host-dispatch predicate union: {dispatch_mask:04x}; not a hardware acknowledgement mask.")
    if args.trace is not None:
            print("Native echo, receive-ready ISR and three operational control readback cycles: PASS")
    if args.restore_log is not None:
        print("Native I/O word, CPU registers and save-time restoration: PASS")
        if args.require_restore_replay:
            print('Native DSP I/O continuation replay: PASS')
    if args.tone_log is not None:
        print("Organic tone initialization: PASS; operational_serial_samples=0 native_audio_claim=0")
    if args.serial_helper_trace is not None:
        print('Live serial helper readback: 2/2 PASS; peripheral_completion_claim=0')
    print("Bounded ROM sequences only; physical port ownership and serial clock attachment remain unresolved.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

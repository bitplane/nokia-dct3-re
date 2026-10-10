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
    "rf_sample_queue_empty_wait": (0x4798, (
        0x7719, 0x0048, 0x7711, 0x06b8, 0x10f8, 0x06b7,
        0x0881, 0xfa45, 0x479e, 0x10f8, 0x06b7,
        0x7214, 0x06b8)),
    "rf_queue_copy_from_request": (0xa1b7, (
        0x7712, 0x1920, 0xec09, 0x7092, 0x216d)),
    "rf_slot_queue_consumption": (0x3706, (
        0x7212, 0x197b, 0x7213, 0x197d, 0x6d92, 0xf495,
        0x7593, 0x0031, 0xf495, 0xf495, 0x7593, 0x0032,
        0x7313, 0x197d)),
    "fr_encoder_wrapper_gate": (0x9c4f, (
        0xf074, 0x247c, 0x60f8, 0x1836, 0x00c1, 0xfc20,
        0x61f8, 0x06bb, 0x0003, 0xf830, 0x9c9f)),
    "fr_encoder_pcm_and_codec": (0x9c91, (
        0xf274, 0xa0b8, 0x7712, 0x1536,
        0xf274, 0x85e0, 0x7714, 0x1536, 0xf074, 0x860c)),
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
    "codec_register8_activation": (0xa4b4, (
        0xe908, 0xf074, 0x4610, 0xf040, 0x0600, 0xf074, 0x45c2)),
    "codec_register8_deactivation": (0xa502, (
        0x7722, 0xc008, 0xe908, 0xf074, 0x4610,
        0xf030, 0x09ff, 0xf074, 0x45c2)),
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
    return results


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


def check_restore(text: str) -> None:
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


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("--trace", type=Path,
                        help="check a fresh four-second passive codec observer log")
    parser.add_argument("--restore-log", type=Path,
                        help="check exact native I/O-word restoration snapshots")
    parser.add_argument("--tone-log", type=Path,
                        help="check physical numeric-key tone delivery and serial totals")
    args = parser.parse_args()
    try:
        results = check(args.image.read_bytes())
        if args.trace is not None:
            check_trace(args.trace.read_text(errors="replace"))
        if args.restore_log is not None:
            check_restore(args.restore_log.read_text(errors="replace"))
        if args.tone_log is not None:
            check_tone(args.tone_log.read_text(errors="replace"))
    except (OSError, ValueError) as error:
        parser.exit(1, f"FAIL: {error}\n")
    print("PASS: " + ", ".join(results))
    print("Candidate immediate-port coverage: reads=9/9 writes=12/12; uploads excluded.")
    if args.trace is not None:
            print("Native echo, receive-ready ISR and three operational control readback cycles: PASS")
    if args.restore_log is not None:
        print("Native I/O word, CPU registers and save-time restoration: PASS")
    if args.tone_log is not None:
        print("Organic tone initialization: PASS; operational_serial_samples=0 native_audio_claim=0")
    print("Bounded ROM sequences only; physical port ownership and serial clock attachment remain unresolved.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

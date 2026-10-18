"""Pin organic NSE-1 uploads and the reviewed reset-free staged checkpoint."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys


SOURCE_OFFSET = 0x94cf8
TAIL_SOURCE_OFFSET = 0x98a60
REPEAT_SOURCE_OFFSET = 0x98af0
DEMAND_SOURCE_OFFSET = 0x958e8
REFERENCE = Path(__file__).resolve().parents[1] / "fixtures/c54x_rom4_staged_checkpoint.json"
EVENT = re.compile(r"rom4_staged_[a-z_]+:.*|rom4_rf_read:.*|rom4_rf_port32:.*|"
                   r"rom4_transfer_timer:.*|rom4_interface_summary:.*|"
                   r"rom4_validator: event=continue.*")
TIME = re.compile(r"(?<= )t=(\d+\.\d+)(?= |$)")


def records(text):
    return [match.group(0).strip() for line in text.splitlines()
            if (match := EVENT.search(line))]


def semantic_row(row):
    return TIME.sub("t=<time>", row)


def check(text, firmware):
    if ("rom4_reset_request" in text or "unsupported" in text
            or "unimplemented" in text or re.search(r"TMS320C54x.*illegal", text)):
        raise ValueError("reset or DSP fault in staged checkpoint")
    expected = json.loads(REFERENCE.read_text())["records"]
    observed = records(text)
    if len(observed) != len(expected):
        raise ValueError(f"checkpoint record count changed: {len(observed)} != {len(expected)}")
    for index, (actual, reference) in enumerate(zip(observed, expected), 1):
        if semantic_row(actual) != semantic_row(reference):
            raise ValueError(f"checkpoint record {index} changed: {actual}")

    # Absolute sub-microsecond timing is not a hardware contract. Preserve
    # event order, ownership epochs and strict serial/handshake causality.
    last_time = 0.0
    stream_times = {}
    for row, reference in zip(observed, expected):
        match = TIME.search(row)
        if not match:
            continue
        timestamp = float(match.group(1))
        if not last_time <= timestamp <= 10.05:
            raise ValueError("checkpoint event timestamp is out of order or bounds")
        last_time = timestamp
        tag = row.split(":", 1)[0]
        if tag != "rom4_rf_read" and tag in stream_times and timestamp <= stream_times[tag]:
            raise ValueError("checkpoint stream timestamps are not strictly ordered")
        stream_times[tag] = timestamp
        reference_time = float(TIME.search(reference).group(1))
        if (timestamp < 0.5) != (reference_time < 0.5):
            raise ValueError("checkpoint event crossed the half-second snapshot")
    if not 10 <= last_time <= 10.05:
        raise ValueError("missing ten-second observation endpoint")

    def fields(prefix):
        row = next(row for row in observed if row.startswith(prefix))
        return dict(re.findall(r"(\w+)=(\S+)", row))

    snapshot = fields("rom4_staged_frontier:")
    if not 0.5 <= float(snapshot["t"]) <= 1:
        raise ValueError("wrong snapshot epoch")
    endpoint = fields("rom4_staged_end:")
    summary = fields("rom4_interface_summary:")
    if fields("rom4_staged_profile:")["system"] != "nse1pipe":
        raise ValueError("wrong staged machine identity")
    if snapshot["illegal"] != "0" or endpoint["illegal"] != "0":
        raise ValueError("staged CPU ILLEGAL flag is set")
    for row in observed:
        if row.startswith("rom4_validator:"):
            validator = dict(re.findall(r"(\w+)=(\S+)", row))
            if validator["r6"] != "00000001" or validator["r8"] != "00000000":
                raise ValueError("MCU did not accept the DSP challenge response")
    offsets = [
        ("rom4_staged_frontier:", 40, SOURCE_OFFSET),
        ("rom4_staged_uploaded_tail:", 24, TAIL_SOURCE_OFFSET),
        ("rom4_staged_repeat_tail:", 24, REPEAT_SOURCE_OFFSET),
        ("rom4_staged_demand_tail:", 32, DEMAND_SOURCE_OFFSET),
    ]
    for prefix, length, offset in offsets:
        words = [int(word, 16) for word in fields(prefix)["words"].split(",")]
        if len(words) != length:
            raise ValueError("uploaded word count changed")
        packed = struct.pack(f">{length}H", *words)
        if firmware.find(packed) != offset or firmware.find(packed, offset + 1) != -1:
            raise ValueError(f"upload at {offset:05x} does not bind uniquely to MCU source")

    def values(prefix):
        return [int(dict(re.findall(r"(\w+)=(\S+)", row))["value"], 16)
                for row in observed if row.startswith(prefix)]

    rf_reads = int(summary["rf_reads"])
    blocks, remainder = divmod(rf_reads, 32)
    return dict(
        status="live_staged_native_service_checkpoint",
        source_offset=SOURCE_OFFSET, observed_words=40,
        tail_source_offset=TAIL_SOURCE_OFFSET, observed_tail_words=24,
        repeat_source_offset=REPEAT_SOURCE_OFFSET, observed_repeat_words=24,
        demand_source_offset=DEMAND_SOURCE_OFFSET, observed_demand_words=32,
        firmware_sha256=hashlib.sha256(firmware).hexdigest(),
        checkpoint_pc=int(endpoint["pc"], 16), terminal_illegal=False,
        host_header_reads=int(endpoint["host_headers"]),
        host_consumed=int(endpoint["host_consumed"]),
        low_gap_bus_reads=int(endpoint["gap_reads"]),
        rf_sample_reads=rf_reads, rf_sample_blocks=blocks, rf_sample_remainder=remainder,
        rf_control_writes=int(summary["rf_port32_writes"]),
        observed_rf_control_writes=sum(row.startswith("rom4_rf_port32:") for row in observed),
        transfer_expiries=int(summary["transfer_expiries"]),
        observed_transfer_expiries=sum(row.startswith("rom4_transfer_timer:") for row in observed),
        bsp_transmit_words=values("rom4_staged_bsp_tx:"),
        bsp_receive_words=values("rom4_staged_bsp_rx:"),
        bsp_control_writes=values("rom4_staged_bsp_write:"),
        completion_strobes=int(summary["completion_strobes"]),
        checkpoint_illegal=False, reset_request_observed=False,
        clean_validator_continuations=sum(row.startswith("rom4_validator:") for row in observed),
        uninterrupted_session_claim=False,
        swwsr=int(snapshot["swwsr"], 16), boot_claim=False,
        preceding_instruction_retirement_claim=False, upload_complete_claim=False,
        fitted_bus_dialect_claim=False, physical_low_gap_identity_claim=False,
        rf_acquisition_claim=False, parallel_store_carry_claim=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("firmware", type=Path)
    args = parser.parse_args()
    try:
        result = check(args.log.read_text(errors="replace"), args.firmware.read_bytes())
    except (OSError, ValueError, struct.error) as error:
        print(f"Staged ROM4 observation: FAIL: {error}", file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Run isolated acquired ROM4 algorithms, not handset audio delivery."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

PASS = "TMS320C54x ROM4 CRC helper: PASS cases=512 gsm_packing_claim=0 speech_claim=0"
COMPLETE = "Fatal error: TMS320C54x ROM4 CRC helper complete"
PACKED_PASS = "TMS320C54x ROM4 packed CRC: PASS cases=516 basis_bits=192 gsm_crc_claim=1 speech_claim=0"
PACKED_COMPLETE = "Fatal error: TMS320C54x ROM4 packed CRC complete"
FR_PASS = "TMS320C54x ROM4 FR packing: PASS vectors=293 basis_bits=260 protected_bits=50 speech_claim=0"
FR_COMPLETE = "Fatal error: TMS320C54x ROM4 FR packing complete"
DECODER_PASS = "TMS320C54x ROM4 FR decoder: PASS frames=32 samples=5120 handset_audio_claim=0"
DECODER_COMPLETE = "Fatal error: TMS320C54x ROM4 FR decoder complete"
DECODER_COMPARISON = "TMS320C54x ROM4 FR decoder comparison: frames=32 samples=5120 mismatches=0"
DECODER_REPLAY = "TMS320C54x ROM4 FR decoder replay: PASS frames=32 ram_words=65440 checkpoints=frame_input reference_advance=once handset_audio_claim=0"
ENCODER_PASS = "TMS320C54x ROM4 FR encoder: PASS frames=32 parameters=2432 handset_audio_claim=0"
ENCODER_COMPLETE = "Fatal error: TMS320C54x ROM4 FR encoder complete"
ENCODER_COMPARISON = "ROM4 FR encoder aggregate: frames=32 parameters=2432 mismatches=0"
ENCODER_REPLAY = "TMS320C54x ROM4 FR encoder replay: PASS frames=32 ram_words=65440 checkpoints=before_preprocessing reference_advance=once handset_audio_claim=0"
PCM_MUTE_PASS = "TMS320C54x ROM4 PCM mute: PASS cases=8 samples=1280 guards=16 handset_audio_claim=0"
EFR_CRC_PASS = "TMS320C54x ROM4 EFR CRC: PASS cases=554 basis_bits=244 protected_bits=65 handset_audio_claim=0"
EFR_CRC_COMPLETE = "Fatal error: TMS320C54x ROM4 EFR CRC complete"
EFR_PACKING_PASS = "TMS320C54x ROM4 EFR packing: PASS vectors=277 basis_bits=244 corrupt_frames=277 handset_audio_claim=0"
EFR_PACKING_COMPLETE = "Fatal error: TMS320C54x ROM4 EFR packing complete"


def file_identity(path):
    path = Path(path).resolve()
    with path.open("rb") as source:
        digest = hashlib.file_digest(source, "sha256").hexdigest()
    return {"path": str(path), "bytes": path.stat().st_size, "sha256": digest}


def check_result(text, returncode, packed=False, fr_packing=False, fr_decoder=False, fr_encoder=False, efr_crc=False, efr_packing=False):
    lines = text.splitlines()
    marker, complete = (PACKED_PASS, PACKED_COMPLETE) if packed else (PASS, COMPLETE)
    if efr_crc:
        marker, complete = EFR_CRC_PASS, EFR_CRC_COMPLETE
    if efr_packing:
        marker, complete = EFR_PACKING_PASS, EFR_PACKING_COMPLETE
    if fr_packing:
        marker, complete = FR_PASS, FR_COMPLETE
    if fr_decoder:
        marker, complete = DECODER_PASS, DECODER_COMPLETE
        if lines.count(DECODER_COMPARISON) != 1:
            raise ValueError("missing unique bit-exact PCM comparison")
        if lines.count(DECODER_REPLAY) != 1:
            raise ValueError("missing unique native decoder frame/history replay")
    if fr_encoder:
        marker, complete = ENCODER_PASS, ENCODER_COMPLETE
        if lines.count(ENCODER_COMPARISON) != 1:
            raise ValueError("missing unique bit-exact parameter comparison")
        if lines.count(ENCODER_REPLAY) != 1:
            raise ValueError("missing unique native encoder frame/history replay")
        if lines.count(PCM_MUTE_PASS) != 1:
            raise ValueError("missing unique native PCM mute buffer-boundary result")
    if returncode != 3:
        raise ValueError(f"unexpected MAME exit {returncode}; expected fixture completion 3")
    if lines.count(marker) != 1 or not lines or lines[-1] != complete:
        raise ValueError("missing unique result or terminal fixture completion")
    if any(line.startswith("Fatal error:") and line != complete for line in lines):
        raise ValueError("unexpected fatal error")
    if "WRONG CHECKSUMS" in text or "NOT FOUND" in text:
        raise ValueError("acquired ROM input failed validation")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mame", type=Path, required=True)
    parser.add_argument("--rompath", type=Path, required=True)
    parser.add_argument("--output-root", type=Path,
                        default=Path(__file__).resolve().parents[1] / 'tmp')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--packed", action="store_true", help="validate the outer packed-input CRC routine")
    mode.add_argument("--fr-packing", action="store_true", help="validate FR parameter packing, CRC and unpacking")
    mode.add_argument("--fr-decoder", action="store_true", help="compare native FR decoding against independent PCM")
    mode.add_argument("--fr-encoder", action="store_true", help="compare native FR encoding against independent parameters")
    mode.add_argument("--efr-crc", action="store_true", help="compare native EFR CRC against normative polynomial division")
    mode.add_argument("--efr-packing", action="store_true", help="validate native EFR packing, unpacking and CRC rejection")
    args = parser.parse_args()
    args.output_root.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix="run_c54x_rom4_crc_", dir=args.output_root))
    bios = "fr-encoder" if args.fr_encoder else "fr-decoder" if args.fr_decoder else "fr-packing" if args.fr_packing else "crc-packed" if args.packed else "crc-helper"
    if args.efr_crc:
        bios = "efr-crc"
    if args.efr_packing:
        bios = "efr-packing"
    command = [str(args.mame.resolve()), "tms54rom4", "-bios", bios,
               "-rompath", str(args.rompath.resolve()), "-video", "none",
               "-sound", "none", "-nothrottle", "-noreadconfig", "-log",
               "-seconds_to_run", "2"]
    root = Path(__file__).resolve().parents[1]
    inputs = [args.mame, Path(__file__), root / "driver/tms320c54x_test.cpp"]
    inputs.extend(root / name for name in ("lib/util/gsmfr.cpp", "lib/util/gsmfr.h",
                                           "driver/gsm_tch_f_l1.cpp", "driver/gsm_tch_f_l1.h"))
    inputs.extend(sorted((args.rompath / "tms54rom4").glob("*.bin")))
    evidence = {"command": command, "bios": bios,
                "inputs": [file_identity(path) for path in inputs],
                "handset_audio_claim": False}
    (run / "evidence.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print(f"ROM4 helper evidence: {run}", flush=True)
    with (run / "console.log").open("w") as output:
        result = subprocess.run(command, cwd=run, stdout=output, stderr=subprocess.STDOUT)
    text = (run / "console.log").read_text()
    print(text, end="")
    try:
        check_result(text, result.returncode, args.packed, args.fr_packing, args.fr_decoder, args.fr_encoder, args.efr_crc, args.efr_packing)
    except ValueError as error:
        print(f"ROM4 helper: FAIL: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

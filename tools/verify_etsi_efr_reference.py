#!/usr/bin/env python3
"""Build the external ETSI EFR reference and verify its installation vectors.

No reference implementation or speech vectors are imported into MAME sources.
"""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile


ARCHIVE_SHA256 = "54f4bcba0fbcc3fe1e42e0fb8de2c7f8bb3ac811cf08f1db9b6e40d38c91894d"
SPECIFICATION_URL = "https://www.etsi.org/deliver/etsi_ts/146000_146099/146053/09.00.00_60/ts_146053v090000p.pdf"
MIRROR_URL = "https://www.freecalypso.org/pub/GSM/codecs/ts_146053v080000p0.zip"
CFLAGS = "-std=gnu89 -O2 -fwrapv -DWMOPS=0"
EXPECTED = {
    "cod": (80196, "506f476855c71e91e7180804d030c1bfbf146d762582e0de72f331c6629872cd"),
    "dec": (80522, "c3abf186ad28c28d8aa71737cf10a1d54fb2148b8c1ff1f2739cb9b92c139e68"),
    "out": (52160, "738af1ead354b8960504bd7d9656d53c1a11880051d01e79b97c276f739c187a"),
}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def validate_members(names):
    """This specific archive is flat; reject paths and lowercase collisions."""
    seen = set()
    for name in names:
        if not name or "/" in name or "\\" in name or name in (".", ".."):
            raise ValueError("reference archive requires flat filenames")
        lower = name.lower()
        if lower in seen:
            raise ValueError("reference archive has case-colliding filenames")
        seen.add(lower)


def check_vectors(directory):
    directory = Path(directory)
    results = {}
    for suffix, (length, expected_hash) in EXPECTED.items():
        expected = (directory / f"spch_dos.{suffix}").read_bytes()
        actual = (directory / f"verified.{suffix}").read_bytes()
        if len(expected) != length or digest(expected) != expected_hash:
            raise ValueError(f"reference {suffix} verification input differs")
        if actual != expected:
            raise ValueError(f"reference {suffix} output is not bit-exact")
        results[suffix] = {"bytes": len(actual), "sha256": digest(actual)}
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("--output-root", type=Path,
                        default=Path(__file__).resolve().parents[1] / "tmp")
    parser.add_argument("--fixture-dir", type=Path,
                        help="export verified decoder inputs/PCM to isolated MAME fixture ROM storage")
    args = parser.parse_args()
    evidence_path = None
    evidence = {}
    try:
        archive = args.archive.read_bytes()
        if digest(archive) != ARCHIVE_SHA256:
            raise ValueError("ETSI reference archive SHA-256 differs")
        args.output_root.mkdir(parents=True, exist_ok=True)
        run = Path(tempfile.mkdtemp(prefix="run_etsi_efr_", dir=args.output_root)).resolve()
        source = run / "source"
        source.mkdir()
        with zipfile.ZipFile(args.archive) as zipped:
            validate_members(zipped.namelist())
            for member in zipped.infolist():
                (source / member.filename.lower()).write_bytes(zipped.read(member))
        commands = [
            ["make", "-f", "makefile.gcc", f"CFLAGS={CFLAGS}", "-j4"],
            ["./coder", "spch_dos.inp", "verified.cod"],
            ["./ed_iface", "verified.cod", "verified.dec"],
            ["./decoder", "verified.dec", "verified.out"],
        ]
        evidence = {"schema": 1, "archive": str(args.archive.resolve()),
                    "archive_sha256": ARCHIVE_SHA256, "specification_url": SPECIFICATION_URL,
                    "mirror_url": MIRROR_URL, "commands": commands,
                    "native_dsp_execution_claim": False, "status": "running"}
        evidence_path = run / "evidence.json"
        evidence_path.write_text(json.dumps(evidence, indent=2) + "\n")
        print(f"ETSI EFR reference evidence: {run}", flush=True)
        with (run / "build-and-run.log").open("w") as log:
            for command in commands:
                subprocess.run(command, cwd=source, stdout=log,
                               stderr=subprocess.STDOUT, check=True)
        evidence["outputs"] = check_vectors(source)
        evidence["frames"] = evidence["outputs"]["out"]["bytes"] // 320
        evidence["unused_input_samples"] = len((source / "spch_dos.inp").read_bytes()) // 2 - evidence["frames"] * 160
        evidence["executables"] = {name: digest((source / name).read_bytes())
                                   for name in ("coder", "ed_iface", "decoder")}
        evidence["status"] = "pass"
        if args.fixture_dir is not None:
            args.fixture_dir.mkdir(parents=True, exist_ok=True)
            for suffix in ("dec", "out"):
                name = f"spch_dos.{suffix}"
                (args.fixture_dir / name).write_bytes((source / name).read_bytes())
            evidence["fixture_directory"] = str(args.fixture_dir.resolve())
        evidence_path.write_text(json.dumps(evidence, indent=2) + "\n")
    except (OSError, ValueError, zipfile.BadZipFile, subprocess.CalledProcessError) as error:
        if evidence_path is not None:
            evidence.update(status="failed", error=str(error))
            evidence_path.write_text(json.dumps(evidence, indent=2) + "\n")
        print(f"ETSI EFR reference: FAIL: {error}", file=sys.stderr)
        return 1
    print("ETSI EFR reference: PASS frames=163 encoder=exact interface=exact decoder=exact native_dsp_claim=0")
    return 0


if __name__ == "__main__":
    sys.exit(main())

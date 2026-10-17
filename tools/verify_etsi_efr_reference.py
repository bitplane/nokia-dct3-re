#!/usr/bin/env python3
"""Build the external ETSI EFR reference and verify its installation vectors.

No reference implementation or speech vectors are imported into MAME sources.
"""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import struct
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
DTX_FRAMES = 518
ERASURE_BURSTS = ((8, 1), (32, 3), (80, 8), (120, 16))
ERASURE_EXPECTED = {
    "dec": "ec37dbbd0f498b5e19b6e549b80687acafe03859c68ff5f337940a25dfb370d5",
    "out": "1c05e9951ffbe91fb25e2ddd4b349f4e563a021ebd5ad7d1e3115881ce326fc9",
}
DTX_EXPECTED = {
    "inp": "f7c76955e1f79777b6a67eab3c6cc346d13351974b40817a6765acc89739ba1d",
    "cod": "b06b7dd60d251b6fa48931ecb3e313f7b03a754bb0a0b3807be1781cd73e590b",
    "dec": "6d21b356fdeb8ff24309659a1caa8b5c6ad3f10ec49b998133fc628ffa9cb409",
    "out": "1d8a85ae33572c9e9035512fd35b59269e76a5914303b429380ebf57b3c6a385",
}


def dtx_input(speech):
    """Two complete supplied-speech segments separated by low-level noise."""
    if len(speech) != 52352:
        raise ValueError("DTX source requires the complete supplied installation input")
    complete = speech[:163 * 320]
    noise = bytearray()
    state = 0x51d
    for _ in range(192 * 160):
        state = (state * 1664525 + 1013904223) & 0xffffffff
        noise.extend(struct.pack('<h', (((state >> 24) & 15) - 8) * 8))
    return bytes(noise[:96 * 320]) + complete + bytes(noise[96 * 320:]) + complete


def inspect_dtx(coded, decoder, pcm):
    if (len(coded), len(decoder), len(pcm)) != (DTX_FRAMES * 492, DTX_FRAMES * 494, DTX_FRAMES * 320):
        raise ValueError("DTX reference frame counts differ")
    flags = []
    for frame in struct.iter_unpack("<246H", coded):
        if any(bit > 1 for bit in frame):
            raise ValueError("DTX encoder contains a nonbinary serial bit or flag")
        flags.append(frame[245])
    sid, bfi = {}, {}
    for frame in struct.iter_unpack("<247H", decoder):
        if frame[0] > 1 or frame[245] > 2 or frame[246] > 1 or any(bit > 1 for bit in frame[1:245]):
            raise ValueError("DTX decoder input contains an invalid bit or flag")
        sid[frame[245]] = sid.get(frame[245], 0) + 1
        bfi[frame[0]] = bfi.get(frame[0], 0) + 1
    resumptions = sum(a == 0 and b == 1 for a, b in zip(flags, flags[1:]))
    nonzero_cni = sum(not flag and any(pcm[i * 320:(i + 1) * 320]) for i, flag in enumerate(flags))
    if not (0 in flags and 1 in flags and resumptions >= 2 and sid.get(2, 0) and bfi.get(1, 0) and nonzero_cni):
        raise ValueError("DTX conversation did not exercise silence, SID, unusable frames and speech resumption")
    return {"frames": DTX_FRAMES, "speech_frames": sum(flags),
            "inactive_frames": flags.count(0), "speech_resumptions": resumptions,
            "sid": sid, "bfi": bfi, "nonzero_cni_frames": nonzero_cni}


def generate_dtx(source, commands, log):
    (source / "dtx.inp").write_bytes(dtx_input((source / "spch_dos.inp").read_bytes()))
    for prefix in ("dtx", "dtx_replay"):
        batch = [["./coder", "dtx.inp", f"{prefix}.cod", "dtx"],
                 ["./ed_iface", f"{prefix}.cod", f"{prefix}.dec"],
                 ["./decoder", f"{prefix}.dec", f"{prefix}.out"]]
        commands.extend(batch)
        for command in batch:
            subprocess.run(command, cwd=source, stdout=log, stderr=subprocess.STDOUT, check=True)
    outputs = {}
    for suffix in ("inp", "cod", "dec", "out"):
        data = (source / f"dtx.{suffix}").read_bytes()
        if suffix != "inp" and data != (source / f"dtx_replay.{suffix}").read_bytes():
            raise ValueError(f"DTX reference {suffix} is not deterministic across fresh processes")
        if digest(data) != DTX_EXPECTED[suffix]:
            raise ValueError(f"DTX generated {suffix} differs from its local reproducibility pin")
        outputs[suffix] = {"bytes": len(data), "sha256": digest(data)}
    coverage = inspect_dtx(*((source / f"dtx.{suffix}").read_bytes() for suffix in ("cod", "dec", "out")))
    return {"profile": "noise96-speech163-noise96-speech163",
            "outputs": outputs, "coverage": coverage, "fresh_process_replay": True,
            "published_expected_vector_claim": False}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def erasure_input(decoder):
    """Retain the installation payload/TAF and mark fixed active-speech losses."""
    if len(decoder) != 163 * 494:
        raise ValueError("erasure source requires 163 complete decoder frames")
    for frame in struct.iter_unpack('<247H', decoder):
        if frame[0] or frame[245] or frame[246] > 1 or any(bit > 1 for bit in frame[1:245]):
            raise ValueError("erasure source requires clean active speech")
    result = bytearray(decoder)
    for start, count in ERASURE_BURSTS:
        for frame in range(start, start + count):
            struct.pack_into('<H', result, frame * 494, 1)
    return bytes(result)


def inspect_erasures(decoder, pcm, clean_pcm):
    if len(pcm) != 163 * 320 or len(clean_pcm) != len(pcm):
        raise ValueError("erasure PCM requires 163 complete frames")
    if len(decoder) != 163 * 494:
        raise ValueError("erasure input requires 163 complete frames")
    expected = {f for start, count in ERASURE_BURSTS for f in range(start, start + count)}
    actual = set()
    for i, frame in enumerate(struct.iter_unpack('<247H', decoder)):
        if frame[0] > 1 or frame[245] or frame[246] > 1 or any(bit > 1 for bit in frame[1:245]):
            raise ValueError("erasure input has invalid active-speech flags or bits")
        if frame[0]:
            actual.add(i)
    if actual != expected:
        raise ValueError("erasure schedule differs")
    for start, count in ERASURE_BURSTS:
        begin, end = start * 320, (start + count) * 320
        if pcm[begin:end] == clean_pcm[begin:end] or not any(pcm[begin:end]):
            raise ValueError("erasure burst lacks nonzero concealment differing from clean speech")
    return {"frames": 163, "erased_frames": len(actual),
            "bursts": [{"start": start, "frames": count} for start, count in ERASURE_BURSTS],
            "changed_frames": sum(pcm[i:i + 320] != clean_pcm[i:i + 320]
                                  for i in range(0, len(pcm), 320))}


def generate_erasures(source, commands, log):
    (source / "erasure.dec").write_bytes(erasure_input((source / "verified.dec").read_bytes()))
    for name in ("erasure.out", "erasure_replay.out"):
        command = ["./decoder", "erasure.dec", name]
        commands.append(command)
        subprocess.run(command, cwd=source, stdout=log, stderr=subprocess.STDOUT, check=True)
    pcm = (source / "erasure.out").read_bytes()
    if pcm != (source / "erasure_replay.out").read_bytes():
        raise ValueError("erasure decoding differs across fresh processes")
    decoder = (source / "erasure.dec").read_bytes()
    coverage = inspect_erasures(decoder, pcm, (source / "verified.out").read_bytes())
    if any(digest(data) != ERASURE_EXPECTED[suffix] for suffix, data in (("dec", decoder), ("out", pcm))):
        raise ValueError("erasure reference differs from its local reproducibility pin")
    return {"coverage": coverage, "fresh_process_replay": True,
            "outputs": {suffix: {"bytes": len(data), "sha256": digest(data)}
                        for suffix, data in (("dec", decoder), ("out", pcm))},
            "published_expected_vector_claim": False}


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
                        help="export verified codec inputs/outputs to isolated MAME fixture ROM storage")
    parser.add_argument("--dtx-fixture", action="store_true",
                        help="generate a deterministic DTX conversation after installation verification")
    parser.add_argument("--erasure-fixture", action="store_true",
                        help="generate fixed active-speech BFI bursts after installation verification")
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
        if args.dtx_fixture:
            with (run / "build-and-run.log").open("a") as log:
                evidence["dtx"] = generate_dtx(source, commands, log)
        if args.erasure_fixture:
            with (run / "build-and-run.log").open("a") as log:
                evidence["erasures"] = generate_erasures(source, commands, log)
        evidence["frames"] = evidence["outputs"]["out"]["bytes"] // 320
        evidence["unused_input_samples"] = len((source / "spch_dos.inp").read_bytes()) // 2 - evidence["frames"] * 160
        evidence["executables"] = {name: digest((source / name).read_bytes())
                                   for name in ("coder", "ed_iface", "decoder")}
        evidence["status"] = "pass"
        if args.fixture_dir is not None:
            args.fixture_dir.mkdir(parents=True, exist_ok=True)
            for suffix in ("inp", "cod", "dec", "out"):
                name = f"spch_dos.{suffix}"
                (args.fixture_dir / name).write_bytes((source / name).read_bytes())
                if args.dtx_fixture:
                    name = f"dtx.{suffix}"
                    (args.fixture_dir / name).write_bytes((source / name).read_bytes())
            evidence["fixture_directory"] = str(args.fixture_dir.resolve())
            if args.erasure_fixture:
                for suffix in ("dec", "out"):
                    name = f"erasure.{suffix}"
                    (args.fixture_dir / name).write_bytes((source / name).read_bytes())
        evidence_path.write_text(json.dumps(evidence, indent=2) + "\n")
    except (OSError, ValueError, zipfile.BadZipFile, subprocess.CalledProcessError) as error:
        if evidence_path is not None:
            evidence.update(status="failed", error=str(error))
            evidence_path.write_text(json.dumps(evidence, indent=2) + "\n")
        print(f"ETSI EFR reference: FAIL: {error}", file=sys.stderr)
        return 1
    print("ETSI EFR reference: PASS frames=163 encoder=exact interface=exact decoder=exact native_dsp_claim=0")
    if args.dtx_fixture:
        print("ETSI EFR DTX reference: PASS frames=518 fresh_process_replay=1 published_vector_claim=0 native_dsp_claim=0")
    if args.erasure_fixture:
        print("ETSI EFR erasure reference: PASS frames=163 erased=28 bursts=1,3,8,16 fresh_process_replay=1 published_vector_claim=0 native_dsp_claim=0")
    return 0


if __name__ == "__main__":
    sys.exit(main())

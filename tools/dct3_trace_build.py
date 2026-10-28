"""Configure the board's firmware-trace quarantine and check linked canaries."""
import argparse
import mmap
from pathlib import Path


SWITCH = "#define DCT3_RESEARCH_TRACES 0"
PROBES = (b"rom4_validator: event=entry", b"nhm6_security_verify: stage=compare",
          b"dsp_audio_shadow_write: address=")


def configure(template, output, enabled):
    text = template.read_text()
    if text.splitlines().count(SWITCH) != 1:
        raise ValueError("trace template must have one default-disabled switch")
    text = text.replace(SWITCH, f"#define DCT3_RESEARCH_TRACES {int(enabled)}")
    if not output.exists() or output.read_text() != text:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(text)


def check(binary, enabled):
    if not binary.stat().st_size:
        raise ValueError("empty executable")
    with binary.open("rb") as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as data:
        for probe in PROBES:
            if (data.find(probe) >= 0) != enabled:
                raise ValueError(f"unexpected linked trace canary: {probe.decode()}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_subparsers(dest="mode", required=True)
    config = modes.add_parser("configure")
    config.add_argument("template", type=Path)
    config.add_argument("output", type=Path)
    config.add_argument("--enabled", choices=("0", "1"), required=True)
    verify = modes.add_parser("check")
    verify.add_argument("binary", type=Path)
    verify.add_argument("--enabled", choices=("0", "1"), required=True)
    args = parser.parse_args()
    try:
        enabled = args.enabled == "1"
        if args.mode == "configure":
            configure(args.template, args.output, enabled)
        else:
            check(args.binary, enabled)
            print(f"DCT3 trace link check: PASS enabled={int(enabled)} canaries={len(PROBES)}")
    except (OSError, ValueError) as error:
        parser.exit(1, f"DCT3 trace build FAIL: {error}\n")


if __name__ == "__main__":
    main()

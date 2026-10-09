"""Build/run unchanged external gDSPsim 0.30 CPU sources with GUI-only glue."""
import argparse
import os
from pathlib import Path
import re
import shlex
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="Extracted gdspsim-0.30 directory")
    parser.add_argument("output", type=Path, help="Isolated build/evidence directory")
    args = parser.parse_args()
    source = args.source.resolve()
    output = args.output.resolve()
    adapter = Path(__file__).resolve().parent
    output.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, TMPDIR=str(output))
    cflags = shlex.split(subprocess.check_output(
        ["pkg-config", "--cflags", "glib-2.0"], text=True))
    libs = shlex.split(subprocess.check_output(
        ["pkg-config", "--libs", "glib-2.0"], text=True))
    flags = ["-std=gnu89", "-fcommon", "-Wno-implicit-function-declaration",
             "-D_C54", "-I" + str(adapter / "include"),
             "-I" + str(source / "c54"), "-I" + str(source / "gdspsim"), *cflags]
    names = re.findall(r"^\s*([A-Za-z0-9_]+)\.o\s*(?:\\)?\s*$",
                       (source / "c54/Makefile").read_text(), re.M)
    excluded = {"decode_window", "entryCB", "fileIO", "gtkbox_add", "main",
                "memory_window", "plot_window", "preferences", "readfile",
                "register_window"}
    objects = []
    for name in names:
        if name in excluded:
            continue
        path = source / "c54" / (name + ".c")
        if not path.exists():
            path = source / "gdspsim" / (name + ".c")
        obj = output / (name + ".o")
        subprocess.run(["gcc", *flags, "-c", str(path), "-o", str(obj)],
                       env=env, check=True, capture_output=True, text=True)
        objects.append(str(obj))
    binary = output / "headless"
    subprocess.run(["gcc", *flags, str(adapter / "headless.c"), *objects,
                    *libs, "-lm", "-o", str(binary)], env=env, check=True)
    for name, argv in (("nop", []), ("xf", ["xf"])):
        result = subprocess.run([str(binary), *argv], env=env, text=True,
                                capture_output=True)
        (output / (name + ".log")).write_text(result.stdout + result.stderr)
        print(result.stdout, end="")
        if result.returncode or "Can't process" in result.stdout + result.stderr:
            raise SystemExit(f"Reference fixture failed: {name}; inspect {output}")
    print(f"PASS external_objects={len(objects)}; no timer/IRQ validation")


if __name__ == "__main__":
    main()

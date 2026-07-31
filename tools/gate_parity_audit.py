#!/usr/bin/env python3
"""Report where sibling product gates assert different things.

The acceptance gates were written per product by copying a sibling and editing
it. That is why 27 capability families exist in two or three near-identical
copies. It also means a family can drift: one product's gate may check fewer
predicates than its sibling without anything recording that the difference was
intended.

This audit reads the extracted gate matrix and, for each capability family,
reports the differences between products in:

- which checker scripts run;
- which options each shared checker is given;
- the emulated run duration; and
- prerequisites and target-specific variables.

It reports differences, not verdicts. A difference may be a deliberate
consequence of a product's own recovered contract — NHM-2 genuinely has its own
lifecycle checker — or it may be an editing accident. Deciding which is a human
step, and this report exists to make that decision possible at all.
"""

from __future__ import annotations

import argparse
import collections
import json
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import gate_matrix


# The unprefixed gates are the Nokia 3210 NSE-8 originals; the others carry an
# explicit product token. 6110 is static-only and has no sibling families.
PRODUCT_RE = re.compile(r"^verify-(3210|3310|3330|3410|6110)-(.+)$")
DEFAULT_PRODUCT = "3210"

# Gates that belong to no product: the native codec and protocol unit tests,
# which compile and run host code rather than a handset. Product bring-up gates
# such as `verify-6110-static` are not neutral; 6110 is a product that
# currently has one family, which the membership view reports as such.
NEUTRAL_PREFIXES = ("gsm-",)


def family_of(name: str) -> tuple[str, str] | None:
    """Split a gate name into (product, capability family)."""
    match = PRODUCT_RE.match(name)
    if match:
        return match.group(1), match.group(2)
    body = name[len("verify-"):] if name.startswith("verify-") else name
    if not body or body.startswith(NEUTRAL_PREFIXES):
        return None
    return DEFAULT_PRODUCT, body


def _scripts(gate: dict) -> set[str]:
    return set(gate.get("scripts_mentioned", []))


def _seconds(gate: dict) -> set[str]:
    values = set()
    for step in gate["steps"]:
        if step["step"] == "run" and "SECONDS" in step["parameters"]:
            values.add(step["parameters"]["SECONDS"])
    return values


def _options_by_script(gate: dict) -> dict[str, set[str]]:
    options: dict[str, set[str]] = collections.defaultdict(set)
    for step in gate["steps"]:
        if step["step"] == "check":
            options[step["script"]].update(step["options"])
    return options


def build_families(matrix: dict) -> dict[str, dict[str, dict]]:
    families: dict[str, dict[str, dict]] = collections.defaultdict(dict)
    for gate in matrix["gates"]:
        split = family_of(gate["name"])
        if split is None:
            continue
        product, family = split
        families[family][product] = gate
    return families


MAKE_TARGET_RE = re.compile(r"\$\(MAKE\)[^\n]*?\b((?:verify|normalize)-[a-z0-9-]+)")


def _dependencies(gate: dict) -> set[str]:
    """Targets this gate reaches directly, however it names them."""
    targets = set(gate["prerequisites"])
    for step in gate["steps"]:
        if step["step"] == "delegate":
            targets.add(step["target"])
    # A control-flow recipe is not decomposed, but its sub-make calls are still
    # plain text and must be followed or the reachability answer is wrong.
    recipe = "\n".join(gate.get("verbatim", {}).get("recipe", []))
    targets.update(MAKE_TARGET_RE.findall(recipe))
    return targets


ROM_PATH_RE = re.compile(r"roms/(nok[a-z0-9]+)/([^\s'\"\\]+)")
PHONE_RE = re.compile(r"PHONE=(nok[a-z0-9]+)")
OUTPUT_RE = re.compile(r"--(?:flash|eeprom)-output\s+\"?(roms/[^\s\"\\]+)")


def _normalisation_outputs(text: str) -> dict[str, tuple[set[str], set[str]]]:
    """Map each `normalize-*` target to the files and ROM sets it produces.

    Derived from the target's own recipe rather than assumed from its name, so
    a product with two ROM families — the 6110 has v4.06 and v5.48 — maps to
    the specific step that builds each one.
    """
    outputs: dict[str, tuple[set[str], set[str]]] = {}
    current: str | None = None
    body: list[str] = []
    for line in text.split("\n"):
        match = re.match(r"^(normalize-[a-z0-9-]+)\s*:", line)
        if match:
            if current:
                outputs[current] = _classify_outputs(body)
            current, body = match.group(1), []
            continue
        if current is not None:
            if line.startswith("\t"):
                body.append(line)
            else:
                outputs[current] = _classify_outputs(body)
                current, body = None, []
    if current:
        outputs[current] = _classify_outputs(body)
    return outputs


def _classify_outputs(body: list[str]) -> tuple[set[str], set[str]]:
    """Files and ROM sets a normalisation target *writes*.

    Only destinations count. These recipes also read from other products —
    `normalize-3410` copies the shared boot and DSP ROMs out of `roms/noki3210`
    — and treating a source as an output would make every 3210 gate appear to
    depend on the 3410 normalisation.
    """
    text = "\n".join(body)
    files = set(OUTPUT_RE.findall(text))
    directories = {path.split("/")[1] for path in files if path.startswith("roms/")}
    for line in body:
        stripped = line.strip().rstrip("\\").strip()
        if not stripped.startswith("cp "):
            continue
        destination = stripped.split()[-1]
        match = re.match(r"roms/(nok[a-z0-9]+)/?$", destination)
        if match:
            directories.add(match.group(1))
    return files, directories


def normalisation_gaps(matrix: dict, text: str) -> dict[str, list[str]]:
    """Gates that never reach the normalisation step building the ROMs they use.

    `normalize-*` extracts flash and PMM images that are not in the repository.
    A gate that consumes one without reaching its producer passes only when an
    earlier invocation happened to leave the images behind, so it is
    order-dependent and fails on a clean tree.
    """
    gates = {gate["name"]: gate for gate in matrix["gates"]}
    produces = _normalisation_outputs(text)

    def reaches(name: str, target: str, seen: set[str] | None = None) -> bool:
        if seen is None:
            seen = set()
        if name in seen:
            return False
        seen.add(name)
        gate = gates.get(name)
        if gate is None:
            return name == target
        return any(dependency == target or reaches(dependency, target, seen)
                   for dependency in _dependencies(gate))

    gaps: dict[str, list[str]] = {}
    for name, gate in gates.items():
        recipe = "\n".join(gate.get("verbatim", {}).get("recipe", []))
        needed_files = {f"roms/{a}/{b}" for a, b in ROM_PATH_RE.findall(recipe)}
        needed_sets = set(PHONE_RE.findall(recipe))

        required: set[str] = set()
        for step, (files, directories) in produces.items():
            if needed_files & files or needed_sets & directories:
                required.add(step)
        # A gate needing a product's ROM set, where exactly one step builds it,
        # must reach that step. Where several could, reaching any one is enough.
        for step in sorted(required):
            if any(reaches(name, candidate) for candidate in required):
                break
            gaps.setdefault(step, []).append(name)
    return {step: sorted(names) for step, names in sorted(gaps.items())}


def audit(matrix: dict, source_text: str = "") -> dict:
    families = build_families(matrix)
    findings: list[dict] = []

    for family, members in sorted(families.items()):
        if len(members) < 2:
            continue
        products = sorted(members)

        # Checker sets.
        script_sets = {p: _scripts(members[p]) for p in products}
        shared = set.intersection(*script_sets.values())
        union = set.union(*script_sets.values())
        if shared != union:
            findings.append({
                "family": family,
                "kind": "checker_set",
                "detail": {p: sorted(script_sets[p] - shared) for p in products},
                "shared": sorted(shared),
            })

        # Checker options and run duration are read from decomposed steps only.
        # A shell-loop gate hides both, so comparing a structured member against
        # an unstructured one manufactures differences that do not exist: the
        # option is usually there, inside the loop. Those families are reported
        # as not comparable instead, which is the truthful answer.
        opaque = [p for p in products if "unstructured" in members[p]]
        if opaque:
            findings.append({
                "family": family,
                "kind": "not_comparable",
                "detail": {p: ("shell" if p in opaque else "structured")
                           for p in products},
            })
        else:
            options = {p: _options_by_script(members[p]) for p in products}
            for script in sorted(shared):
                per_product = {p: options[p].get(script, set()) for p in products}
                if not any(per_product.values()):
                    continue
                if len({frozenset(v) for v in per_product.values()}) > 1:
                    common = set.intersection(*per_product.values())
                    findings.append({
                        "family": family,
                        "kind": "checker_options",
                        "script": script,
                        "detail": {p: sorted(per_product[p] - common)
                                   for p in products},
                        "shared": sorted(common),
                    })

            seconds = {p: sorted(_seconds(members[p])) for p in products}
            if len({tuple(v) for v in seconds.values()}) > 1 and any(seconds.values()):
                findings.append({
                    "family": family, "kind": "seconds", "detail": seconds,
                })

        # Prerequisites, excluding each product's own ROM normalisation step.
        # `normalize-3330` and `normalize-3410` extract that product's flash and
        # PMM images from its WinTesla package; 3210 and 3310 need no such step
        # and have no such target. Comparing them directly would report an
        # intrinsic product property as drift in every family forever. Whether
        # each gate reaches its own normalisation is a separate, real check —
        # see `normalisation_gaps` below.
        prerequisites = {
            p: sorted(item for item in members[p]["prerequisites"]
                      if item != f"normalize-{p}")
            for p in products
        }
        if len({tuple(v) for v in prerequisites.values()}) > 1:
            findings.append({
                "family": family, "kind": "prerequisites", "detail": prerequisites,
            })

    # Membership is a separate question from parity. A family that two products
    # share and a third does not may be an untaken coverage step rather than a
    # drifted gate, so it is reported apart from the difference findings.
    products = sorted({p for members in families.values() for p in members})
    membership = {
        family: {p: (p in members) for p in products}
        for family, members in sorted(families.items())
        if len(members) > 1
    }

    return {
        "products": products,
        "normalisation_gaps": normalisation_gaps(matrix, source_text),
        "families": {f: sorted(m) for f, m in sorted(families.items())},
        "multi_product_families": sorted(f for f, m in families.items() if len(m) > 1),
        "membership": membership,
        "findings": findings,
    }


SEVERITY_ORDER = ["checker_set", "checker_options", "prerequisites", "seconds",
                  "not_comparable"]


def render_report(result: dict) -> str:
    lines = ["# Acceptance-gate parity audit", ""]
    lines += [
        "Generated by `tools/gate_parity_audit.py` from the extracted gate",
        "matrix. Each entry is a difference between sibling product gates in the",
        "same capability family. A difference is not automatically a defect: a",
        "product with its own recovered contract legitimately runs its own",
        "checker. Entries are for adjudication, not automatic correction.",
        "",
        f"Families with more than one product: {len(result['multi_product_families'])}",
        f"Differences found: {len(result['findings'])}",
        "",
    ]

    gaps = result["normalisation_gaps"]
    lines += ["## ROM normalisation reachability", ""]
    if gaps:
        lines += ["A gate listed here never reaches its product's ROM",
                  "normalisation step, so it passes only when an earlier",
                  "invocation left the extracted images behind.", ""]
        for step, names in gaps.items():
            lines.append(f"- **{step}** not reached by:")
            lines += [f"  - `{name}`" for name in names]
        lines.append("")
    else:
        lines += ["Every product gate reaches its own ROM normalisation step.",
                  ""]

    products = result["products"]
    lines += ["## Family membership", "",
              "Which products have a gate in each shared capability family. An",
              "absent product is a coverage question, not a drifted gate.", "",
              "| Family | " + " | ".join(products) + " |",
              "| --- | " + " | ".join("---" for _ in products) + " |"]
    for family, present in result["membership"].items():
        cells = " | ".join("yes" if present[p] else "—" for p in products)
        lines.append(f"| `{family}` | {cells} |")
    lines.append("")

    by_kind = collections.defaultdict(list)
    for finding in result["findings"]:
        by_kind[finding["kind"]].append(finding)

    titles = {
        "checker_set": "Different checker scripts",
        "checker_options": "Same checker, different options",
        "prerequisites": "Different prerequisites",
        "seconds": "Different run duration",
        "not_comparable": "Not comparable — a member hides its steps in shell",
    }

    for kind in SEVERITY_ORDER:
        entries = by_kind.get(kind)
        if not entries:
            continue
        lines += [f"## {titles[kind]}", ""]
        for finding in entries:
            head = f"### `{finding['family']}`"
            if "script" in finding:
                head += f" — `{finding['script']}`"
            lines += [head, ""]
            for product, value in sorted(finding["detail"].items()):
                shown = ", ".join(f"`{v}`" for v in value) if isinstance(value, list) else f"`{value}`"
                lines.append(f"- **{product}**: {shown or '_(none)_'}")
            if finding.get("shared"):
                lines.append(f"- shared: {', '.join(f'`{v}`' for v in finding['shared'])}")
            lines.append("")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--makefile", type=pathlib.Path, default=None)
    parser.add_argument("--json", type=pathlib.Path)
    parser.add_argument("--report", type=pathlib.Path)
    arguments = parser.parse_args()

    source = arguments.makefile or gate_matrix.gate_source()
    matrix = gate_matrix.check_round_trip(source.read_text())
    result = audit(matrix, source.read_text() + "\n"
                   + gate_matrix.MAKEFILE.read_text())

    if arguments.json:
        arguments.json.parent.mkdir(parents=True, exist_ok=True)
        arguments.json.write_text(json.dumps(result, indent=2) + "\n")
    if arguments.report:
        arguments.report.parent.mkdir(parents=True, exist_ok=True)
        arguments.report.write_text(render_report(result) + "\n")

    counts = collections.Counter(f["kind"] for f in result["findings"])
    print(f"gate parity: {len(result['multi_product_families'])} multi-product "
          f"families, {len(result['findings'])} differences")
    for step, names in result["normalisation_gaps"].items():
        print(f"  {len(names):3d}  gates never reach {step}")
    for kind in SEVERITY_ORDER:
        if counts.get(kind):
            print(f"  {counts[kind]:3d}  {kind}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

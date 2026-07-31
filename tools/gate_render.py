#!/usr/bin/env python3
"""Render extracted gate records back into the commands they represent.

Phase 1 of the gate-matrix migration. Extraction alone proves nothing about
whether the typed step records are *complete*: a field silently dropped during
parsing would never be noticed while the Makefile remains the source of truth.

This module closes that loop. It reconstructs each gate's command sequence from
its typed steps only — never from the retained verbatim text — and the check
below requires the result to equal the commands the Makefile actually specifies.
A gate that survives this is one whose behaviour is fully captured by data, and
can therefore be generated rather than hand-written.

Comparison is on logical commands, not on Makefile bytes. Line wrapping and
indentation are presentation; which commands run, in which order, with which
arguments, is behaviour.
"""

from __future__ import annotations

import argparse
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import gate_matrix


class RenderMismatch(AssertionError):
    """A gate's typed steps do not reproduce its Makefile commands."""


def render_step(step: dict) -> str:
    """Reconstruct one command from a typed step."""
    prefix = "@" if step.get("silent") else ""
    kind = step["step"]

    if kind == "run":
        parts = ["$(MAKE)", *step["make_flags"], step["run_target"],
                 *step["tokens"]]
        return prefix + " ".join(parts)

    if kind == "check":
        parts = [step["interpreter"], *step["before_script"], step["script"],
                 *step["arguments"]]
        return prefix + " ".join(parts)

    # Every other step kind is retained as its exact command text. These are
    # steps whose internals the matrix does not model — shell fragments, native
    # compiles, announcements — so reproducing them is exact by construction
    # and claims nothing about understanding their contents.
    return prefix + step["command"]


def render_gate(gate: dict) -> list[str]:
    """The full command sequence a gate's typed steps describe."""
    return [render_step(step) for step in gate["steps"]]


def original_commands(gate: dict) -> list[str]:
    """The commands the Makefile specifies for this gate."""
    return [command for command
            in gate_matrix._logical_commands(gate["verbatim"]["recipe"])
            if command]


def check_gate(gate: dict) -> list[tuple[str, str]]:
    """Return the (expected, rendered) pairs that differ for one gate."""
    expected = original_commands(gate)
    rendered = render_gate(gate)
    if expected == rendered:
        return []
    differences: list[tuple[str, str]] = []
    for left, right in zip(expected, rendered):
        if left != right:
            differences.append((left, right))
    if len(expected) != len(rendered):
        differences.append((f"<{len(expected)} commands>",
                            f"<{len(rendered)} commands>"))
    return differences


def check_all(matrix: dict) -> dict:
    """Verify every structured gate renders back to its own commands."""
    structured = [gate for gate in matrix["gates"] if "unstructured" not in gate]
    failures: dict[str, list[tuple[str, str]]] = {}
    for gate in structured:
        differences = check_gate(gate)
        if differences:
            failures[gate["name"]] = differences
    return {
        "structured": len(structured),
        "unstructured": len(matrix["gates"]) - len(structured),
        "reproduced": len(structured) - len(failures),
        "failures": failures,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--makefile", type=pathlib.Path, default=None)
    parser.add_argument("--verbose", action="store_true")
    arguments = parser.parse_args()

    source = arguments.makefile or gate_matrix.gate_source()
    matrix = gate_matrix.check_round_trip(source.read_text())
    result = check_all(matrix)

    if result["failures"]:
        print(f"FAIL - {len(result['failures'])} of {result['structured']} "
              f"structured gates do not render back to their own commands")
        for name, differences in sorted(result["failures"].items()):
            print(f"\n  {name}")
            for expected, rendered in differences[:3 if not arguments.verbose else 99]:
                print(f"    makefile: {expected}")
                print(f"    rendered: {rendered}")
        return 1

    print(f"gate render: {result['reproduced']}/{result['structured']} structured "
          f"gates reproduce their commands from typed data "
          f"({result['unstructured']} still shell-only)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

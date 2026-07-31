#!/usr/bin/env python3
"""Compare what make itself resolves for every gate, before and after a change.

`gate_generate.py` proves the generated rules specify the same commands by
re-parsing them with this project's own extractor. That is a check of the
extractor against itself. This tool asks make instead.

`make -p` prints the rule database: for every target, its prerequisites,
whether it is phony, and the recipe make would run. Comparing that database
before and after the migration tests the parts the extractor cannot vouch for —
include placement, `.PHONY` membership, target-specific variables and variable
scoping — using make as the authority.

`make -n` is deliberately not used: it recurses into the MAME sub-build, whose
parallel output ordering is not deterministic and swamps the signal.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys


TARGET_RE = re.compile(r"^([a-zA-Z0-9_./-]+):(.*)$")


def parse_database(text: str) -> dict[str, dict]:
    """Extract each target's prerequisites and recipe from `make -p` output."""
    targets: dict[str, dict] = {}
    lines = text.split("\n")
    index = 0
    while index < len(lines):
        match = TARGET_RE.match(lines[index])
        if not match or lines[index].startswith("\t"):
            index += 1
            continue
        name = match.group(1)
        prerequisites = match.group(2).split()
        index += 1
        recipe: list[str] = []
        phony = False
        while index < len(lines):
            line = lines[index]
            if line.startswith("\t"):
                recipe.append(line)
            elif line.startswith("#"):
                if "Phony target" in line:
                    phony = True
            elif not line.strip():
                break
            else:
                break
            index += 1
        # A target may be declared more than once; keep the entry carrying the
        # recipe so a bare `.PHONY` mention cannot erase it.
        if name in targets and not recipe:
            continue
        targets[name] = {
            "prerequisites": prerequisites,
            "recipe": recipe,
            "phony": phony,
        }
    return targets


def logical_commands(recipe: list[str]) -> list[str]:
    """Join continuation lines into the commands make hands to the shell.

    Make passes each logical command to one shell invocation, joining backslash
    continuations first. Comparing raw recipe lines would therefore report a
    pure re-wrapping as a difference; comparing the joined commands reports
    only what changes execution.
    """
    commands: list[str] = []
    pending: list[str] = []
    for line in recipe:
        body = line[1:] if line.startswith("\t") else line
        if body.endswith("\\"):
            pending.append(body[:-1].strip())
            continue
        pending.append(body.strip())
        joined = " ".join(part for part in pending if part)
        if joined:
            commands.append(joined)
        pending = []
    if pending:
        joined = " ".join(part for part in pending if part)
        if joined:
            commands.append(joined)
    return commands


VARIABLE_DEF_RE = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)\s*[:?+]?=\s?(.*)$")
VARIABLE_USE_RE = re.compile(r"\$\(([A-Za-z_][A-Za-z0-9_]*)\)")


def parse_variables(text: str) -> dict[str, str]:
    """Collect variable values from the `# Variables` section of `make -p`."""
    variables: dict[str, str] = {}
    for line in text.split("\n"):
        if line.startswith("\t") or line.startswith("#"):
            continue
        match = VARIABLE_DEF_RE.match(line)
        if match:
            variables.setdefault(match.group(1), match.group(2))
    return variables


def expand(command: str, variables: dict[str, str], depth: int = 12) -> str:
    """Expand plain `$(NAME)` references using make's own variable values.

    Only simple references are expanded. A function call such as `$(call ...)`
    or `$(if ...)` is not evaluated, though plain references nested inside one
    are. That is sufficient for comparison: both sides are treated the same
    way, so an unevaluated function reads identically before and after.
    Evaluating make's functions properly would add risk without adding signal.
    An unknown name is likewise left as written.
    """
    for _ in range(depth):
        def substitute(match: re.Match) -> str:
            name = match.group(1)
            return variables.get(name, match.group(0))

        expanded = VARIABLE_USE_RE.sub(substitute, command)
        if expanded == command:
            return expanded
        command = expanded
    return command


def compare(before: dict[str, dict], after: dict[str, dict],
            prefix: str = "verify",
            variables_before: dict[str, str] | None = None,
            variables_after: dict[str, str] | None = None) -> dict:
    names_before = {n for n in before if n.startswith(prefix)}
    names_after = {n for n in after if n.startswith(prefix)}

    problems: dict[str, list[str]] = {}
    for name in sorted(names_before - names_after):
        problems[name] = ["missing after the change"]
    for name in sorted(names_after - names_before):
        problems[name] = ["unexpectedly added"]

    for name in sorted(names_before & names_after):
        issues = []
        commands_before = logical_commands(before[name]["recipe"])
        commands_after = logical_commands(after[name]["recipe"])
        if variables_before is not None and variables_after is not None:
            commands_before = [expand(c, variables_before) for c in commands_before]
            commands_after = [expand(c, variables_after) for c in commands_after]
        if commands_before != commands_after:
            for left, right in zip(commands_before, commands_after):
                if left != right:
                    issues.append(f"command differs:\n      before: {left}\n      after:  {right}")
                    break
            if len(commands_before) != len(commands_after):
                issues.append(f"command count {len(commands_before)} -> {len(commands_after)}")
        if sorted(before[name]["prerequisites"]) != sorted(after[name]["prerequisites"]):
            issues.append(
                f"prerequisites differ: {before[name]['prerequisites']} -> "
                f"{after[name]['prerequisites']}")
        if before[name]["phony"] != after[name]["phony"]:
            issues.append(f"phony differs: {before[name]['phony']} -> "
                          f"{after[name]['phony']}")
        if issues:
            problems[name] = issues

    return {
        "compared": len(names_before & names_after),
        "before_only": sorted(names_before - names_after),
        "after_only": sorted(names_after - names_before),
        "problems": problems,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("before", type=pathlib.Path)
    parser.add_argument("after", type=pathlib.Path)
    parser.add_argument("--prefix", default="verify")
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument("--expand", action="store_true",
                        help="compare commands after expanding make variables, "
                             "so naming a repeated literal is not a difference")
    arguments = parser.parse_args()

    before_text = arguments.before.read_text()
    after_text = arguments.after.read_text()
    result = compare(
        parse_database(before_text), parse_database(after_text),
        arguments.prefix,
        parse_variables(before_text) if arguments.expand else None,
        parse_variables(after_text) if arguments.expand else None)

    if result["problems"]:
        print(f"FAIL - {len(result['problems'])} targets differ in make's own "
              f"rule database")
        for name, issues in list(result["problems"].items())[:15]:
            print(f"    {name}: {'; '.join(issues)}")
        return 1

    scope = "expanded commands" if arguments.expand else "recipe text"
    print(f"gate database: {result['compared']} targets identical in make's rule "
          f"database ({scope}, prerequisites and phony status)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

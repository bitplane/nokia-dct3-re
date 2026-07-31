#!/usr/bin/env python3
"""Extract the acceptance-gate matrix from the Makefile as reviewable data.

Phase 0 of the gate-matrix migration. This module does not change how any gate
runs. It reads the Makefile, represents every `verify-*` target as a record,
and renders those records back to Makefile text. The extraction is trusted only
while the rendered text reproduces the original file byte for byte, so the
round-trip assertion below is the contract, not a convenience.

Two levels of parsing are layered:

- Every gate is captured verbatim, which makes the byte-exact round-trip
  unconditional.
- Recipes that match the ordinary `run -> capture log -> check` shape are
  additionally decomposed into typed steps. That decomposition is what the
  parity audit reads. A gate whose recipe embeds shell control flow keeps only
  its verbatim form and is reported as unstructured rather than guessed at.

Nothing here infers intent. A missing checker argument is recorded as missing;
deciding whether that is deliberate is a separate, human step.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys


ROOT = pathlib.Path(__file__).resolve().parent.parent
MAKEFILE = ROOT / "Makefile"
GATES_MK = ROOT / "gates.mk"


def gate_source() -> pathlib.Path:
    """The file the gate rules currently live in.

    They moved out of the Makefile into the generated `gates.mk`. Preferring
    `gates.mk` when it exists keeps every check working on either side of that
    migration rather than silently reporting zero gates.
    """
    return GATES_MK if GATES_MK.exists() else MAKEFILE

TARGET_RE = re.compile(r"^(verify[a-z0-9-]*)\s*:(?!=)(.*)$")
VAR_ASSIGN_RE = re.compile(r"^([A-Z_][A-Z0-9_]*)\s*=\s*(.*)$")

# Recipe lines that embed shell control flow are not decomposed. Matching them
# is deliberately broad: an unstructured gate is a known gap, while a
# mis-parsed one would silently corrupt the audit.
# A recipe referencing a named shell guard is still shell: the guard expands
# to a `trap` and the rest of the recipe is one `;`-chained command. Without
# this the blob would be mistaken for a single run step.
SHELL_CONTROL_RE = re.compile(
    r"\bfor\s+\w+\s+in\b|\bcase\s+\"|\btrap\b|\bwhile\b|\bif\s+\[|\bset\s+-e\b"
    r"|\$\(DCT3_EEPROM_GUARD")

RUN_TARGETS = ("run-prebuilt-captured", "run-prebuilt", "run-captured",
               "run-frontier", "run")


class RoundTripError(AssertionError):
    """The rendered Makefile did not reproduce the original text."""


def _split_segments(text: str) -> list[dict]:
    """Split the Makefile into interstitial text and gate blocks.

    A gate block is the run of consecutive declaration lines for one target
    plus the recipe lines that follow. Everything else — comments, variables,
    other targets — is preserved verbatim as interstitial text so that
    rendering is a plain concatenation.
    """
    lines = text.split("\n")
    segments: list[dict] = []
    buffer: list[str] = []
    index = 0

    while index < len(lines):
        match = TARGET_RE.match(lines[index])
        if not match:
            buffer.append(lines[index])
            index += 1
            continue

        name = match.group(1)
        declarations: list[str] = []
        # Consecutive declaration lines for the same target belong to one gate;
        # a declaration for a different target starts a new block.
        while index < len(lines):
            head = TARGET_RE.match(lines[index])
            if not head or head.group(1) != name:
                break
            declarations.append(lines[index])
            index += 1

        recipe: list[str] = []
        while index < len(lines) and lines[index].startswith("\t"):
            recipe.append(lines[index])
            index += 1

        if buffer:
            segments.append({"kind": "text", "lines": buffer})
            buffer = []
        segments.append({
            "kind": "gate",
            "name": name,
            "declarations": declarations,
            "recipe": recipe,
        })

    if buffer:
        segments.append({"kind": "text", "lines": buffer})
    return segments


def _logical_commands(recipe: list[str]) -> list[str]:
    """Join make line continuations into logical commands."""
    commands: list[str] = []
    pending: list[str] = []
    for line in recipe:
        body = line[1:] if line.startswith("\t") else line
        if body.endswith("\\"):
            pending.append(body[:-1].strip())
            continue
        pending.append(body.strip())
        commands.append(" ".join(part for part in pending if part))
        pending = []
    if pending:
        commands.append(" ".join(part for part in pending if part))
    return commands


def _split_arguments(command: str) -> list[str]:
    """Split a command on whitespace, keeping quoted runs and `$(...)` intact.

    A make expansion is one token however much whitespace or quoting it
    contains: `$(if $(RAW_TRACE),--raw-trace "$(RAW_TRACE)")` is a single
    conditional argument, and splitting inside it would silently reshape the
    command when it is rendered back.
    """
    tokens: list[str] = []
    current: list[str] = []
    quote: str | None = None
    depth = 0
    index = 0
    while index < len(command):
        character = command[index]
        if quote:
            current.append(character)
            if character == quote:
                quote = None
            index += 1
            continue
        if character in "'\"":
            quote = character
            current.append(character)
            index += 1
            continue
        if character == "$" and command[index:index + 2] == "$(":
            depth += 1
            current.append("$(")
            index += 2
            continue
        if depth:
            if character == "(":
                depth += 1
            elif character == ")":
                depth -= 1
            current.append(character)
            index += 1
            continue
        if character.isspace():
            if current:
                tokens.append("".join(current))
                current = []
            index += 1
            continue
        current.append(character)
        index += 1
    if current:
        tokens.append("".join(current))
    return tokens


def _parse_run(command: str) -> dict | None:
    body = command.lstrip("@")
    if "$(MAKE)" not in body:
        return None
    tokens = _split_arguments(body)
    target = next((token for token in tokens if token in RUN_TARGETS), None)
    if target is None:
        return None
    index = tokens.index(target)
    # Tokens are kept in order rather than as a bare mapping: some gates pass a
    # variable that expands to several assignments, such as `$(NOKI3410_RUN)`,
    # and its position matters to what later assignments override.
    tokens_before = tokens[1:index]
    tokens_after = tokens[index + 1:]
    parameters: dict[str, str] = {}
    for token in tokens_after:
        assignment = re.match(r"^([A-Z_][A-Z0-9_]*)=(.*)$", token)
        if assignment:
            parameters[assignment.group(1)] = assignment.group(2)
    return {
        "step": "run",
        "silent": command.startswith("@"),
        "make_flags": tokens_before,
        "run_target": target,
        "tokens": tokens_after,
        "parameters": parameters,
    }


def _parse_check(command: str) -> dict | None:
    body = command.lstrip("@")
    interpreter = re.match(r"^(\$\(PYTHON\)|\$\(VENV\)/bin/python)", body)
    if not interpreter:
        return None
    tokens = _split_arguments(body)
    script = next((token for token in tokens if token.startswith("tools/")), None)
    if script is None:
        return None
    index = tokens.index(script)
    arguments = tokens[index + 1:]
    return {
        "step": "check",
        "silent": command.startswith("@"),
        "interpreter": interpreter.group(1),
        "before_script": tokens[1:index],
        "script": script,
        "arguments": arguments,
        "options": sorted(token for token in arguments if token.startswith("--")),
    }


def _parse_capture(command: str) -> dict | None:
    body = command.lstrip("@")
    if body.startswith("cp ") and "error.log" in body:
        return {"step": "capture_log", "silent": command.startswith("@"),
                "command": body}
    return None


def _parse_delegate(command: str) -> dict | None:
    """A `$(MAKE)` call that runs another gate rather than a phone session."""
    body = command.lstrip("@")
    if "$(MAKE)" not in body:
        return None
    tokens = _split_arguments(body)
    target = next(
        (token for token in tokens
         if token.startswith("verify") or token in ("eeprom-profile", "build",
                                                    "smoke", "normalize-3330",
                                                    "normalize-3410")),
        None)
    if target is None:
        return None
    return {"step": "delegate", "silent": command.startswith("@"),
            "target": target, "command": body}


def _parse_compile(command: str) -> dict | None:
    body = command.lstrip("@")
    if body.startswith("$(CXX)"):
        sources = [token for token in _split_arguments(body)
                   if token.endswith((".cpp", ".c"))]
        return {"step": "compile", "silent": command.startswith("@"),
                "sources": sources, "command": body}
    return None


def _parse_execute(command: str) -> dict | None:
    body = command.lstrip("@")
    if body.startswith("scratchpad/") or body.startswith("$(LIBGSM_DIR)/"):
        return {"step": "execute", "silent": command.startswith("@"),
                "command": body}
    return None


def _parse_announce(command: str) -> dict | None:
    body = command.lstrip("@")
    if body.startswith("echo "):
        return {"step": "announce", "silent": command.startswith("@"),
                "command": body}
    if body.startswith("mkdir -p"):
        return {"step": "prepare", "silent": command.startswith("@"),
                "command": body}
    return None


# A recipe command that is plain shell. Recorded verbatim and typed as shell so
# that it is visible to the audit as an unmodelled step rather than silently
# absent, but no meaning is imputed to it.
def _parse_shell(command: str) -> dict:
    return {"step": "shell", "silent": command.startswith("@"),
            "command": command.lstrip("@")}


SCRIPT_RE = re.compile(r"tools/[a-z0-9_]+\.(?:py|sh)")
CALL_RE = re.compile(r"\$\(call\s+([a-z_0-9]+)")
DEFINE_RE = re.compile(r"^define\s+([a-z_0-9]+)\s*$")

# A shell command that can fail the gate. `exit 1` and a `||` guard are the two
# forms used here; both are assertions even though no checker script runs.
SHELL_ASSERTION_RE = re.compile(r"\bexit 1\b|\|\|\s*\{|grep -Fqx")


def _macro_bodies(text: str) -> dict[str, list[str]]:
    """Collect `define ... endef` bodies so calls can be resolved."""
    bodies: dict[str, list[str]] = {}
    name: str | None = None
    body: list[str] = []
    for line in text.split("\n"):
        if name is None:
            match = DEFINE_RE.match(line)
            if match:
                name, body = match.group(1), []
            continue
        if line.strip() == "endef":
            bodies[name] = body
            name = None
            continue
        body.append(line)
    return bodies


def _parse_recipe(recipe: list[str]) -> tuple[list[dict], str | None]:
    """Decompose a recipe into typed steps, or explain why it cannot be."""
    if not recipe:
        return [], "empty recipe"
    text = "\n".join(recipe)
    if SHELL_CONTROL_RE.search(text):
        return [], "embedded shell control flow"

    steps: list[dict] = []
    for command in _logical_commands(recipe):
        if not command:
            continue
        for parser in (_parse_run, _parse_capture, _parse_check, _parse_delegate,
                       _parse_compile, _parse_execute, _parse_announce):
            parsed = parser(command)
            if parsed is not None:
                steps.append(parsed)
                break
        else:
            steps.append(_parse_shell(command))
    return steps, None


def _parse_declarations(declarations: list[str]) -> tuple[list[str], dict[str, str]]:
    prerequisites: list[str] = []
    variables: dict[str, str] = {}
    for line in declarations:
        rhs = TARGET_RE.match(line).group(2).strip()
        if not rhs:
            continue
        assignment = VAR_ASSIGN_RE.match(rhs)
        if assignment:
            variables[assignment.group(1)] = assignment.group(2)
        else:
            prerequisites.extend(rhs.split())
    return prerequisites, variables


def _assertion_kinds(gate: dict) -> list[str]:
    """How this gate can fail.

    A gate that asserts nothing is a gate that cannot fail, so this is the
    property the audit checks first. The kinds are not exclusive: an ordinary
    radio gate both runs checkers and delegates.
    """
    kinds: set[str] = set()
    steps = gate["steps"]
    if any(step["step"] == "check" for step in steps):
        kinds.add("checker")
    if any(step["step"] == "delegate" for step in steps):
        kinds.add("delegate")
    if any(step["step"] == "execute" for step in steps):
        kinds.add("native_test")
    if gate["scripts_mentioned"]:
        kinds.add("script")
    if gate["macros"]:
        kinds.add("macro")
    if gate["prerequisites"] and not steps:
        kinds.add("prerequisite")
    if SHELL_ASSERTION_RE.search("\n".join(gate["verbatim"]["recipe"])):
        kinds.add("shell_assertion")
    return sorted(kinds)


def extract(text: str) -> dict:
    """Build the gate matrix from Makefile text."""
    segments = _split_segments(text)
    macros = _macro_bodies(text)
    gates: list[dict] = []
    for segment in segments:
        if segment["kind"] != "gate":
            continue
        prerequisites, variables = _parse_declarations(segment["declarations"])
        steps, unstructured = _parse_recipe(segment["recipe"])
        gate = {
            "name": segment["name"],
            "prerequisites": prerequisites,
            "variables": variables,
            "steps": steps,
            "verbatim": {
                "declarations": segment["declarations"],
                "recipe": segment["recipe"],
            },
        }
        # Every gate names the scripts it runs somewhere in its recipe, even
        # when a checker is nested inside a shell command, a control-flow loop
        # or a `define` macro. Resolving all three keeps the parity audit's
        # view of "what does this gate actually assert" independent of how well
        # the recipe happens to decompose.
        recipe_text = "\n".join(segment["recipe"])
        called = sorted(set(CALL_RE.findall(recipe_text)))
        scripts = set(SCRIPT_RE.findall(recipe_text))
        for macro in called:
            scripts.update(SCRIPT_RE.findall("\n".join(macros.get(macro, []))))
        gate["macros"] = called
        gate["scripts_mentioned"] = sorted(scripts)
        gate["asserts_via"] = _assertion_kinds(gate)
        if unstructured:
            gate["unstructured"] = unstructured
        gates.append(gate)

    merged: dict[str, dict] = {}
    for gate in gates:
        if gate["name"] in merged:
            raise ValueError(f"non-contiguous declarations for {gate['name']}")
        merged[gate["name"]] = gate

    return {"schema": 1, "source": "Makefile", "gates": gates, "segments": segments}


def render(matrix: dict) -> str:
    """Render the extracted matrix back to Makefile text."""
    out: list[str] = []
    for segment in matrix["segments"]:
        if segment["kind"] == "text":
            out.extend(segment["lines"])
        else:
            out.extend(segment["declarations"])
            out.extend(segment["recipe"])
    return "\n".join(out)


def check_round_trip(text: str) -> dict:
    """Extract and re-render, requiring byte-exact reproduction."""
    matrix = extract(text)
    rendered = render(matrix)
    if rendered != text:
        original_lines = text.split("\n")
        rendered_lines = rendered.split("\n")
        for number, (left, right) in enumerate(zip(original_lines, rendered_lines), 1):
            if left != right:
                raise RoundTripError(
                    f"line {number} differs\n  original: {left!r}\n  rendered: {right!r}")
        raise RoundTripError(
            f"length differs: {len(original_lines)} original vs {len(rendered_lines)} rendered")
    return matrix


def public_matrix(matrix: dict) -> dict:
    """The reviewable projection: gates without the segment reconstruction."""
    return {
        "schema": matrix["schema"],
        "source": matrix["source"],
        "gates": [
            {key: value for key, value in gate.items() if key != "verbatim"}
            for gate in matrix["gates"]
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--makefile", type=pathlib.Path, default=None)
    parser.add_argument("--json", type=pathlib.Path,
                        help="write the reviewable gate matrix here")
    parser.add_argument("--check", action="store_true",
                        help="require a byte-exact round trip")
    arguments = parser.parse_args()

    source = arguments.makefile or gate_source()
    text = source.read_text()
    try:
        matrix = check_round_trip(text)
    except RoundTripError as error:
        print(f"FAIL - gate matrix round trip: {error}")
        return 1

    structured = [gate for gate in matrix["gates"] if "unstructured" not in gate]
    unstructured = [gate for gate in matrix["gates"] if "unstructured" in gate]

    if arguments.json:
        arguments.json.parent.mkdir(parents=True, exist_ok=True)
        arguments.json.write_text(
            json.dumps(public_matrix(matrix), indent=2, sort_keys=False) + "\n")

    print(f"gate matrix: {len(matrix['gates'])} gates, "
          f"{len(structured)} structured, {len(unstructured)} verbatim-only, "
          f"round trip exact")
    return 0


if __name__ == "__main__":
    sys.exit(main())

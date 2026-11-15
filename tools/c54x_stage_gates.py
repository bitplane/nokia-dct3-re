#!/usr/bin/env python3
"""Generate the C54x pipeline's per-opcode stage gates.

Most of pipeline_step's guarded blocks act on an older instruction in Access,
Read or Execute (stages 3-5) and begin with a test of that instruction's
opcode, for example `m_slot[3].valid && staged_move(m_slot[3].word)`.
Those words cannot change during a clock, so a run of such blocks can be
skipped with one test when no block's opcode test holds.

In the source, a gated run is wrapped as

	if (BIT(gates, N)) // stage gate N
	{
		...blocks...
	}

For every block in gate N this tool takes the first conjunct of its condition
that depends on exactly one of m_slot[3..5].word and nothing else (pure
opcode predicates and constants), provided every earlier conjunct is free of
side effects. It writes cpu/tms320c54x/tms320c54x_gates.h, whose tables give,
for each stage and opcode, the gates in which some block's opcode test holds.
A block's opcode test failing makes its whole condition false, so skipping the
gate is exact. A block without such a test, or with an else, is an error.

The Decode stage's checks test the decoded word, held in the constant local
`word`, and are wrapped as `if (BIT(decode_gates, N)) // decode gate N`.
Their opcode tests may name `bool const` locals of the function, read through
to their definitions, and `fact(decode_fact::X)` lookups. A decode gate may
also hold `bool const` declarations whose calls have no side effects (pure
predicates or the device's const members): skipping one is harmless, since
C++ scoping keeps its name inside the gate.

  c54x_stage_gates.py            regenerate the header
  c54x_stage_gates.py --check    fail if the header is stale
  c54x_stage_gates.py --wrap F L run-length   (development) wrap runs in F..L
"""
import argparse
from pathlib import Path
import re
import sys

SOURCE = Path('cpu/tms320c54x/tms320c54x.cpp')
DECODE = Path('cpu/tms320c54x/tms320c54x_decode.h')
DEVICE = Path('cpu/tms320c54x/tms320c54x.h')
HEADER = Path('cpu/tms320c54x/tms320c54x_gates.h')
GATE = re.compile(r'^(\t+)if \(BIT\(gates, (\d+)\)\) // stage gate \2$')
DECODE_GATE = re.compile(r'^(\t+)if \(BIT\(decode_gates, (\d+)\)\) // decode gate \2$')
FACT = re.compile(r'\bfact\((decode_fact::\w+)\)')
LOCAL = re.compile(r'^\t+bool const (\w+) = ((?:.|\n)*?);$', re.M)
DECLARATION = re.compile(r'\bbool const (\w+) = ([^;]*);')
DISPATCH = '\tswitch (execute_entry_table[m_op]) // first matching case'
CASE = re.compile(r'^\tcase (\d+): // (.+)$')
PURE_EXTRA = {'BIT', 'u8', 'u16', 's8', 's16', 'staged_opcode_descriptor', 'staged_opcode_flags'}


def pure_predicates(decode):
    return set(re.findall(r'(?:constexpr|inline)\s+(?:constexpr\s+)?[\w:<>]+\s+(\w+)\(u16 word\)', decode)) | PURE_EXTRA


def side_effect_free(decode, device_header):
    """Calls a skipped declaration may make: tms320c54x_decode.h's free
    functions, which take values, and the device's const members."""
    return (set(re.findall(r'(?:constexpr|inline)\s+(?:constexpr\s+)?[\w:<>]+\s+(\w+)\(', decode)) |
            set(re.findall(r'\b(\w+)\([^()]*\) const\b', device_header)) | PURE_EXTRA)


def matching(text, start, opening='(', closing=')'):
    """Index of the bracket closing the one at text[start]."""
    depth = 0
    for index in range(start, len(text)):
        if text[index] == opening:
            depth += 1
        elif text[index] == closing:
            depth -= 1
            if depth == 0:
                return index
    raise ValueError('unbalanced brackets')


def statement_end(text, start):
    """Index just past the statement beginning at text[start] (a block or
    one statement ending in a semicolon), ignoring // comments."""
    depth, index = 0, start
    while index < len(text):
        if text.startswith('//', index):
            index = text.index('\n', index)
            continue
        char = text[index]
        if char in '({':
            depth += 1
        elif char in ')}':
            depth -= 1
            if depth == 0 and char == '}' and text[start] == '{':
                return index + 1
        elif char == ';' and depth == 0 and text[start] != '{':
            return index + 1
        index += 1
    raise ValueError('unterminated statement')


def skip_space_and_comments(text, index):
    while True:
        while text[index] in ' \t\n':
            index += 1
        if not text.startswith('//', index):
            return index
        index = text.index('\n', index)


def conjuncts(condition):
    terms, depth, current, index = [], 0, '', 0
    while index < len(condition):
        char = condition[index]
        depth += char in '(['
        depth -= char in ')]'
        if depth == 0 and condition.startswith('&&', index):
            terms.append(current.strip())
            current, index = '', index + 2
            continue
        current += char
        index += 1
    terms.append(current.strip())
    return terms


def strip_comments(text):
    return re.sub(r'//[^\n]*', '', text)


def opcode_test(condition, pure):
    """(stage, expression in `word`) for the block's gating conjunct, or None."""
    for term in conjuncts(strip_comments(condition)):
        stages = set(re.findall(r'm_slot\[(\d)\]\.word', term))
        rest = re.sub(r'm_slot\[\d\]\.word', '', term)
        names = set(re.findall(r'[A-Za-z_]\w*', rest)) - pure
        names = {name for name in names if not re.fullmatch(r'x[0-9a-fA-F]+', name)}
        if len(stages) == 1 and stages <= set('345') and not names:
            return int(stages.pop()), re.sub(r'm_slot\[\d\]\.word', 'word', term)
        # Any call other than a pure predicate may have side effects: the
        # original evaluation order must then decide the block alone.
        calls = set(re.findall(r'([A-Za-z_]\w*)\s*\(', re.sub(r'm_\w+', '', term))) - pure
        if calls:
            return None
    return None


def declarations_only(text, allowed_calls):
    """Whether text holds only `bool const` declarations whose calls are allowed."""
    rest = strip_comments(text)
    for match in DECLARATION.finditer(rest):
        calls = set(re.findall(r'([A-Za-z_]\w*)\s*\(', match[2]))
        if calls - allowed_calls:
            return False
    return not DECLARATION.sub('', rest).strip()


def blocks(text, allowed_calls=None):
    """(start, end, condition) for each top-level if inside a gate body.

    With allowed_calls, side-effect-free declarations may sit between blocks."""
    position, found = 0, []
    while True:
        match = re.compile(r'\bif \(').search(text, position)
        if not match:
            return found
        start = match.start()
        close = matching(text, match.end() - 1)
        condition = text[match.end():close]
        rest = skip_space_and_comments(text, close + 1)
        end = statement_end(text, rest)
        following = text[skip_space_and_comments(text, end):] if end < len(text.rstrip()) else ''

        if following.startswith('else'):
            raise ValueError(f'gated block has an else: if ({condition[:60]}')
        between = text[position:start].strip()
        if strip_comments(between).strip() and not (allowed_calls is not None and declarations_only(between, allowed_calls)):
            raise ValueError(f'gate body has a statement outside its blocks: {between[:60]!r}')
        found.append((start, end, condition))
        position = end
        if allowed_calls is not None and not re.compile(r'\bif \(').search(text, position):
            tail = text[position:].strip()
            if strip_comments(tail).strip() and not declarations_only(tail, allowed_calls):
                raise ValueError(f'gate body has a statement outside its blocks: {tail[:60]!r}')


def gates(source, pure):
    """{gate: [(stage, expression)]} from the source's gate wrappers."""
    lines = source.split('\n')
    result = {}
    offsets = [0]
    for line in lines:
        offsets.append(offsets[-1] + len(line) + 1)
    for number, line in enumerate(lines):
        match = GATE.match(line)
        if not match:
            continue
        gate = int(match[2])
        if gate in result:
            raise ValueError(f'gate {gate} is defined twice')
        if lines[number + 1] != match[1] + '{':
            raise ValueError(f'gate {gate} must open a block on its next line')
        body_start = offsets[number + 1] + len(match[1])
        body = source[body_start + 1:matching(source, body_start, '{', '}')]
        tests = []
        for start, end, condition in blocks(body):
            test = opcode_test(condition, pure)
            if test is None:
                raise ValueError(f'gate {gate}: block has no opcode test: if ({condition[:80]}')
            tests.append(test)
        if not tests:
            raise ValueError(f'gate {gate} is empty')
        result[gate] = tests
    if sorted(result) != list(range(len(result))) or len(result) > 64:
        raise ValueError(f'gates must be numbered 0..N-1, at most 64: {sorted(result)}')
    return result


def word_only(term, pure):
    names = set(re.findall(r'[A-Za-z_]\w*', re.sub(r'decode_fact::\w+', '', term))) - pure - {'word', 'fact'}
    return (re.search(r'\bword\b', term) or FACT.search(term)) and all(re.fullmatch(r'x[0-9a-fA-F]+', name) for name in names)


def decode_test(condition, pure, local, depth=0):
    """Expression in `word` for a decode block's gating conjunct, or None.

    local maps a name to its definition (that of the declaration in scope)."""
    if depth > 8:
        return None
    # Every word-only conjunct before the first that may have side effects
    # must hold for the block to run; together they are its opcode test.
    tests = []
    for term in conjuncts(strip_comments(condition)):
        expanded = term
        for _ in range(8):
            expanded = re.sub(r'(?<![.:\w])\w+\b', lambda name: f'({local[name[0]]})' if name[0] in local else name[0], expanded)
        if word_only(expanded, pure):
            tests.append(FACT.sub(r'BIT(decode_facts[word], unsigned(\1))', ' '.join(expanded.split())))
            continue
        if term in local:
            inner = decode_test(local[term], pure, local, depth + 1)
            if inner:
                tests.append(inner)
        calls = set(re.findall(r'([A-Za-z_]\w*)\s*\(', re.sub(r'm_\w+', '', expanded))) - pure - {'fact'}
        if calls:
            break
    return ' && '.join(tests) or None


def decode_gates(source, pure, allowed_calls):
    """{gate: [expression]} from the source's decode gate wrappers."""
    lines = source.split('\n')
    offsets = [0]
    for line in lines:
        offsets.append(offsets[-1] + len(line) + 1)
    result = {}
    for number, line in enumerate(lines):
        match = DECODE_GATE.match(line)
        if not match:
            continue
        gate, indent = int(match[2]), match[1]
        if gate in result:
            raise ValueError(f'decode gate {gate} is defined twice')
        if lines[number + 1] != indent + '{':
            raise ValueError(f'decode gate {gate} must open a block on its next line')
        body_start = offsets[number + 1] + len(indent)
        body_end = matching(source, body_start, '{', '}')
        body = source[body_start + 1:body_end]
        # Locals in scope: the function's earlier declarations at this depth
        # or shallower, then the gate's own; later definitions shadow.
        function_start = source.rindex('\n{\n', 0, offsets[number])
        local = {}
        for definition in LOCAL.finditer(source, function_start, offsets[number]):
            if len(definition[0]) - len(definition[0].lstrip('\t')) <= len(indent):
                local[definition[1]] = ' '.join(definition[2].split())
        tests = []
        for start, end, condition in blocks(body, allowed_calls):
            for definition in LOCAL.finditer(body, 0, start):
                if len(definition[0]) - len(definition[0].lstrip('\t')) == len(indent) + 1:
                    local[definition[1]] = ' '.join(definition[2].split())
            test = decode_test(condition, pure, local)
            if test is None:
                raise ValueError(f'decode gate {gate}: block has no opcode test: if ({condition[:80]}')
            tests.append(test)
        if not tests:
            raise ValueError(f'decode gate {gate} is empty')
        result[gate] = tests
    if sorted(result) != list(range(len(result))) or len(result) > 8:
        raise ValueError(f'decode gates must be numbered 0..N-1, at most 8: {sorted(result)}')
    return result


def decode_section(table):
    if not table:
        return ''
    terms = []
    for gate, tests in sorted(table.items()):
        joined = ' ||\n\t\t\t'.join(f'({expression})' for expression in tests)
        terms.append(f'\tif ({joined})\n\t\tresult |= 1U << {gate};')
    return ('// For every opcode, the decode gates in pipeline_step in which some\n'
            '// block\'s opcode test holds.\n'
            'inline u8 decode_gates(u16 word)\n{\n\tunsigned result = 0;\n' + '\n'.join(terms) + '\n\treturn u8(result);\n}\n\n'
            'inline const std::array<u8, 65536> decode_gate_table = []\n{\n\tstd::array<u8, 65536> result{};\n'
            '\tfor (unsigned word = 0; word < result.size(); ++word)\n\t\tresult[word] = decode_gates(u16(word));\n'
            '\treturn result;\n}();\n\n')


def execute_cases(source, pure):
    """Ordered m_op conditions of pipeline_execute's dispatch switch.

    Case N runs when condition N is the first that holds: the table is the
    if/else-if chain it replaced, evaluated once per opcode.
    """
    lines = source.split('\n')
    if lines.count(DISPATCH) != 1:
        raise ValueError('pipeline_execute has no unique execute dispatch switch')
    start = source.index(DISPATCH)
    body = source[start:matching(source, source.index('{', start), '{', '}')]
    conditions = []
    for line in body.split('\n'):
        match = CASE.match(line)
        if match:
            if int(match[1]) != len(conditions):
                raise ValueError(f'execute case {match[1]} is out of order')
            condition = match[2]
            rest = re.sub(r'\bm_op\b', '', condition)
            names = set(re.findall(r'[A-Za-z_]\w*', rest)) - pure
            if any(not re.fullmatch(r'x[0-9a-fA-F]+', name) for name in names):
                raise ValueError(f'execute case {match[1]} depends on more than m_op: {condition}')
            conditions.append(re.sub(r'\bm_op\b', 'word', condition))
    if not conditions or len(conditions) > 255:
        raise ValueError('execute dispatch must have 1..255 cases')
    return conditions


def execute_section(cases):
    """The dispatch table, once pipeline_execute has its switch."""
    if not cases:
        return ''
    tests = '\n'.join(f'\tif ({condition}) return {index};' for index, condition in enumerate(cases))
    return f'''// pipeline_execute's dispatch: the first case whose opcode test holds, or
// {len(cases)} for none.
inline u8 execute_entry(u16 word)
{{
{tests}
	return {len(cases)};
}}

inline const std::array<u8, 65536> execute_entry_table = []
{{
	std::array<u8, 65536> result{{}};
	for (unsigned word = 0; word < result.size(); ++word)
		result[word] = execute_entry(u16(word));
	return result;
}}();

'''


def header(table, cases=(), decode=None):
    functions, rows = [], []
    for stage in (3, 4, 5):
        terms = []
        for gate, tests in sorted(table.items()):
            expressions = [expression for test_stage, expression in tests if test_stage == stage]
            if expressions:
                joined = ' ||\n\t\t\t'.join(f'({expression})' for expression in expressions)
                terms.append(f'\tif ({joined})\n\t\tresult |= u64(1) << {gate};')
        functions.append(f'inline u64 stage{stage}_gates(u16 word)\n{{\n\tu64 result = 0;\n' + '\n'.join(terms) + '\n\treturn result;\n}\n')
        rows.append(f'inline const std::array<u64, 65536> stage{stage}_gate_table = make_gate_table(stage{stage}_gates);')
    return f'''// license:BSD-3-Clause
// copyright-holders:Gaz
// Generated by tools/c54x_stage_gates.py from tms320c54x.cpp; do not edit.
// For each of the Access, Read and Execute stages and every opcode, the
// stage gates in pipeline_step in which some block's opcode test holds.
#ifndef MAME_CPU_TMS320C54X_GATES_H
#define MAME_CPU_TMS320C54X_GATES_H
#include "tms320c54x_decode.h"

namespace c54x_decode {{

{chr(10).join(functions)}
inline std::array<u64, 65536> make_gate_table(u64 (*gates)(u16))
{{
	std::array<u64, 65536> result{{}};
	for (unsigned word = 0; word < result.size(); ++word)
		result[word] = gates(u16(word));
	return result;
}}

{chr(10).join(rows)}

{decode_section(decode or {})}{execute_section(cases)}}} // namespace c54x_decode

#endif // MAME_CPU_TMS320C54X_GATES_H
'''


def convert_chain(source, first_line):
    """Replace the if/else-if chain starting at first_line with the dispatch switch."""
    lines = source.split('\n')
    offsets = [0]
    for line in lines:
        offsets.append(offsets[-1] + len(line) + 1)
    position = offsets[first_line - 1]
    if not source.startswith('\tif (', position):
        raise ValueError('the chain must start with a top-level if')
    def with_trailing_comment(end):
        # Keep a comment that shares the statement's last line.
        line_end = source.index('\n', end)
        return line_end if source[end:line_end].strip().startswith('//') else end

    def comments_between(start, stop):
        return [line.strip() for line in source[start:stop].split('\n') if line.strip().startswith('//')]

    branches, final, notes = [], None, []
    index = position + 1
    while True:
        open_paren = source.index('(', index)
        close = matching(source, open_paren)
        condition = ' '.join(source[open_paren + 1:close].split())
        body_start = skip_space_and_comments(source, close + 1)
        body_end = with_trailing_comment(statement_end(source, body_start))
        # Comments between the condition and its body open the case body.
        leading = ''.join(note + '\n\t' for note in comments_between(close + 1, body_start))
        branches.append((notes, condition, leading + source[body_start:body_end]))
        after = skip_space_and_comments(source, body_end)
        notes = comments_between(body_end, after)
        if source.startswith('else if (', after):
            index = after + len('else ')
            continue
        if source.startswith('else', after) and not source[after + 4].isalnum():
            final_start = skip_space_and_comments(source, after + 4)
            final_end = with_trailing_comment(statement_end(source, final_start))
            leading = ''.join(note + '\n\t' for note in comments_between(after + 4, final_start))
            final = (notes, leading + source[final_start:final_end])
            body_end = final_end
        break
    def indented(body):
        # A body's first line starts at its statement; later lines keep their
        # own indentation, one level deeper inside the case.
        first, *rest = body.split('\n')
        return '\n'.join(['\t\t' + first] + [('\t' + line) if line else line for line in rest])

    out = [DISPATCH, '\t{']
    for number, (notes, condition, body) in enumerate(branches):
        out.extend('\t' + note for note in notes)
        out.append(f'\tcase {number}: // {condition}')
        out.append(indented(body))
        out.append('\t\tbreak;')
    if final is not None:
        out.extend('\t' + note for note in final[0])
        out.append('\tdefault:')
        out.append(indented(final[1]))
        out.append('\t\tbreak;')
    out.append('\t}')
    return source[:position] + '\n'.join(out) + source[body_end:]


def wrap(source, first, last, run_length, pure):
    """Wrap gatable top-level blocks of lines first..last in new gates."""
    lines = source.split('\n')
    existing = len(gates(source, pure))
    output, index, gate = lines[:first - 1], first - 1, existing
    pending = []

    def flush():
        nonlocal gate
        for start in range(0, len(pending), run_length):
            chunk = pending[start:start + run_length]
            if len(chunk) < 2:
                output.extend(line for block in chunk for line in block)
                continue
            output.append(f'\tif (BIT(gates, {gate})) // stage gate {gate}')
            output.append('\t{')
            output.extend(('\t' + line) if line else line for block in chunk for line in block)
            output.append('\t}')
            gate += 1
        pending.clear()

    while index < last:
        line = lines[index]
        if line.startswith('\tif (') and not line.startswith('\t\t'):
            text = '\n'.join(lines[index:])
            start = text.index('(')
            close = matching(text, start)
            condition = text[start + 1:close]
            end = statement_end(text, skip_space_and_comments(text, close + 1))
            count = text[:end].count('\n') + 1
            block = lines[index:index + count]
            following = text[skip_space_and_comments(text, end):]
            if not following.startswith('else') and opcode_test(condition, pure):
                pending.append(block)
                index += count
                continue
        flush()
        output.append(line)
        index += 1
    flush()
    output.extend(lines[last:])
    return '\n'.join(output)


def generate(source, pure, allowed):
    return header(gates(source, pure), execute_cases(source, pure) if DISPATCH in source else (), decode_gates(source, pure, allowed))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--wrap', nargs=3, type=int, metavar=('FIRST', 'LAST', 'RUN'))
    parser.add_argument('--convert-chain', type=int, metavar='LINE')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    pure = pure_predicates((root / DECODE).read_text())
    allowed = side_effect_free((root / DECODE).read_text(), (root / DEVICE).read_text())
    source = (root / SOURCE).read_text()
    if args.wrap:
        source = wrap(source, *args.wrap, pure)
        (root / SOURCE).write_text(source)
    if args.convert_chain:
        source = convert_chain(source, args.convert_chain)
        (root / SOURCE).write_text(source)
    try:
        generated = generate(source, pure, allowed)
    except ValueError as error:
        raise SystemExit(f'FAIL - C54x stage gates: {error}')
    if args.check:
        if not (root / HEADER).exists() or (root / HEADER).read_text() != generated:
            raise SystemExit(f'FAIL - {HEADER} is stale; run tools/c54x_stage_gates.py')
        print('C54x stage gates: header current')
        return
    (root / HEADER).write_text(generated)
    print(f'C54x stage gates: wrote {HEADER}')


if __name__ == '__main__':
    main()

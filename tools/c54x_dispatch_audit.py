"""Conservative C54x residual-dispatch census; requires libclang and a C++ compiler.

Clang supplies raw C++ tokens (not a type-checked AST). A bounded statement
reader recovers if/else/switch structure; actual C++ evaluates opcode-only
guards against the production decode header. Unknown state conditions fork
both ways. This proves absence only within ordinary admitted execution, not
arbitrary debugger edits or corrupted saved states.
"""
import argparse
import ctypes as C
import ctypes.util
from dataclasses import dataclass, field
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ALL = (1 << 65536) - 1


class CXString(C.Structure):
    _fields_ = [('data', C.c_void_p), ('flags', C.c_uint)]


class CXLocation(C.Structure):
    _fields_ = [('data', C.c_void_p * 2), ('value', C.c_uint)]


class CXRange(C.Structure):
    _fields_ = [('data', C.c_void_p * 2), ('begin', C.c_uint), ('end', C.c_uint)]


class CXToken(C.Structure):
    _fields_ = [('data', C.c_uint * 4), ('pointer', C.c_void_p)]


class CXUnsaved(C.Structure):
    _fields_ = [('filename', C.c_char_p), ('contents', C.c_char_p), ('length', C.c_ulong)]


@dataclass
class Token:
    text: str
    start: int
    end: int


def clang_library():
    for name in ('clang', 'clang-21', 'clang-20', 'clang-19', 'clang-18'):
        found = ctypes.util.find_library(name)
        if found:
            return found
    raise ValueError('libclang is required for the dispatch audit')


def lex(source, library=None):
    lib = C.CDLL(library or clang_library())
    def api(name, result, arguments):
        fn = getattr(lib, name)
        fn.restype, fn.argtypes = result, arguments
        return fn
    string_data = api('clang_getCString', C.c_char_p, [CXString])
    dispose_string = api('clang_disposeString', None, [CXString])
    index = api('clang_createIndex', C.c_void_p, [C.c_int, C.c_int])(0, 0)
    path = b'/tmp/c54x-dispatch-lex.cpp'
    unsaved = CXUnsaved(path, source, len(source))
    tu = C.c_void_p()
    arguments = (C.c_char_p * 3)(b'-xc++', b'-std=c++20', b'-ferror-limit=1')
    result = api('clang_parseTranslationUnit2', C.c_int,
        [C.c_void_p, C.c_char_p, C.POINTER(C.c_char_p), C.c_int,
         C.POINTER(CXUnsaved), C.c_uint, C.c_uint, C.POINTER(C.c_void_p)])(
        index, path, arguments, 3, C.byref(unsaved), 1, 0, C.byref(tu))
    if result:
        raise ValueError(f'Clang could not open the token buffer: {result}')
    tokens, count = C.POINTER(CXToken)(), C.c_uint()
    try:
        file = api('clang_getFile', C.c_void_p, [C.c_void_p, C.c_char_p])(tu, path)
        location = api('clang_getLocationForOffset', CXLocation, [C.c_void_p, C.c_void_p, C.c_uint])
        span = api('clang_getRange', CXRange, [CXLocation, CXLocation])(
            location(tu, file, 0), location(tu, file, len(source)))
        api('clang_tokenize', None, [C.c_void_p, CXRange, C.POINTER(C.POINTER(CXToken)), C.POINTER(C.c_uint)])(
            tu, span, C.byref(tokens), C.byref(count))
        spelling = api('clang_getTokenSpelling', CXString, [C.c_void_p, CXToken])
        kind = api('clang_getTokenKind', C.c_uint, [CXToken])
        extent = api('clang_getTokenExtent', CXRange, [C.c_void_p, CXToken])
        begin = api('clang_getRangeStart', CXLocation, [CXRange])
        end = api('clang_getRangeEnd', CXLocation, [CXRange])
        offset = api('clang_getSpellingLocation', None,
                     [CXLocation, C.c_void_p, C.c_void_p, C.c_void_p, C.POINTER(C.c_uint)])
        result = []
        for i in range(count.value):
            if kind(tokens[i]) == 4:  # Comments are not statement delimiters.
                continue
            text = spelling(tu, tokens[i])
            word = string_data(text).decode()
            dispose_string(text)
            start, finish = C.c_uint(), C.c_uint()
            token_range = extent(tu, tokens[i])
            offset(begin(token_range), None, None, None, C.byref(start))
            offset(end(token_range), None, None, None, C.byref(finish))
            result.append(Token(word, start.value, finish.value))
        return result
    finally:
        api('clang_disposeTokens', None, [C.c_void_p, C.POINTER(CXToken), C.c_uint])(tu, tokens, count)
        api('clang_disposeTranslationUnit', None, [C.c_void_p])(tu)
        api('clang_disposeIndex', None, [C.c_void_p])(index)


@dataclass
class Node:
    kind: str
    start: int
    end: int
    expression: str = ''
    children: list = field(default_factory=list)
    otherwise: object = None
    labels: list = field(default_factory=list)


class Reader:
    def __init__(self, tokens):
        self.tokens = tokens
        self.words = [token.text for token in tokens]
        self.pairs = {}
        stack = []
        for i, word in enumerate(self.words):
            if word in ('(', '[', '{'):
                stack.append(i)
            elif word in (')', ']', '}'):
                if not stack or self.words[stack[-1]] != {')': '(', ']': '[', '}': '{'}[word]:
                    raise ValueError('unbalanced C++ token delimiters')
                first = stack.pop()
                self.pairs[first] = i
        if stack:
            raise ValueError('unfinished C++ token delimiters')

    def expression(self, first, last):
        return ' '.join(self.words[first:last])

    def function(self, name, qualified=True):
        candidates = [i for i, word in enumerate(self.words)
                      if word == name and i and (not qualified or self.words[i-1] == '::')
                      and self.words[i+1] == '(' and self.words[self.pairs[i+1]+1] in ('{', 'const')]
        if len(candidates) != 1:
            raise ValueError(f'expected one definition of {name}, found {len(candidates)}')
        start = self.pairs[candidates[0]+1]+1
        if self.words[start] == 'const':
            start += 1
        if self.words[start] != '{':
            raise ValueError(f'unsupported declaration of {name}')
        return self.statement(start)[0]

    def statement(self, i):
        first, word = i, self.words[i]
        if word == '#':
            raise ValueError('conditional preprocessing in an audited body is unsupported')
        if word in ('goto', 'try', 'catch', 'do', 'co_return', '__asm', 'asm'):
            raise ValueError(f'unsupported control-flow statement: {word}')
        if word == '{':
            children, last = [], self.pairs[i]
            i += 1
            while i < last:
                child, i = self.statement(i)
                children.append(child)
            return Node('block', first, last+1, children=children), last+1
        if word == 'if':
            if self.words[i+1] != '(':
                raise ValueError('unsupported if form')
            close = self.pairs[i+1]
            then, end = self.statement(close+1)
            otherwise = None
            if self.words[end] == 'else':
                otherwise, end = self.statement(end+1)
            return Node('if', first, end, self.expression(i+2, close), [then], otherwise), end
        if word in ('switch', 'for', 'while'):
            close = self.pairs[i+1]
            body, end = self.statement(close+1)
            return Node(word, first, end, self.expression(i+2, close), [body]), end
        if word in ('case', 'default'):
            end = i+1
            while self.words[end] != ':':
                end += 1
            return Node('label', first, end+1, self.expression(i+1, end) if word == 'case' else 'default'), end+1
        # Expression/declaration statements can contain lambdas and nested calls.
        while self.words[i] != ';':
            if self.words[i] in ('(', '[', '{'):
                i = self.pairs[i]
            elif self.words[i] == '}':
                raise ValueError('statement lacks a terminator')
            i += 1
        return Node(word if word in ('return', 'break', 'continue') else 'simple', first, i+1), i+1

    def walk(self, node):
        yield node
        for child in node.children:
            yield from self.walk(child)
        if node.otherwise:
            yield from self.walk(node.otherwise)

    def calls(self, node, name):
        return sum(self.words[i:i+2] == [name, '('] for i in range(node.start, node.end-1))


def constant_alias(reader, function, name):
    candidates = []
    for node in function.children:
        words = reader.words[node.start:node.end]
        if node.kind == 'simple' and name in words:
            i = words.index(name)
            if words[i+1:i+2] == ['='] and 'const' in words[:i]:
                candidates.append((reader.expression(node.start, node.end),
                                   ' '.join(words[i+2:-1])))
    if len(candidates) != 1:
        raise ValueError(f'expected one constant opcode alias {name}')
    return name, *candidates[0]


class Guards:
    def __init__(self, header, aliases=(), extra_admission='false'):
        self.header = header
        self.names = set(re.findall(r'\b(?:bool|u8|u16|u32)\s+(\w+)\s*\(', header.read_text()))
        self.names |= {'op',
                       'BIT', 'bool', 'u8', 'u16', 'u32', 'u64', 's8', 's16', 's32', 'true', 'false'}
        self.expressions, self.bits = {}, {}
        self.extra_admission = extra_admission
        self.aliases = []
        for name, declaration, expression in aliases:
            if not self.collect(expression):
                raise ValueError(f'alias {name} is not opcode-only')
            self.aliases.append(self.normalize(declaration))
            self.names.add(name)

    def normalize(self, expression):
        return re.sub(r'\b(?:m_op|m_pipeline_repeat_word)\b', 'op', expression)

    def collect(self, expression):
        expression = self.normalize(expression)
        # Numeric literals must not contribute identifiers such as xff00.
        identifiers = set(re.findall(r'\b[A-Za-z_]\w*\b', re.sub(r'\b0[xX][0-9a-fA-F]+[uUlL]*\b|\b\d+[uUlL]*\b', '', expression)))
        if identifiers - self.names:
            return False
        self.expressions.setdefault(expression, len(self.expressions))
        return True

    def compile(self, directory, compiler):
        expressions = list(self.expressions)
        program = ['#include "tms320c54x_decode.h"', '#include <cstdio>', '#include <vector>',
                   'using namespace c54x_decode;', 'int main() {',
                   f'std::vector<unsigned char> bits({len(expressions)+2} * 8192);',
                   'for (unsigned word=0; word<65536; ++word) {',
                   'u16 const op=word;', *self.aliases,
                   'auto bus_admission=[&](bool wait, bool m_c5416_bus_profile) {',
                   'auto software_wait_enabled=[&]() { return wait; };',
                   f'return {self.extra_admission};', '};',
                   'bool const bus=bus_admission(false,false)||bus_admission(false,true)||bus_admission(true,false)||bus_admission(true,true);']
        for i, expression in enumerate(expressions):
            program.append(f'if ({expression}) bits[{i}*8192+word/8] |= 1U<<(word%8);')
        for i, mode in enumerate((4, 5), len(expressions)):
            program.append(f'if ((staged_opcode_descriptor[op] & (1U<<{mode})) || bus) bits[{i}*8192+word/8] |= 1U<<(word%8);')
        program += ['}', 'return std::fwrite(bits.data(),1,bits.size(),stdout)==bits.size() ? 0 : 1;', '}']
        source, binary = directory/'guards.cpp', directory/'guards'
        source.write_text('\n'.join(program)+'\n')
        subprocess.run([compiler, '-std=c++20', '-O2', '-I'+str(self.header.parent),
            *['-I'+str(ROOT/'mame'/part) for part in ('src/emu', 'src/lib', 'src/lib/util', 'src/osd')],
            str(source), '-o', str(binary)], check=True, capture_output=True, text=True)
        data = subprocess.check_output([str(binary)])
        if len(data) != (len(expressions)+2)*8192:
            raise ValueError('incomplete compiled guard census')
        rows = [int.from_bytes(data[i:i+8192], 'little') for i in range(0, len(data), 8192)]
        self.bits = dict(zip(expressions, rows))
        return rows[-2:]

    def get(self, expression):
        return self.bits.get(self.normalize(expression))


def ranges(bits):
    result, start, previous = [], None, None
    for word in range(65536):
        if bits >> word & 1:
            if previous is None or word != previous+1:
                if start is not None:
                    result.append([start, previous])
                start = word
            previous = word
    if start is not None:
        result.append([start, previous])
    return [[f'{a:04x}', f'{b:04x}'] for a, b in result]


def execute_routes(reader, function, guards, admitted, callee):
    candidates = [n for n in function.children if n.kind == 'if' and
                  n.expression.startswith('staged_immediate_repeat (')]
    if len(candidates) != 1:
        raise ValueError('cannot identify the ordered Execute dispatch chain')
    node, remaining, routes = candidates[0], admitted, {}
    while node:
        if node.kind != 'if' or guards.get(node.expression) is None:
            raise ValueError('Execute dispatch condition is not opcode-only')
        selected = remaining & guards.get(node.expression)
        calls = reader.calls(node.children[0], callee)
        if calls:
            if calls != 1:
                raise ValueError('ambiguous Execute caller')
            routes[node.expression] = selected
        remaining &= ~guards.get(node.expression)
        node = node.otherwise
    # A final false guard is a no-call path (e.g. XC), not a residual caller.
    return routes


class Flow:
    def __init__(self, reader, guards, callee):
        self.reader, self.guards, self.callee = reader, guards, callee
        self.calls, self.handlers = [], []

    def record(self, node, expression, incoming, matched, depth):
        self.handlers.append(dict(node=node, expression=expression, incoming=incoming,
                                  matched=matched, depth=depth))

    def run(self, node, incoming, depth=0):
        """Return (fall-through, function return, switch break) word supersets."""
        if not incoming:
            if node.kind in ('if', 'switch'):
                self.record(node, node.expression, 0, 0, depth)
            return 0, 0, 0
        if node.kind == 'block':
            normal, returned, broken = incoming, 0, 0
            for child in node.children:
                normal, ret, brk = self.run(child, normal, depth+1)
                returned |= ret
                broken |= brk
            return normal, returned, broken
        if node.kind == 'if':
            bits = self.guards.get(node.expression)
            yes, no = (incoming, incoming) if bits is None else (incoming & bits, incoming & ~bits)
            self.record(node, node.expression, incoming, yes, depth)
            a = self.run(node.children[0], yes, depth+1)
            b = self.run(node.otherwise, no, depth+1) if node.otherwise else (no, 0, 0)
            return tuple(x | y for x, y in zip(a, b))
        if node.kind == 'switch':
            groups, labels, children = [], [], []
            for child in node.children[0].children:
                if child.kind == 'label':
                    if children:
                        groups.append((labels, children))
                        labels, children = [], []
                    labels.append(child)
                else:
                    if not labels:
                        raise ValueError('switch body precedes its labels')
                    children.append(child)
            if labels:
                groups.append((labels, children))
            explicit = 0
            for labels, _ in groups:
                for label in labels:
                    if label.expression != 'default':
                        bits = self.guards.get(f'({node.expression}) == ({label.expression})')
                        if bits is None:
                            raise ValueError('switch selector is not opcode-only')
                        explicit |= bits
            fall, after, returned, default = 0, 0, 0, False
            for labels, children in groups:
                selected = 0
                for label in labels:
                    if label.expression == 'default':
                        selected |= incoming & ~explicit
                        default = True
                    else:
                        selected |= incoming & self.guards.get(f'({node.expression}) == ({label.expression})')
                arm = Node('arm', labels[0].start, children[-1].end,
                           ','.join(label.expression for label in labels), children)
                self.record(arm, f'switch ({node.expression}): {arm.expression}', incoming, selected | fall, depth)
                fall, ret, brk = self.run(Node('block', arm.start, arm.end, children=children), selected | fall, depth+1)
                returned |= ret
                after |= brk
            return after | fall | (0 if default else incoming & ~explicit), returned, 0
        if node.kind == 'return':
            return 0, incoming, 0
        if node.kind == 'break':
            return 0, 0, incoming
        if node.kind in ('for', 'while'):
            # No loop count/state proof: iterations and termination stay unknown.
            self.run(node.children[0], incoming, depth+1)
            return incoming, 0, 0
        if node.kind == 'continue':
            return incoming, 0, 0
        if self.reader.calls(node, self.callee):
            self.calls.append((node, incoming))
        return incoming, 0, 0


def audit(source_path, header, directory, compiler='g++', library=None, callee='execute_one'):
    source = source_path.read_bytes()
    reader = Reader(lex(source, library))
    if not any(reader.words[i:i+8] == ['::', callee, '(', 'u16', 'const', 'op', ')', '{']
               for i in range(len(reader.words)-7)):
        raise ValueError('the residual opcode parameter must be const for this proof')
    execute, step, residual = [reader.function(name) for name in ('pipeline_execute', 'pipeline_step', callee)]
    repeat = [n for n in step.children if n.kind == 'if' and n.expression == 'm_pipeline_repeat_stream']
    if len(repeat) != 1:
        raise ValueError('expected one accelerated repeat scope')
    aliases = [constant_alias(reader, residual, name) for name in ('low', 'short_preincrement', 'mmr_source')
               if name in reader.words[residual.start:residual.end]]
    aliases.append(constant_alias(reader, repeat[0].children[0], 'immediate_store'))
    cpu_header = source_path.with_suffix('.h').read_bytes()
    header_reader = Reader(lex(cpu_header, library))
    bus = header_reader.function('staged_bus_control', qualified=False)
    if len(bus.children) != 1 or bus.children[0].kind != 'return':
        raise ValueError('bus admission is not a single pure return expression')
    bus = header_reader.expression(bus.children[0].start+1, bus.children[0].end-1)
    bus_names = set(re.findall(r'\b[A-Za-z_]\w*\b', re.sub(r'\b0x[0-9a-fA-F]+\b', '', bus)))
    if bus_names - {'op', 'software_wait_enabled', 'm_c5416_bus_profile', 'true', 'false'}:
        raise ValueError('unmodeled configuration in bus admission')
    guards = Guards(header, aliases, bus)
    for function in (execute, repeat[0].children[0], residual):
        for node in reader.walk(function):
            if node.kind == 'if':
                guards.collect(node.expression)
            elif node.kind == 'switch':
                for label in node.children[0].children:
                    if label.kind == 'label' and label.expression != 'default':
                        guards.collect(f'({node.expression}) == ({label.expression})')
    admitted = guards.compile(directory, compiler)
    routes, combined = {}, 0
    for mode, words in zip(('base', 'extended'), admitted):
        mode_routes = execute_routes(reader, execute, guards, words, callee)
        flow = Flow(reader, guards, callee)
        flow.run(repeat[0].children[0], words)
        if len(flow.calls) != 1:
            raise ValueError('expected one repeat caller')
        mode_routes['accelerated_repeat'] = flow.calls[0][1]
        routes[mode] = {name: dict(words=bits.bit_count(), ranges=ranges(bits)) for name, bits in mode_routes.items()}
        for bits in mode_routes.values():
            combined |= bits
    if reader.calls(execute, callee) != 2 or reader.calls(step, callee) != 1:
        raise ValueError('caller inventory changed; review all call sites')
    if sum(t.text == callee and i+1 < len(reader.tokens) and reader.words[i+1] == '(' for i, t in enumerate(reader.tokens)) != 4:
        raise ValueError('unclassified dispatcher definition/caller')
    flow = Flow(reader, guards, callee)
    fallthrough, _, _ = flow.run(residual, combined)
    rows = []
    for row in flow.handlers:
        node, matched = row['node'], row['matched']
        begin, end = reader.tokens[node.start].start, reader.tokens[node.end-1].end
        unknown = node.kind == 'if' and guards.get(node.expression) is None
        rows.append(dict(kind=node.kind, line=source[:begin].count(b'\n')+1,
                         begin=begin, end=end, depth=row['depth'], expression=row['expression'],
                         status='shadowed' if not matched else 'unresolved' if unknown else 'live',
                         words=matched.bit_count(), incoming_words=row['incoming'].bit_count(),
                         has_else=bool(node.otherwise)))
    return dict(source_sha256=hashlib.sha256(source).hexdigest(),
                cpu_header_sha256=hashlib.sha256(cpu_header).hexdigest(),
                decode_sha256=hashlib.sha256(header.read_bytes()).hexdigest(),
                guard_count=len(guards.expressions), caller_sites=3, opcode_universe=65536,
                routes=routes, combined_words=combined.bit_count(), combined_ranges=ranges(combined),
                unhandled_words=fallthrough.bit_count(), handlers=rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT/'cpu/tms320c54x/tms320c54x.cpp')
    parser.add_argument('--header', type=Path, default=ROOT/'cpu/tms320c54x/tms320c54x_decode.h')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--compiler', default='g++')
    parser.add_argument('--libclang')
    parser.add_argument('--callee', default='execute_one')
    parser.add_argument('--check', type=Path, help='require the reviewed caller opcode sets')
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='c54x-dispatch-') as temp:
        result = audit(args.source, args.header, Path(temp), args.compiler, args.libclang, args.callee)
    if args.check:
        contract = json.loads(args.check.read_text())
        if set(contract['profiles']) != set(result['routes']):
            raise ValueError('reviewed dispatch profiles differ')
        if any(result['routes'][profile] != contract['routes'] for profile in contract['profiles']):
            raise ValueError('reviewed caller opcode sets differ')
        for key in ('combined_words', 'combined_ranges', 'unhandled_words'):
            if contract[key] != result[key]:
                raise ValueError(f'reviewed dispatch contract differs: {key}')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2)+'\n')
    print(f"Dispatch census: {result['combined_words']} words, {len(result['handlers'])} conditions/arms; "
          f"report {args.output}")


if __name__ == '__main__':
    main()

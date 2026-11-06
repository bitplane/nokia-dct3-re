"""Inventory explicit CPU rejection sites, not ISA completeness or reachability.

Uses the dispatch audit's raw C++ token reader. Category labels summarize source
diagnostics; neither a matching opcode nor an absent log proves a guard is reached.
"""
import argparse
import ast
from collections import Counter
import hashlib
import json
from pathlib import Path

try:
    from .c54x_dispatch_audit import Reader, lex
except ImportError:
    from c54x_dispatch_audit import Reader, lex

ROOT = Path(__file__).resolve().parents[1]


def category(message):
    if message is None:
        return 'unlogged'
    if 'arbitration' in message:
        return 'bus-arbitration'
    if any(word in message for word in ('form %04x', 'unimplemented', 'addressing', 'extension')):
        return 'encoding'
    if any(word in message for word in ('composition', 'control delay', 'control/count',
            'nested', 'repeat count owner', 'BRAF clear', 'delay BRC', 'delay PMST')):
        return 'control-composition'
    return 'other-diagnostic'


def inventory(source):
    reader = Reader(lex(source))
    words = reader.words
    sites = [i for i in range(len(words)-3)
             if words[i:i+4] == ['m_illegal', '=', 'true', ';']]
    definitions = []
    for i in range(1, len(words)-2):
        if words[i-1:i+1] != ['tms320c54x_device', '::'] or words[i+2] != '(':
            continue
        end = reader.pairs[i+2]+1
        if words[end] == 'const':
            end += 1
        if words[end] == '{':
            definitions.append((words[i+1], end, reader.pairs[end]))
    rows = []
    for site in sites:
        owners = [name for name, first, last in definitions if first < site < last]
        if len(owners) != 1:
            raise ValueError('unclassified rejection owner')
        owner = reader.function(owners[0])
        path = []

        def visit(node, guards):
            if not node.start <= site < node.end:
                return
            path.append((node, guards))
            for child in node.children:
                visit(child, guards + ([node.expression] if node.kind in ('if', 'for', 'while', 'switch') else []))
            if node.otherwise:
                visit(node.otherwise, guards + ['!(' + node.expression + ')'])

        visit(owner, [])
        block = next(node for node, _ in reversed(path) if node.kind == 'block')
        message = None
        for child in block.children:
            if child.end > site:
                break
            if reader.words[child.start:child.start+2] == ['logerror', '(']:
                literals = []
                for word in reader.words[child.start+2:child.end]:
                    if not word.startswith('"'):
                        break
                    literals.append(word)
                if not literals:
                    raise ValueError('nonliteral rejection diagnostic')
                message = ''.join(ast.literal_eval(word) for word in literals)
        rows.append(dict(function=owners[0], line=source[:reader.tokens[site].start].count(b'\n')+1,
                         guards=path[-1][1], message=message, category=category(message)))
    return dict(source_sha256=hashlib.sha256(source).hexdigest(), sites=len(rows),
                categories=dict(Counter(row['category'] for row in rows)), rows=rows,
                reachability_claim=False, isa_completeness_claim=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT/'cpu/tms320c54x/tms320c54x.cpp')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = inventory(args.source.read_bytes())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(f"Rejection sites: {result['sites']}; source categories: {result['categories']}")


if __name__ == '__main__':
    main()

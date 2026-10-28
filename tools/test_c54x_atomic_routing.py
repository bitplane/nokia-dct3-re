"""Independently check atomic fixture routing, not processor correctness."""
import ast
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SOURCES = ROOT / 'tests' / 'c54x'
OWNERS = ('basic', 'corpus', 'reference', 'control')
ALLOWED = (ast.Expression, ast.BoolOp, ast.And, ast.Or, ast.Compare,
           ast.Eq, ast.NotEq, ast.Lt, ast.LtE, ast.Gt, ast.GtE,
           ast.Name, ast.Load, ast.Constant, ast.BinOp, ast.Add)


def top_level_guards(text):
    # Ignore braces inside comments/literals; indentation is not C++ structure.
    masked = re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])\'',
                    lambda match: ' ' * len(match[0]), text, flags=re.S)
    guards = []
    depth = 0
    for token in re.finditer(r'\{|\}|\bif\s*\(', masked):
        if token[0] == '{':
            depth += 1
        elif token[0] == '}':
            depth -= 1
        elif depth == 2:  # namespace + method
            end = token.end()
            nesting = 1
            while nesting and end < len(masked):
                nesting += (masked[end] == '(') - (masked[end] == ')')
                end += 1
            if nesting:
                raise ValueError('Unclosed top-level condition')
            guards.append(masked[token.end():end - 1])
    return guards


def guarded_partition():
    header = (SOURCES / 'tms320c54x_test_state.h').read_text()
    native = int(re.search(r'native_store_phase\s*=\s*([\d\']+)', header)[1].replace("'", ''))
    opcodes = re.search(r'far_opcodes\[\]\s*=\s*\{(.*?)\}', header, re.S)[1]
    far_count = len(re.findall(r'0x[0-9a-f]+', opcodes))
    points = {0, native + 1}
    expressions = []
    for owner in OWNERS:
        text = (SOURCES / f'tms320c54x_test_atomic_{owner}.cpp').read_text()
        guards = top_level_guards(text)
        for guard in guards:
            expression = guard.replace('m_phase', 'phase').replace('native_store_phase', str(native))
            expression = expression.replace('std::size(far_opcodes)', str(far_count))
            expression = expression.replace('&&', ' and ').replace('||', ' or ')
            tree = ast.parse(expression, mode='eval')
            for node in ast.walk(tree):
                if not isinstance(node, ALLOWED):
                    raise ValueError(f'Unclassified phase guard: {guard}')
                if isinstance(node, ast.Name) and node.id != 'phase':
                    raise ValueError(f'Unknown phase-guard input: {node.id}')
                if isinstance(node, ast.Constant) and type(node.value) is int:
                    points.update((node.value, node.value + 1))
                if isinstance(node, ast.BinOp):
                    if not isinstance(node.left, ast.Constant) or not isinstance(node.right, ast.Constant):
                        raise ValueError('Only constant phase limits are supported')
                    limit = node.left.value + node.right.value
                    points.update((limit, limit + 1))
            expressions.append((owner, compile(tree, '<phase guard>', 'eval')))
    ranges = re.findall(r'\{(\d+), (\d+), atomic_fixture::(\w+)\}',
                        (SOURCES / 'tms320c54x_test_atomic_routes.h').read_text())
    for first, last, _ in ranges:
        points.update((int(first), int(last) + 1))
    expected = {}
    for phase in sorted(points):
        matches = {owner for owner, code in expressions
                   if eval(code, {'__builtins__': {}}, {'phase': phase})}
        if len(matches) > 1:
            raise ValueError(f'Overlapping atomic fixtures at phase {phase}')
        expected[phase] = next(iter(matches), 'none')
    return ranges, expected


class AtomicRoutingTests(unittest.TestCase):
    def test_guard_extraction_ignores_indentation_and_literal_braces(self):
        text = '''namespace fixture {
bool result() {
// } if (m_phase == 99) {
if (m_phase == 77)
{
    log("} if (fake) {");
    if (m_phase == 8) return true;
}
    if (m_phase >= 78 && m_phase < count(80)) { return true; }
}
}'''
        self.assertEqual(top_level_guards(text),
                         ['m_phase == 77', 'm_phase >= 78 && m_phase < count(80)'])

    def test_ranges_cover_exactly_the_fixture_guards(self):
        ranges, expected = guarded_partition()
        for phase, owner in expected.items():
            matches = [label for first, last, label in ranges if int(first) <= phase <= int(last)]
            with self.subTest(phase=phase):
                self.assertEqual(matches, [] if owner == 'none' else [owner])

    def test_compiled_router_matches_independent_partition(self):
        compiler = shutil.which('c++')
        if compiler is None:
            self.skipTest('C++ compiler unavailable')
        _, expected = guarded_partition()
        checks = '\n'.join(
            f'if (atomic_fixture_for({phase}) != atomic_fixture::{owner}) return 1;'
            for phase, owner in expected.items())
        source = '#include "tests/c54x/tms320c54x_test_atomic_routes.h"\nusing namespace c54x_test;\nint main() {\n' + checks + '\n}\n'
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / 'test.cpp').write_text(source)
            subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I', str(ROOT),
                            str(path / 'test.cpp'), '-o', str(path / 'test')],
                           check=True, capture_output=True)
            subprocess.run([str(path / 'test')], check=True, capture_output=True)


if __name__ == '__main__':
    unittest.main()

from pathlib import Path
import shutil
import tempfile
import unittest

from tools.c54x_dispatch_audit import ALL, Flow, Guards, Reader, clang_library, constant_alias, lex


try:
    LIBCLANG = clang_library()
except ValueError:
    LIBCLANG = None


def function(body):
    reader = Reader(lex(('void C::test() { ' + body + ' }').encode(), LIBCLANG))
    return reader, reader.function('test')


class GuardMap:
    def __init__(self, **bits):
        self.bits = bits

    def get(self, expression):
        return self.bits.get(expression)


@unittest.skipUnless(LIBCLANG, 'manual C++ census requires libclang')
class DispatchFlowTests(unittest.TestCase):
    def test_lexer_does_not_treat_comments_or_literals_as_branches(self):
        reader, root = function('/* if (fake) {} */ const char *s="if (; else }"; if (first) return; effects(op);')
        flow = Flow(reader, GuardMap(first=2), 'effects')
        fall, returned, _ = flow.run(root, 6)
        self.assertEqual((fall, returned), (4, 2))
        self.assertEqual(flow.calls[0][1], 4)

    def test_unknown_state_cannot_prove_an_opcode_absent(self):
        reader, root = function('if (state) return; effects(op);')
        flow = Flow(reader, GuardMap(), 'effects')
        self.assertEqual(flow.run(root, 6), (6, 6, 0))
        self.assertEqual(flow.calls[0][1], 6)

    def test_ordered_else_and_early_return(self):
        reader, root = function('if (first) return; else if (second) effects(op); else return;')
        flow = Flow(reader, GuardMap(first=2, second=6), 'effects')
        self.assertEqual(flow.run(root, 14), (4, 10, 0))
        self.assertEqual(flow.calls[0][1], 4)

    def test_switch_labels_break_fallthrough_and_return(self):
        reader, root = function('switch (op) { case 1: case 2: effects(op); break; case 3: note(); default: return; } tail();')
        guards = GuardMap(**{'(op) == (1)': 2, '(op) == (2)': 4, '(op) == (3)': 8})
        flow = Flow(reader, guards, 'effects')
        self.assertEqual(flow.run(root, 30), (6, 24, 0))
        self.assertEqual(flow.calls[0][1], 6)
        arms = [row for row in flow.handlers if row['node'].kind == 'arm']
        self.assertEqual([row['matched'] for row in arms], [6, 8, 24])

    def test_unknown_switch_and_unmodeled_jumps_fail_closed(self):
        reader, root = function('switch (state) { case 1: effects(op); break; }')
        with self.assertRaisesRegex(ValueError, 'opcode-only'):
            Flow(reader, GuardMap(), 'effects').run(root, 2)
        for statement in ('goto label;', 'do {} while (state);', '#if STATE\nreturn;\n#endif'):
            with self.subTest(statement=statement), self.assertRaises(ValueError):
                function(statement)

    def test_alias_initializers_come_from_actual_constant_declarations(self):
        reader, root = function('u8 const low=op; bool const narrow=(low & 3)==2;')
        self.assertEqual(constant_alias(reader, root, 'low'), ('low', 'u8 const low = op ;', 'op'))
        self.assertEqual(constant_alias(reader, root, 'narrow')[2], '( low & 3 ) == 2')
        with self.assertRaises(ValueError):
            constant_alias(reader, root, 'missing')


@unittest.skipUnless(shutil.which('g++'), 'compiled guard test requires g++ and MAME headers')
class CompiledGuardTests(unittest.TestCase):
    def test_actual_cpp_widths_and_decode_table_are_used(self):
        header = Path(__file__).resolve().parents[1]/'cpu/tms320c54x/tms320c54x_decode.h'
        guards = Guards(header, [('low', 'u8 const low=op;', 'op')])
        self.assertTrue(guards.collect('(op & 0xff00) == 0xe800'))
        self.assertTrue(guards.collect('low == 0xff'))
        self.assertFalse(guards.collect('m_st1 & 0x800'))
        with tempfile.TemporaryDirectory() as directory:
            modes = guards.compile(Path(directory), 'g++')
        self.assertEqual(guards.get('(op & 0xff00) == 0xe800').bit_count(), 256)
        self.assertEqual(guards.get('low == 0xff').bit_count(), 256)
        self.assertEqual(guards.get('low == 0xff') & 1, 0)
        self.assertTrue(all(0 < mode.bit_count() <= ALL.bit_count() for mode in modes))


if __name__ == '__main__':
    unittest.main()

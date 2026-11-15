from pathlib import Path
import unittest

from tools.c54x_stage_gates import (DECODE, DISPATCH, HEADER, SOURCE, convert_chain, execute_cases, gates, header, opcode_test,
                                    pure_predicates)

ROOT = Path(__file__).resolve().parents[1]
PURE = {'staged_move', 'staged_banz', 'BIT', 'u8'}


def source(*blocks):
    body = '\n'.join(blocks)
    return f'void f()\n{{\n\tif (BIT(gates, 0)) // stage gate 0\n\t{{\n{body}\n\t}}\n}}\n'


class OpcodeTestTest(unittest.TestCase):
    def test_takes_the_first_word_only_conjunct(self):
        self.assertEqual(opcode_test('m_pipeline_valid[3] && staged_move(m_pipeline_word[3]) && m_x', PURE),
                         (3, 'staged_move(word)'))

    def test_accepts_masks_and_disjunctions(self):
        self.assertEqual(opcode_test('m_pipeline_valid[4] && ((m_pipeline_word[4] & 0xfff8) == 0x7688 || staged_banz(m_pipeline_word[4]))', PURE),
                         (4, '((word & 0xfff8) == 0x7688 || staged_banz(word))'))

    def test_rejects_a_test_after_a_call_with_side_effects(self):
        self.assertIsNone(opcode_test('pipeline_execute() && staged_move(m_pipeline_word[3])', PURE))

    def test_rejects_decode_stage_and_mixed_words(self):
        self.assertIsNone(opcode_test('m_pipeline_valid[2] && staged_move(m_pipeline_word[2])', PURE))
        self.assertIsNone(opcode_test('m_pipeline_word[3] == m_pipeline_word[4]', PURE))

    def test_rejects_a_test_that_reads_other_state(self):
        self.assertIsNone(opcode_test('staged_move(m_pipeline_word[3]) == m_flag', PURE))


class GateParseTest(unittest.TestCase):
    def test_collects_each_blocks_test(self):
        text = source('\t\tif (m_pipeline_valid[3] && staged_move(m_pipeline_word[3]))\n\t\t\tx = 1;',
                      '\t\tif (m_pipeline_valid[5] && u8(m_pipeline_word[5]) == 0xf8)\n\t\t{\n\t\t\ty();\n\t\t}')
        self.assertEqual(gates(text, PURE), {0: [(3, 'staged_move(word)'), (5, 'u8(word) == 0xf8')]})

    def test_rejects_an_else(self):
        text = source('\t\tif (staged_move(m_pipeline_word[3]))\n\t\t\tx = 1;\n\t\telse\n\t\t\tx = 2;')
        with self.assertRaisesRegex(ValueError, 'else'):
            gates(text, PURE)

    def test_rejects_a_block_without_an_opcode_test(self):
        with self.assertRaisesRegex(ValueError, 'no opcode test'):
            gates(source('\t\tif (m_flag)\n\t\t\tx = 1;'), PURE)

    def test_rejects_a_statement_between_blocks(self):
        text = source('\t\tx = 0;', '\t\tif (staged_move(m_pipeline_word[3]))\n\t\t\tx = 1;')
        with self.assertRaisesRegex(ValueError, 'outside its blocks'):
            gates(text, PURE)

    def test_committed_header_is_current(self):
        pure = pure_predicates((ROOT / DECODE).read_text())
        text = (ROOT / SOURCE).read_text()
        self.assertEqual((ROOT / HEADER).read_text(), header(gates(text, pure), execute_cases(text, pure) if DISPATCH in text else ()))


CHAIN = '''bool f()
{
\tif (staged_move(m_op))
\t{
\t\ta();
\t}
\t// The second form.
\telse if ((m_op & 0xff00) == 0x7700)
\t\tb(); // trailing note
\telse if (!staged_banz(m_op))
\t\t// Why c.
\t\tc();
\tafter();
}
'''


class ExecuteDispatchTest(unittest.TestCase):
    def test_converts_a_chain_to_first_matching_cases(self):
        converted = convert_chain(CHAIN, 3)
        self.assertIn('\t// The second form.\n\tcase 1: // (m_op & 0xff00) == 0x7700\n\t\tb(); // trailing note\n\t\tbreak;', converted)
        self.assertIn('\t}\n\tafter();', converted)
        self.assertIn('\tcase 2: // !staged_banz(m_op)\n\t\t// Why c.\n\t\tc();\n\t\tbreak;', converted)
        self.assertEqual(execute_cases(converted, PURE), ['staged_move(word)', '(word & 0xff00) == 0x7700', '!staged_banz(word)'])

    def test_keeps_a_final_else_as_default(self):
        converted = convert_chain(CHAIN.replace('\telse if (!staged_banz(m_op))', '\telse'), 3)
        self.assertIn('\tdefault:\n\t\t// Why c.\n\t\tc();\n\t\tbreak;', converted)

    def test_rejects_a_case_that_reads_state(self):
        converted = convert_chain(CHAIN.replace('staged_move(m_op)', 'staged_move(m_op) && m_flag'), 3)
        with self.assertRaisesRegex(ValueError, 'more than m_op'):
            execute_cases(converted, PURE)

    def test_rejects_a_device_member_predicate(self):
        converted = convert_chain(CHAIN.replace('staged_move(m_op)', 'staged_bus_control(m_op)'), 3)
        with self.assertRaisesRegex(ValueError, 'more than m_op'):
            execute_cases(converted, PURE)


if __name__ == '__main__':
    unittest.main()

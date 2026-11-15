from pathlib import Path
import unittest

from tools.c54x_stage_gates import (DECODE, DECODED, DEVICE, HEADER, SOURCE, convert_chain, decode_gates, decode_test, execute_cases,
                                    gates, generate, opcode_test, pure_predicates, side_effect_free)

ROOT = Path(__file__).resolve().parents[1]
PURE = {'staged_move', 'staged_banz', 'BIT', 'u8'}


def source(*blocks):
    body = '\n'.join(blocks)
    return f'void f()\n{{\n\tif (BIT(gates, 0)) // stage gate 0\n\t{{\n{body}\n\t}}\n}}\n'


class OpcodeTestTest(unittest.TestCase):
    def test_takes_the_first_word_only_conjunct(self):
        self.assertEqual(opcode_test('m_slot[3].valid && staged_move(m_slot[3].word) && m_x', PURE),
                         (3, 'staged_move(word)'))

    def test_accepts_masks_and_disjunctions(self):
        self.assertEqual(opcode_test('m_slot[4].valid && ((m_slot[4].word & 0xfff8) == 0x7688 || staged_banz(m_slot[4].word))', PURE),
                         (4, '((word & 0xfff8) == 0x7688 || staged_banz(word))'))

    def test_rejects_a_test_after_a_call_with_side_effects(self):
        self.assertIsNone(opcode_test('pipeline_execute() && staged_move(m_slot[3].word)', PURE))

    def test_rejects_decode_stage_and_mixed_words(self):
        self.assertIsNone(opcode_test('m_slot[2].valid && staged_move(m_slot[2].word)', PURE))
        self.assertIsNone(opcode_test('m_slot[3].word == m_slot[4].word', PURE))

    def test_rejects_a_test_that_reads_other_state(self):
        self.assertIsNone(opcode_test('staged_move(m_slot[3].word) == m_flag', PURE))


class GateParseTest(unittest.TestCase):
    def test_collects_each_blocks_test(self):
        text = source('\t\tif (m_slot[3].valid && staged_move(m_slot[3].word))\n\t\t\tx = 1;',
                      '\t\tif (m_slot[5].valid && u8(m_slot[5].word) == 0xf8)\n\t\t{\n\t\t\ty();\n\t\t}')
        self.assertEqual(gates(text, PURE), {0: [(3, 'staged_move(word)'), (5, 'u8(word) == 0xf8')]})

    def test_rejects_an_else(self):
        text = source('\t\tif (staged_move(m_slot[3].word))\n\t\t\tx = 1;\n\t\telse\n\t\t\tx = 2;')
        with self.assertRaisesRegex(ValueError, 'else'):
            gates(text, PURE)

    def test_rejects_a_block_without_an_opcode_test(self):
        with self.assertRaisesRegex(ValueError, 'no opcode test'):
            gates(source('\t\tif (m_flag)\n\t\t\tx = 1;'), PURE)

    def test_rejects_a_statement_between_blocks(self):
        text = source('\t\tx = 0;', '\t\tif (staged_move(m_slot[3].word))\n\t\t\tx = 1;')
        with self.assertRaisesRegex(ValueError, 'outside its blocks'):
            gates(text, PURE)

    def test_committed_header_is_current(self):
        decode = (ROOT / DECODE).read_text()
        allowed = side_effect_free(decode, (ROOT / DEVICE).read_text())
        self.assertEqual((ROOT / HEADER).read_text(), generate((ROOT / SOURCE).read_text(), pure_predicates(decode), allowed))


ALLOWED = PURE | {'staged_clock_destination_move', 'pipeline_early_braf_clear'}


def decode_source(*blocks, before=''):
    body = '\n'.join(blocks)
    return (f'void f()\n{{\n\tu16 const word = m_slot[2].word;\n{before}'
            f'\tif (BIT(decode_gates, 0)) // decode gate 0\n\t{{\n{body}\n\t}}\n}}\n')


class DecodeGateTest(unittest.TestCase):
    def test_reads_through_locals_and_facts(self):
        local = {'direct_move': 'fact(decode_fact::direct_move)', 'brc_move': 'direct_move && m_slot[2].immediate == 0x1a'}
        self.assertEqual(decode_test('m_slot[2].valid && brc_move && m_flag', PURE, local),
                         '(BIT(decode_facts[word], unsigned(decode_fact::direct_move)))')

    def test_joins_every_word_test_before_a_side_effect(self):
        self.assertEqual(decode_test('word >= 0x8a1a && m_flag && word <= 0x8a1c', PURE, {}), 'word >= 0x8a1a && word <= 0x8a1c')
        self.assertEqual(decode_test('staged_move(word) && pipeline_execute() && u8(word) == 0xf8', PURE, {}), 'staged_move(word)')
        self.assertIsNone(decode_test('pipeline_execute() && staged_move(word)', PURE, {}))

    def test_a_qualified_fact_name_is_not_a_local(self):
        local = {'short_repeat': 'fact(decode_fact::short_repeat)'}
        self.assertEqual(decode_test('short_repeat', PURE, local), '(BIT(decode_facts[word], unsigned(decode_fact::short_repeat)))')

    def test_collects_gates_with_declarations_between_blocks(self):
        text = decode_source('\t\tif (m_slot[2].valid && staged_move(word) && m_x)\n\t\t\tx = 1;',
                             '\t\tbool const clock_move = staged_move(word) && staged_clock_destination_move(word, m_slot[2].immediate);',
                             '\t\tif (clock_move && m_y)\n\t\t\ty = 1;',
                             before='\tbool const short_repeat = fact(decode_fact::short_repeat);\n')
        self.assertEqual(decode_gates(text.replace('m_x)', 'short_repeat)'), PURE, ALLOWED),
                         {0: ['staged_move(word) && (BIT(decode_facts[word], unsigned(decode_fact::short_repeat)))', 'staged_move(word)']})

    def test_rejects_a_declaration_with_a_side_effect(self):
        text = decode_source('\t\tbool const moved = pipeline_execute();', '\t\tif (staged_move(word))\n\t\t\tx = 1;')
        with self.assertRaisesRegex(ValueError, 'outside its blocks'):
            decode_gates(text, PURE, ALLOWED)

    def test_rejects_a_trailing_statement(self):
        text = decode_source('\t\tif (staged_move(word))\n\t\t\tx = 1;', '\t\tm_x = 0;')
        with self.assertRaisesRegex(ValueError, 'outside its blocks'):
            decode_gates(text, PURE, ALLOWED)

    def test_rejects_a_block_that_reads_state_first(self):
        with self.assertRaisesRegex(ValueError, 'no opcode test'):
            decode_gates(decode_source('\t\tif (m_flag)\n\t\t\tx = 1;'), PURE, ALLOWED)


STEP = 'void tms320c54x_device::pipeline_step()\n{\n'


class DecodedGateTest(unittest.TestCase):
    def test_accepts_decode_word_tests_after_the_marker(self):
        text = STEP + '\tm_slot[2].word = 0;\n' + DECODED + source('\t\tif (m_slot[2].valid && staged_move(m_slot[2].word))\n\t\t\tx = 1;')[len('void f()\n{\n'):]
        self.assertEqual(gates(text, PURE), {0: [(2, 'staged_move(word)')]})

    def test_rejects_decode_word_tests_before_the_marker(self):
        text = STEP + source('\t\tif (staged_move(m_slot[2].word))\n\t\t\tx = 1;')[len('void f()\n{\n'):] + DECODED
        with self.assertRaisesRegex(ValueError, 'no opcode test'):
            gates(text, PURE)

    def test_rejects_a_slot_write_after_the_marker(self):
        for write in ('\tm_slot[2].word = 0xf495;\n', '\tm_slot[stage] = m_slot[stage - 1];\n'):
            with self.assertRaisesRegex(ValueError, 'writes a slot word after the Decode marker'):
                gates(STEP + DECODED + write + '}\n', PURE)


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

import unittest

from tools.run_c54x_core import (COMPLETION, REQUIRED, check_result,
                                BUS_REGISTER_REQUIRED, BUS_REGISTER_COMPLETION)


class CoreResultTests(unittest.TestCase):
    def test_bus_register_scope_is_separate(self):
        output = BUS_REGISTER_REQUIRED + '\n' + BUS_REGISTER_COMPLETION + '\n'
        check_result(output, 3, bus_registers=True)
        with self.assertRaises(ValueError):
            check_result(output, 3)
        for bad in (output.replace('io_waits_implemented=0', 'io_waits_implemented=1'),
                    output.replace('physical_mapping=unvalidated', 'physical_mapping=validated'),
                    output.replace('encodings=65536', 'encodings=65535'),
                    BUS_REGISTER_REQUIRED + '\n' + output,
                    output + 'late output\n', output.replace(BUS_REGISTER_COMPLETION, COMPLETION)):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, bus_registers=True)

    def setUp(self):
        self.output = '\n'.join([*REQUIRED, COMPLETION]) + '\n'

    def test_complete(self):
        check_result(self.output, 3)

    def test_existing_prefix_markers_allow_detail(self):
        check_result(self.output.replace(
            'stack address latency conformance: PASS\n',
            'stack address latency conformance: PASS variants=24\n'), 3)

    def test_wrong_exit(self):
        for status in (0, 1, -11):
            with self.subTest(status=status), self.assertRaises(ValueError):
                check_result(self.output, status)

    def test_each_marker_required(self):
        for marker in REQUIRED:
            with self.subTest(marker=marker), self.assertRaises(ValueError):
                check_result(self.output.replace(marker + '\n', ''), 3)

    def test_duplicate_marker(self):
        with self.assertRaises(ValueError):
            check_result(REQUIRED[0] + '\n' + self.output, 3)

    def test_old_interrupt_checkpoint_is_insufficient(self):
        with self.assertRaises(ValueError):
            check_result(self.output.replace(' intm_at_iack=1', ''), 3)

    def test_checkpoint_without_decode_order_is_insufficient(self):
        with self.assertRaises(ValueError):
            check_result(self.output.replace(' decode_before_access=1', ''), 3)

    def test_completion_must_be_terminal(self):
        for output in (self.output.replace(COMPLETION, ''), self.output + 'crash\n'):
            with self.assertRaises(ValueError):
                check_result(output, 3)

    def test_other_fatal_error_rejected(self):
        with self.assertRaises(ValueError):
            check_result('Fatal error: unexpected failure\n' + self.output, 3)


if __name__ == '__main__':
    unittest.main()

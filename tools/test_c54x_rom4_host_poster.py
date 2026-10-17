import unittest

from tools.c54x_rom4_host_poster import check_case, parameter_contract, SHADOWS


def case(field, payload=0x9209, notify=1):
    parameters = [] if field == 0x2e else [dict(address=0x100aa if field == 0x1b else 0x100a8,
                                               size=2, value=payload if field == 0x1b else (field << 12) | (payload & 0xfff))]
    if notify or field == 0x2e:
        parameters += [dict(address=address, size=size, value=value)
                       for address, size, value in ((0x100e0, 2, 1), (0x30000, 2, 4), (0x20008, 1, 2))]
    private = [dict(address=SHADOWS[field - 8], size=2, value=(field << 12) | (payload & 0xfff))] if field in (8, 9) else []
    return dict(field=field, payload=payload, notify=notify, result=1,
                writes=parameters, shadow_writes=private)


class HostPosterTests(unittest.TestCase):
    def test_full_selector_eight_and_nine_initialize_the_real_shadows(self):
        self.assertEqual(parameter_contract(8, 0x8426, 0xffff),
                         ([(0x100a8, 0x8426)], [(0x10a9e6, 0x8426)]))
        self.assertEqual(parameter_contract(9, 0x9209, 0xffff),
                         ([(0x100a8, 0x9209)], [(0x10a9e8, 0x9209)]))
        broken = case(8)
        broken['shadow_writes'] = []
        with self.assertRaisesRegex(ValueError, 'private shadow'):
            check_case(broken)

    def test_subfields_preserve_shadow_not_the_unrelated_hpi_word(self):
        self.assertEqual(parameter_contract(0x14, 0, 0xffff, (0x842e, 0x9209)),
                         ([(0x100a8, 0x8426)], [(0x10a9e6, 0x8426)]))
        self.assertEqual(parameter_contract(0x1a, 12, 0, (0x8426, 0x9209)),
                         ([(0x100a8, 0x920c)], [(0x10a9e8, 0x920c)]))
        valid = dict(field=0x1a, payload=12, notify=0, retained=0,
                     shadows=[0x8426, 0x9209], result=1,
                     writes=[dict(address=0x100a8, size=2, value=0x920c)],
                     shadow_writes=[dict(address=0x10a9e8, size=2, value=0x920c)])
        check_case(valid)
        with self.assertRaisesRegex(ValueError, 'parameter transformation'):
            check_case(dict(valid, writes=[dict(address=0x100a8, size=2, value=12)]))
        with self.assertRaisesRegex(ValueError, 'private shadow'):
            check_case(dict(valid, shadow_writes=[]))

    def test_single_bit_subfields_use_the_low_bit_not_truthiness(self):
        for field, mask in zip(range(0x10, 0x17), (0x400, 0x200, 0x20, 0x10, 8, 4, 1)):
            self.assertEqual(parameter_contract(field, 2, 0, (0xffff, 0)),
                             ([(0x100a8, 0xffff ^ mask)], [(0x10a9e6, 0xffff ^ mask)]))

    def test_other_masked_and_whole_word_fields(self):
        examples = {
            0x1c: (0x100ac, 0x007a), 0x1d: (0x100ac, 0x005b),
            0x1e: (0x100ac, 0x005a), 0x1f: (0x100ac, 0x004e),
            0x20: (0x100ae, 0x1203), 0x23: (0x100b4, 0x1203),
            0x26: (0x100be, 0x035a), 0x27: (0x100be, 0xa503),
            0x2a: (0x100ba, 0xa55a), 0x2b: (0x100ba, 0xe55a),
            0x2c: (0x100ba, 0xa553), 0x2d: (0x100be, 0x9203),
        }
        for field, expected in examples.items():
            with self.subTest(field=field):
                self.assertEqual(parameter_contract(field, 0x9203, 0xa55a), ([expected], []))

    def test_busy_rejects_even_trigger_only_case_without_stores(self):
        for field in range(47):
            for busy in (1, 0xffff):
                valid = dict(field=field, payload=0x9209, notify=1, busy=busy, result=0, writes=[])
                check_case(valid)
                with self.assertRaisesRegex(ValueError, 'reject'):
                    check_case(dict(valid, result=1))
                with self.assertRaisesRegex(ValueError, 'stores'):
                    check_case(dict(valid, writes=case(0x2e)['writes']))

    def test_invalid_field_rejects_without_stores(self):
        for field in (0x2f, 0xffff):
            valid = dict(field=field, payload=0xffff, notify=1, busy=0, result=0, writes=[])
            check_case(valid)
            with self.assertRaisesRegex(ValueError, 'reject'):
                check_case(dict(valid, result=1))
            with self.assertRaisesRegex(ValueError, 'stores'):
                check_case(dict(valid, writes=case(0x2e)['writes']))

    def test_packed_selectors_preserve_payload_not_retained_word(self):
        for field in range(16):
            for payload in (0, 1, 0x9209, 0xffff):
                for notify in (0, 1):
                    check_case(case(field, payload, notify))
        broken = case(9)
        broken['writes'][0]['value'] = 0
        with self.assertRaisesRegex(ValueError, 'packed'):
            check_case(broken)

    def test_software_shared_word_and_trigger_only_case(self):
        check_case(case(0x1b))
        check_case(case(0x2e, notify=0))
        broken = case(0x1b)
        broken['writes'][0]['address'] = 0x100a8
        with self.assertRaisesRegex(ValueError, 'field 1b'):
            check_case(broken)
        broken = case(0x2e)
        broken['writes'].insert(0, dict(address=0x100a8, size=2, value=1))
        with self.assertRaisesRegex(ValueError, 'trigger-only'):
            check_case(broken)

    def test_notification_sequence_is_exact(self):
        for index in range(1, 4):
            for key in ('address', 'size', 'value'):
                broken = case(9)
                broken['writes'][index][key] = 0
                with self.subTest(index=index, key=key), self.assertRaises(ValueError):
                    check_case(broken)
        broken = case(9)
        broken['writes'][-2:] = reversed(broken['writes'][-2:])
        with self.assertRaises(ValueError):
            check_case(broken)
        broken = case(9, notify=0)
        broken['writes'].append(dict(address=0x30000, size=2, value=4))
        with self.assertRaises(ValueError):
            check_case(broken)

    def test_unclassified_stores_and_failed_return_rejected(self):
        broken = case(9)
        broken['writes'].insert(0, dict(address=0x30002, size=2, value=4))
        with self.assertRaisesRegex(ValueError, 'unclassified'):
            check_case(broken)
        broken = case(9)
        broken['result'] = 0
        with self.assertRaisesRegex(ValueError, 'success'):
            check_case(broken)


if __name__ == '__main__':
    unittest.main()

import unittest

from tools.c54x_rom4_host_poster import check_case


def case(field, payload=0x9209, notify=1):
    parameters = [] if field == 0x2e else [dict(address=0x100aa if field == 0x1b else 0x100a8,
                                               size=2, value=payload if field == 0x1b else (field << 12) | (payload & 0xfff))]
    if notify or field == 0x2e:
        parameters += [dict(address=address, size=size, value=value)
                       for address, size, value in ((0x100e0, 2, 1), (0x30000, 2, 4), (0x20008, 1, 2))]
    return dict(field=field, payload=payload, notify=notify, result=1, writes=parameters)


class HostPosterTests(unittest.TestCase):
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

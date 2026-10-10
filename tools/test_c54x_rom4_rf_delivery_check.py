import unittest

from tools.c54x_rom4_rf_delivery_check import (check, check_request, check_tuning,
    check_tuning_stages, pack_tuning_nonnegative, decode_search_bitmap)


TRACE = """rom4_rf_native_publication: t=1.0 type=008a bytes=2 words=006d
rom4_rf_native_publish_exit: address=3807 t=1.1 type=008a bytes=2 producer=00ab consumer=00a9
rom4_rf_mcu_ring_write: t=1.2 address=000101c8 value=000000aa mask=0000ffff
rom4_rf_mcu_ring_write: t=1.3 address=000101c8 value=000000ab mask=0000ffff
rom4_rf_status_header_write: t=1.4 pc=0026be48 address=00101438 value=13900000 mask=ffff0000
rom4_rf_post_fetch: t=1.5 address=00254c84 pc=00254c84 r0=0000000a r1=00101438 lr=0026be5d
rom4_rf_task10_lifecycle: t=1.6 pc=00211a9a r0=00101438 status=1390
"""


class DeliveryTests(unittest.TestCase):
    def test_packing_reference(self):
        self.assertEqual(pack_tuning_nonnegative(0x1413), 0x302813)
        for quotient in (0, 1, 0x50, 0x3ff):
            for remainder in range(64):
                self.assertEqual(pack_tuning_nonnegative(quotient * 64 + remainder),
                                 (0x30 << 16) | (quotient << 7) | remainder)
        for value in (-1, 0x10000):
            with self.assertRaises(ValueError):
                pack_tuning_nonnegative(value)

    def test_packing_stages(self):
        fields = "ar2=0031 flags6be=0010 flags216a=0004 addend216b=13a6 selected1867=006d"
        stages = [("3ffb", "6d", "4", "32"), ("400f", "6d", "13a6", "32"),
                  ("402e", "1413", "13a6", "32"), ("403a", "1413", "13a6", "32"),
                  ("4048", "302813", "302813", "50")]
        lines = [f"rom4_rf_tuning_stage: t={i} address={address} a={a} b={b} field60={field} {fields}\n"
                 for i, (address, a, b, field) in enumerate(stages)]
        ports = """rom4_rf_tuning_write: t=5 pc=4023 port=0031 value=2813 a=00302813 ar2=0031 selected1867=006d
rom4_rf_tuning_write: t=6 pc=4028 port=0032 value=0030 a=00302813 ar2=0031 selected1867=006d
"""
        trace = "".join(lines) + ports
        self.assertEqual(check_tuning_stages(trace), 0x302813)
        for index in range(len(lines)):
            with self.subTest(index=index), self.assertRaises(ValueError):
                check_tuning_stages("".join(lines[:index] + lines[index + 1:]) + ports)
        for old, new in (("a=1413", "a=1414"), ("flags6be=0010", "flags6be=0090"),
                         ("flags216a=0004", "flags216a=0006"), ("field60=50", "field60=51"),
                         ("b=13a6", "b=13a7")):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_tuning_stages(trace.replace(old, new))

    def test_tuning_accumulator_pair(self):
        trace = """rom4_rf_tuning_write: t=1.0 pc=4023 port=0031 value=2813 a=0000302813 ar2=0031 selected1867=006d
rom4_rf_tuning_write: t=1.1 pc=4028 port=0032 value=0030 a=0000302813 ar2=0031 selected1867=006d
"""
        self.assertEqual(check_tuning(trace), 0x302813)
        for old, new in (("value=2813", "value=2814"),
                         ("value=0030", "value=0031"),
                         ("pc=4028", "pc=4023"), ("t=1.1", "t=0.9"),
                         ("selected1867=006d", "selected1867=0000")):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_tuning(trace.replace(old, new))

    def test_request_selection_contract(self):
        words = [0x10, 0x9800] + [0] * 31
        words[26] = 0x1000
        request = "rom4_rf_search_request: t=1.0 ar2=0816 words=" + ",".join(f"{word:04x}" for word in words) + "\n"
        scan = "rom4_rf_scan_entry: address=3129 t=1.1 selected1867=006d cursor_word=006d\n"
        self.assertEqual(check_request(request + scan), 109)
        for text in (scan, request, scan + request,
                     request + scan.replace("t=1.1", "t=0.9"),
                     request + scan.replace("selected1867=006d", "selected1867=0001"),
                     request + scan.replace("cursor_word=006d", "cursor_word=7fff"),
                     request.replace("words=0010", "words=0011") + scan,
                     request.replace("1000", "1001") + scan):
            with self.subTest(text=text), self.assertRaises(ValueError):
                check_request(text)

    def test_captured_bitmap(self):
        words = [0x10, 0x9800] + [0] * 31
        words[26] = 0x1000
        self.assertEqual(decode_search_bitmap(words), [109])

    def test_bitmap_basis(self):
        for group in range(32):
            for bit in range(16):
                words = [0x10, 0] + [0] * 31
                words[32 - group] = 1 << bit
                expected = (1 + group * 16 if group < 8 else 512 + (group - 8) * 16) + bit
                if group == 31 and bit >= 8:
                    expected_entries = []  # High byte of word 1 is not copied into scratch.
                else:
                    expected_entries = [expected]
                self.assertEqual(decode_search_bitmap(words), expected_entries)

    def test_bitmap_rejects_wrong_contract(self):
        for words in ([0] * 32, [1] + [0] * 32, [0x10000] + [0] * 32):
            with self.assertRaises(ValueError):
                decode_search_bitmap(words)

    def test_complete_chain(self):
        self.assertEqual(check(TRACE), ("00101438", "006d"))

    def test_every_link_required(self):
        lines = TRACE.splitlines()
        for index in range(len(lines)):
            with self.subTest(index=index), self.assertRaises(ValueError):
                check("\n".join(lines[:index] + lines[index + 1:]))

    def test_identity_and_order(self):
        for old, new in (("r1=00101438", "r1=00101480"),
                         ("status=1390", "status=138f"),
                         ("t=1.6", "t=0.5"),
                         ("producer=00ab", "producer=00ac"),
                         ("mask=0000ffff", "mask=ffff0000")):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check(TRACE.replace(old, new))

    def test_wrap(self):
        wrapped = TRACE.replace("consumer=00a9", "consumer=00e3")
        wrapped = wrapped.replace("producer=00ab", "producer=0081")
        wrapped = wrapped.replace("value=000000aa", "value=00000080")
        wrapped = wrapped.replace("value=000000ab", "value=00000081")
        self.assertEqual(check(wrapped)[1], "006d")


if __name__ == "__main__":
    unittest.main()

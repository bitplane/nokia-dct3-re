import unittest
from types import SimpleNamespace

from tools import dct3_type_1f_static_check as check


class Dct3Type1fStaticCheckTests(unittest.TestCase):
    def test_constructor_census_is_register_independent(self):
        instructions = [
            SimpleNamespace(
                address=0x100,
                mnemonic="movs",
                op_str="r2, #0x1f",
            ),
            SimpleNamespace(
                address=0x102,
                mnemonic="strb",
                op_str="r2, [r1, #3]",
            ),
            SimpleNamespace(
                address=0x104,
                mnemonic="movs",
                op_str="r0, #0x1f",
            ),
            SimpleNamespace(
                address=0x106,
                mnemonic="strb",
                op_str="r1, [r4, #3]",
            ),
        ]
        self.assertEqual(
            [[0x100, 0x102]],
            check.direct_type_0x1f_constructors(instructions),
        )

    def test_exact_model_censuses_remain_distinct(self):
        self.assertEqual(
            [4, 1, 5],
            [
                len(check.EXPECTED[name]["constructors"])
                for name in (
                    "nse8_3210_v600",
                    "nhm5_3310_v639",
                    "nse3_6110_v406",
                )
            ],
        )

    def test_all_model_forms_are_accounted_for(self):
        for expected in check.EXPECTED.values():
            self.assertTrue(expected["task_post_calls"])
            self.assertTrue(
                all(
                    form["site"] in {
                        pair[0] for pair in expected["constructors"]
                    }
                    for form in expected["forms"]
                )
            )

    def test_swap16_is_involutive(self):
        value = bytes.fromhex("12345678")
        self.assertEqual(value, check.swap16(check.swap16(value)))


if __name__ == "__main__":
    unittest.main()

import unittest

from tools.mad2_active_reset_check import PREPARED, PASS, check


class ActiveResetCheckTests(unittest.TestCase):
    def test_complete(self):
        check(f"[luaengine] {PREPARED}\n[luaengine] {PASS}\n")

    def test_rejects_incomplete_duplicate_reversed_or_lua_failure(self):
        for text, console in ((PREPARED, ""), (PASS, ""),
                              (f"{PASS}\n{PREPARED}", ""),
                              (f"{PREPARED}\n{PASS}\n{PASS}", ""),
                              (f"{PREPARED}\n{PASS}", "[LUA ERROR] stopped")):
            with self.subTest(text=text), self.assertRaises(ValueError):
                check(text, console)

    def test_each_result_field_required(self):
        for field in PASS.split()[2:]:
            with self.subTest(field=field), self.assertRaises(ValueError):
                check(PREPARED + "\n" + PASS.replace(field, field + "0"))

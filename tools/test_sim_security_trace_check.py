import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import make_sim_card_profile as profile
from sim_security_trace_check import CHV_OFFSET, ATTEMPTS_OFFSET, validate


class SimSecurityTraceCheckTest(unittest.TestCase):
    def test_successful_verify(self):
        nvram = profile.make_profile(True)
        trace = "SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1\n"
        validate(trace, nvram, "verify", "1234")

    def test_block_and_unblock(self):
        nvram = bytearray(profile.make_profile(True))
        nvram[CHV_OFFSET:CHV_OFFSET + 8] = b"4321\xff\xff\xff\xff"
        trace = "\n".join((
            "SIM status ins=20 sw=9804 chv=2/3 puk=10/10 enabled=1",
            "SIM status ins=20 sw=9804 chv=1/3 puk=10/10 enabled=1",
            "SIM status ins=20 sw=9840 chv=0/3 puk=10/10 enabled=1",
            "SIM status ins=2c sw=9000 chv=3/3 puk=10/10 enabled=1",
        ))
        validate(trace, bytes(nvram), "block-unblock", "4321")

    def test_retry_state_survives_machine_round_trip(self):
        trace = "\n".join((
            "SIM status ins=20 sw=9804 chv=2/3 puk=10/10 enabled=1",
            "state_roundtrip: result=pass timer_delta=0000 mode=0004 requested_at=10.0 t=10.1",
            "SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1",
        ))
        validate(trace, profile.make_profile(True), "retry-restore", "1234")

    def test_retry_state_requires_round_trip_marker(self):
        trace = "\n".join((
            "SIM status ins=20 sw=9804 chv=2/3 puk=10/10 enabled=1",
            "SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1",
        ))
        with self.assertRaisesRegex(ValueError, "round trip"):
            validate(trace, profile.make_profile(True), "retry-restore", "1234")

    def test_removal_clears_session_authorization(self):
        trace = "\n".join((
            "SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1",
            "SIM lifecycle event=deactivate verified_before=1/0",
            "SIM lifecycle event=inactive verified=0/0",
        ))
        validate(trace, profile.make_profile(True), "removal", "1234")

    def test_disable_enable_toggle(self):
        disabled = "SIM status ins=26 sw=9000 chv=3/3 puk=10/10 enabled=0\n"
        enabled = "SIM status ins=28 sw=9000 chv=3/3 puk=10/10 enabled=1\n"
        validate(enabled, profile.make_profile(True), "toggle", "1234", disabled)

    def test_toggle_rejects_startup_verify_on_disabled_reboot(self):
        disabled = "SIM status ins=26 sw=9000 chv=3/3 puk=10/10 enabled=0\n"
        enabled = "\n".join((
            "SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=0",
            "SIM status ins=28 sw=9000 chv=3/3 puk=10/10 enabled=1",
        ))
        with self.assertRaisesRegex(ValueError, "unexpectedly required"):
            validate(enabled, profile.make_profile(True), "toggle", "1234", disabled)

    def test_change_pin_and_verify_after_reboot(self):
        changed = bytearray(profile.make_profile(True))
        changed[CHV_OFFSET:CHV_OFFSET + 8] = b"4321\xff\xff\xff\xff"
        first_boot = "\n".join((
            "SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1",
            "SIM status ins=24 sw=9000 chv=3/3 puk=10/10 enabled=1",
        ))
        second_boot = "SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1\n"
        validate(second_boot, bytes(changed), "change", "4321", first_boot)

    def test_rejected_change_consumes_retry_without_changing_pin(self):
        rejected = bytearray(profile.make_profile(True))
        rejected[ATTEMPTS_OFFSET] = 2
        trace = "\n".join((
            "SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1",
            "SIM status ins=24 sw=9804 chv=2/3 puk=10/10 enabled=1",
        ))
        validate(trace, bytes(rejected), "change-reject", "1234")

    def test_rejects_missing_retry_transition(self):
        with self.assertRaisesRegex(ValueError, "retry sequence"):
            validate("SIM status ins=20 sw=9840 chv=0/3 puk=10/10 enabled=1",
                     profile.make_profile(True), "block-unblock", "1234")

    def test_rejects_wrong_persistent_pin(self):
        with self.assertRaisesRegex(ValueError, "persistent PIN"):
            validate("SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1",
                     profile.make_profile(True), "verify", "4321")


if __name__ == "__main__":
    unittest.main()

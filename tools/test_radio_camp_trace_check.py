import unittest

from tools.radio_camp_trace_check import verify


GOOD = """
radio_candidate_table: object=00101800 status=0447 usable=01 arfcn=0001 rssi_id=00 acquisition_mode=00 task=10
radio_no_psw_left_decode: object=00101800 accept=03 lifecycle=03 phase=0c task=04
radio_channel_change_state: pc=00284f74 accept=03 controller=02 flags=00010011
radio_acquisition_transition: pc=00213c04 object=00101830 status=13a5 action=01 arg=01 sequence=04 lifecycle=03 controller=03 task=0b
radio_bcch_parse: channel=50 block=0010229a data=49 06 1b 00 01 00 f1 10 00 01 00 00
radio_si3_path: pc=00273e58 result=0010edb4 state=02 active=00 flags=0f/0f task=0c
"""


class RadioCampTraceCheckTest(unittest.TestCase):
    def test_accepts_ordered_checkpoint(self):
        verify(GOOD)

    def test_rejects_missing_acquisition_action(self):
        with self.assertRaisesRegex(ValueError, "task-11 acquisition action"):
            verify(GOOD.replace("action=01", "action=02"))

    def test_rejects_si_before_channel_change(self):
        lines = GOOD.strip().splitlines()
        reordered = "\n".join(lines[:2] + lines[4:] + lines[2:4])
        with self.assertRaisesRegex(ValueError, "SI3 identity"):
            verify(reordered)


if __name__ == "__main__":
    unittest.main()

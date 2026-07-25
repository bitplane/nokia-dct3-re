import unittest

from tools.radio_call_audio_wire_trace_check import verify


GOOD = """
dsp_hle: GSM service uplink sapi=0 pd=03 message=07 length=2 t=1
dsp_shared_write: off=0a8 old=900f data=860b pc=002909d6 t=2
dspif_transport: RX enqueue type=80 payload=34 data=b0120000170a00010000036009030f2b t=3
dsp_shared_write: off=0aa old=0000 data=ffff pc=002909d6 t=3.1
dsp_hle: TX packet type=1b radio_phase=service_uplink_request t=4
dsp_hle: TX packet type=1b radio_phase=service_uplink_request t=5
dsp_hle: TX packet type=1b radio_phase=service_uplink_request t=6
dsp_shared_write: off=0aa old=ffff data=0000 pc=002909d6 t=6.9
dsp_hle: GSM service uplink sapi=0 pd=03 message=25 length=5 t=7
dspif_transport: RX enqueue type=80 payload=34 data=b012000019f900010000038209032d2b t=8
dsp_hle: GSM service uplink sapi=0 pd=03 message=2a length=2 t=9
dsp_hle: TX packet type=02 radio_phase=release_channel_change t=10
dsp_shared_write: off=0a8 old=91ff data=840a pc=002909d6 t=11
dsp_shared_write: off=0a8 old=910f data=8002 pc=002909d6 t=12
"""


class RadioCallAudioWireTraceCheckTest(unittest.TestCase):
    def test_complete_wire_lifecycle(self):
        result = verify(GOOD)
        self.assertEqual(3, result["stable_tch_polls"])
        self.assertEqual(5, result["wire_transitions"])

    def test_rejects_missing_answer_word(self):
        with self.assertRaisesRegex(ValueError, "answered wire word"):
            verify(GOOD.replace("data=860b", "data=860a"))

    def test_rejects_missing_idle_restoration(self):
        with self.assertRaisesRegex(ValueError, "idle wire word"):
            verify(GOOD.replace("data=8002 pc=002909d6 t=12", "data=8003"))

    def test_rejects_treating_adjacent_control_as_static_acknowledgement(self):
        with self.assertRaisesRegex(ValueError, "active adjacent control"):
            verify(GOOD.replace("off=0aa old=0000 data=ffff", "off=0aa old=0000 data=0000"))


if __name__ == "__main__":
    unittest.main()

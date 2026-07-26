import unittest

from tools.radio_answered_call_lifecycle_trace_check import verify


GOOD = """
dsp_hle: GSM service uplink sapi=0 pd=03 message=07 length=2 t=1
dsp_audio_shadow_write: address=0011206c old=0002 data=0203 pc=0028d9a8 task=05 t=2
dsp_audio_shadow_write: address=0011206c old=0203 data=060b pc=0028dd1c task=05 t=3
dsp_shared_control: command=08 value=060b commit=1 caller=0028e074 task=05 t=4
dspif_transport: RX enqueue type=80 payload=34 data=b0120000170a00010000036009030f2b t=5
dsp_hle: TX packet type=1b radio_phase=service_uplink_request t=6
dsp_hle: TX packet type=1b radio_phase=service_uplink_request t=7
dsp_hle: TX packet type=1b radio_phase=service_uplink_request t=8
dsp_hle: GSM service uplink sapi=0 pd=03 message=25 length=5 t=9
dspif_transport: RX enqueue type=80 payload=34 data=b012000019f900010000038209032d2b t=10
dsp_hle: GSM service uplink sapi=0 pd=03 message=2a length=2 t=11
dsp_hle: TX packet type=02 radio_phase=release_channel_change t=12
dsp_audio_shadow_write: address=0011206c old=060b data=060a pc=0028d97e task=09 t=13
dsp_audio_shadow_write: address=0011206c old=060a data=040a pc=0028d986 task=09 t=14
dsp_shared_control: command=08 value=040a commit=1 caller=0028e074 task=09 t=15
dsp_audio_shadow_write: address=0011206c old=040a data=0002 pc=0028dcf6 task=09 t=16
dsp_shared_control: command=08 value=0002 commit=1 caller=0028e074 task=09 t=17
"""


class AnsweredCallLifecycleTraceCheckTest(unittest.TestCase):
    def test_complete_lifecycle(self):
        result = verify(GOOD)
        self.assertEqual(3, result["stable_tch_polls"])
        self.assertEqual(3, result["control_states"])
        self.assertEqual("split", result["connected_producer_path"])

    def test_accepts_observed_full_word_connected_producer(self):
        trace = GOOD.replace(
            "dsp_audio_shadow_write: address=0011206c old=0002 data=0203 "
            "pc=0028d9a8 task=05 t=2\n"
            "dsp_audio_shadow_write: address=0011206c old=0203 data=060b "
            "pc=0028dd1c task=05 t=3\n",
            "dsp_audio_shadow_write: address=0011206c old=040a data=060b "
            "pc=0028d9a8 task=05 t=3\n",
        )
        result = verify(trace)
        self.assertEqual("full-word", result["connected_producer_path"])

    def test_rejects_unidentified_connected_producer(self):
        with self.assertRaisesRegex(ValueError, "connected-state producer"):
            verify(GOOD.replace("pc=0028d9a8 task=05 t=2", "pc=0028d9aa task=05 t=2"))

    def test_requires_release_complete(self):
        with self.assertRaisesRegex(ValueError, "CC Release Complete"):
            verify(GOOD.replace("message=2a", "message=29"))

    def test_requires_idle_restoration(self):
        with self.assertRaisesRegex(ValueError, "idle-state"):
            verify(GOOD.replace("old=040a data=0002", "old=040a data=0003"))

    def test_requires_stable_answered_interval(self):
        with self.assertRaisesRegex(ValueError, "three organic TCH polls"):
            verify(GOOD.replace(
                "dsp_hle: TX packet type=1b radio_phase=service_uplink_request "
                "t=8\n", ""))


if __name__ == "__main__":
    unittest.main()

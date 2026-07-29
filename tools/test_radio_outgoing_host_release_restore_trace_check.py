import unittest

from tools.radio_outgoing_host_release_restore_trace_check import check


TRACE = """
GSM service downlink kind=13 sapi=0 pd=03 message=25 length=5 t=29.107
state_replay: phase=reference event=begin t=29.110
GSM service uplink sapi=0 pd=03 message=2d length=2 t=29.118
LAPDm service Channel Release acknowledged t=29.129
state_replay: phase=reference event=end t=30.110
state_replay: phase=restored event=begin t=29.110
GSM service uplink sapi=0 pd=03 message=2d length=2 t=29.118
LAPDm service Channel Release acknowledged t=29.129
state_replay: phase=restored event=end t=30.110
state_roundtrip: result=pass requested_at=29.110 t=29.110
PCH no-identity fill
"""


class HostReleaseRestoreTraceCheckTests(unittest.TestCase):
    def test_accepts_deterministic_release_replay(self):
        check(TRACE)

    def test_rejects_save_before_disconnect(self):
        with self.assertRaisesRegex(ValueError, "inside the release"):
            check(TRACE.replace("requested_at=29.110", "requested_at=29.100"))


if __name__ == "__main__":
    unittest.main()

import unittest

from tools.nse5_msid_reply_check import check


CODEC = ("nse5_compat_codec: input=be4c:f224:0016:0010:a8a9:aa46 "
         "table=57f4:b241:27f4:e4ea:55cf:fea0 "
         "schedule=b1b1:7373:e6e6:5a5a:abab:4747:8e8e:0d0d:1a1a:3434:6868:0b0b "
         "output=f4af:7937:041f:6f93:224c:65ab t=0.671833\n")
REPLY = ("nse5_compat_dsp_control: name=task2_message_received message=001043d4 "
         "bytes=0000007400120100340e0082f4af7937041f6f93224c65ab flags=cc t=0.673422\n")


class MsidReplyTests(unittest.TestCase):
    def test_observed_reply(self):
        self.assertEqual(check(CODEC + REPLY)[0]["decoded_groups"],
                         ["be4cf224", "00160010", "a8a9aa46"])

    def test_missing_reply(self):
        with self.assertRaises(ValueError):
            check(CODEC)

    def test_missing_encoder(self):
        with self.assertRaises(ValueError):
            check(REPLY)

    def test_changed_source(self):
        with self.assertRaisesRegex(ValueError, "disagrees"):
            check(CODEC.replace("input=be4c", "input=be4d") + REPLY)

    def test_late_encoder(self):
        with self.assertRaises(ValueError):
            check(CODEC.replace("t=0.671833", "t=0.9") + REPLY)

    def test_changed_algorithm(self):
        with self.assertRaisesRegex(ValueError, "header"):
            check(CODEC + REPLY.replace("0e0082", "0e0083"))


if __name__ == "__main__":
    unittest.main()

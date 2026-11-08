import unittest
from tools.gsm_normal_signal import decode_p27_normal,decode_control,TSC0
from tools.gsm_xcch_signal import normal_control_bursts


class NormalSignalTest(unittest.TestCase):
    def test_exact_control_blocks_and_serializer_layout(self):
        for payload in (bytes(23),bytes([255])*23,bytes(range(23)),
                        bytes.fromhex('013f4905087000f000fffe330809101010325476982b2b')):
            decoded=[]
            for burst in normal_control_bursts(payload):
                burst=list(burst);burst[61:87]=TSC0
                bits=(1,)*9+tuple(burst)+(1,)*23
                words=[sum(bits[word*12+bit]<<bit for bit in range(12)) for word in range(15)]
                decoded.append(decode_p27_normal(words))
                for at in (9,69,70,96,154):
                    bad=words.copy();bad[at//12]^=1<<(at%12)
                    with self.assertRaises(ValueError):decode_p27_normal(bad)
            self.assertEqual(decode_control(decoded),payload)
            for row in range(4):
                bad=[list(burst) for burst in decoded];bad[row][0]^=1
                with self.assertRaises(ValueError):decode_control(bad)

    def test_missing_blocks_and_invalid_words_reject(self):
        with self.assertRaisesRegex(ValueError,'FIRE'):
            decode_control([[0]*114]*4)
        for bursts in ([],[[0]*114]*3,[[0]*113]*4,[[2]*114]*4):
            with self.assertRaises(ValueError):decode_control(bursts)
        for words in ([],[0]*14,[65536]*15,[0]*15):
            with self.assertRaises(ValueError):decode_p27_normal(words)


if __name__=='__main__':unittest.main()

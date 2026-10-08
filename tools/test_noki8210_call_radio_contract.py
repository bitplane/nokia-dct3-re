import unittest
from tools.noki8210_call_radio_contract import channel_patterns


class CallRadioContractTest(unittest.TestCase):
    def test_dcs_words_are_not_gsm_words(self):
        words = ('041202000271012fc10003370000000400000000',
                 '041202001117001a600003370000001400000001')
        prefix = 'TX packet type=02 payload=20 words=11 data='
        for index, pattern in enumerate(channel_patterns(dcs1800=True)):
            self.assertTrue(pattern.search(prefix + words[index]))
            self.assertFalse(pattern.search(prefix + words[index].replace('0337', '0004')))
            for gsm in (False, True):
                self.assertFalse(channel_patterns(gsm)[index].search(prefix + words[index]))
        with self.assertRaisesRegex(ValueError, 'mutually exclusive'):
            channel_patterns(True, dcs1800=True)

    def test_configured_and_legacy_words_are_not_interchangeable(self):
        old = ('040002000271012fc10000010000000400000000',
               '040000001117001a600000040000001400000001')
        configured = ('041202000271012fc10000040000000400000000',
                      '041202001117001a600000040000001400000001')
        for flag, accepted, rejected in ((False, old, configured), (True, configured, old)):
            for pattern, good, wrong in zip(channel_patterns(flag), accepted, rejected):
                with self.subTest(flag=flag, data=good):
                    prefix = 'TX packet type=02 payload=20 words=11 data='
                    self.assertTrue(pattern.search(prefix + good))
                    self.assertFalse(pattern.search(prefix + wrong))
                    self.assertFalse(pattern.search(prefix.replace('payload=20', 'payload=24') + good))

    def test_configured_traffic_cannot_silently_accept_default_assignment(self):
        traffic, release = channel_patterns(True)
        prefix = 'TX packet type=02 payload=20 words=11 data='
        self.assertFalse(traffic.search(prefix + '041202000271012fc10000010000000400000000'))
        self.assertFalse(release.search(prefix + '041202001117001a600000010000001400000001'))


if __name__ == '__main__':
    unittest.main()

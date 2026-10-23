import unittest

from tools.nse2_lifecycle_check import diagnose, diagnose_transport


def transcript():
    return '\n'.join((
        'nse2_native_identity: n=1 data=0004 mask=ffff pc=0f0f t=0.007 observer_writes=0',
        'nse2_reset_reason: value=68 pc=0028db6e lr=00290361 t=31.0008 observer_writes=0',
        'nse2_reset_entry: reason=00000068 lr=0023616d stack=00000000,000000d5 t=31.9694 observer_writes=0',
        'nse2_native_identity: n=2 data=0004 mask=ffff pc=0f0f t=31.9748 observer_writes=0'))


class LifecycleTest(unittest.TestCase):
    def test_transport_distinguishes_initial_delivery_from_later_silence(self):
        rows = []
        for second in (1, 7, 15, 30):
            packets = 4 if second == 1 else 0
            rows.append(f'nse2_native_transport: t={second}.0 pc=09ac imr=035f ifr=0020 '
                        f'words=0852:0041:0,0853:0041:21,08e4:00a9:4,08e5:00a9:0 mcu_packets={packets:04x} observer_writes=0')
        rows.append('nse2_packet_count: value=0000 mask=ffff pc=0020feae lr=00254e67 t=4.7018 observer_writes=0')
        text = '\n'.join(rows)
        result = diagnose_transport(text)
        self.assertTrue(result['initial_rx_consumed'])
        self.assertFalse(result['broken_initial_fiq_delivery'])
        for old, new in (('08e4:00a9:4', '08e4:00a9:5'), ('mcu_packets=0004', 'mcu_packets=0000'),
                         ('0852:0041', '0852:0042'), ('pc=0020feae', 'pc=0020feac')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                diagnose_transport(text.replace(old, new))

    def test_reset_is_reported_as_failure_not_stable_boot(self):
        result = diagnose(transcript())
        self.assertFalse(result['stable_long_boot'])
        self.assertEqual(result['native_startups'], 2)

    def test_wrong_owner_missing_reboot_and_wrong_order_fail(self):
        for old, new in (('lr=00290361', 'lr=00290363'), ('n=2 data=0004', 'n=2 data=0000'),
                         ('t=31.9748', 't=30.1'), ('000000d5', '000000d4')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                diagnose(transcript().replace(old, new))


if __name__ == '__main__':
    unittest.main()

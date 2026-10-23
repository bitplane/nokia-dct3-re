import unittest

from tools.nse2_lifecycle_check import diagnose, diagnose_transport, diagnose_schedule, diagnose_processing


def transcript():
    return '\n'.join((
        'nse2_native_identity: n=1 data=0004 mask=ffff pc=0f0f t=0.007 observer_writes=0',
        'nse2_reset_reason: value=68 pc=0028db6e lr=00290361 t=31.0008 observer_writes=0',
        'nse2_reset_entry: reason=00000068 lr=0023616d stack=00000000,000000d5 t=31.9694 observer_writes=0',
        'nse2_native_identity: n=2 data=0004 mask=ffff pc=0f0f t=31.9748 observer_writes=0'))


class LifecycleTest(unittest.TestCase):
    def test_two_filter_frames_are_not_sustained_acquisition(self):
        text = '\n'.join(f'rom4_dispatch_count: address={address} count={count}'
                         for address, count in (('32f4', 2), ('09a4', 1627), ('4414', 1629), ('33f3', 21226)))
        text += '\n' + '\n'.join(f'rom4_comparison_count: address={address} count={count}'
                                  for address, count in (('330a', 16), ('332b', 2), ('3347', 0), ('33ac', 1)))
        text += '\nrom4_comparison_input: reads=679232 nonzero=0 reduction_nonzero=0'
        for i, mode in enumerate(('0000', '0001', '0002', '000c', '0004', '000c', '0004')):
            text += f'\nrom4_mode_write: t={4.7 + i * 0.01} pc=00442b address=00ac data={mode} mask=ffff'
        result = diagnose_processing(text)
        self.assertFalse(result['acquisition_proved'])
        self.assertFalse(result['stable_long_boot'])
        for old, new in (('address=32f4 count=2', 'address=32f4 count=2000'),
                         ('nonzero=0', 'nonzero=1'), ('data=0002', 'data=0003'),
                         ('address=33ac count=1', 'address=33ac count=0')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                diagnose_processing(text.replace(old, new))

    def test_active_scheduler_is_not_evidence_of_stable_native_receive(self):
        text = ('nse2_native_work_writer: pc=32c1 writes=1625 bit0_set=1625 bits=0821 t=30.0 observer_writes=0\n'
                'nse2_native_work_writer: pc=446d writes=1628 bit0_set=0 bits=0830 t=30.0 observer_writes=0\n'
                'nse2_native_host_packet: previous=001e next=0041 header=441a words=' +
                ','.join(['0011', '9800'] + ['0000'] * 32) + ' t=4.704 observer_writes=0')
        result = diagnose_schedule(text)
        self.assertTrue(result['native_scheduling_active'])
        self.assertFalse(result['stable_long_boot'])
        for old, new in (('bit0_set=1625', 'bit0_set=0'), ('bit0_set=0', 'bit0_set=1'),
                         ('0011,9800', '0010,9800'), ('t=4.704', 't=6.0')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                diagnose_schedule(text.replace(old, new))

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

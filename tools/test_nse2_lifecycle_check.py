import unittest

from tools.nse2_lifecycle_check import (
    diagnose, diagnose_transport, diagnose_schedule, diagnose_processing,
    diagnose_scan_progress, diagnose_timer_boundary, diagnose_timer_queue,
    timer_queue_offset, diagnose_host_packets,
)


def transcript():
    return '\n'.join((
        'nse2_native_identity: n=1 data=0004 mask=ffff pc=0f0f t=0.007 observer_writes=0',
        'nse2_reset_reason: value=68 pc=0028db6e lr=00290361 t=31.0008 observer_writes=0',
        'nse2_reset_entry: reason=00000068 lr=0023616d stack=00000000,000000d5 t=31.9694 observer_writes=0',
        'nse2_native_identity: n=2 data=0004 mask=ffff pc=0f0f t=31.9748 observer_writes=0'))


class LifecycleTest(unittest.TestCase):
    def test_host_capture_requires_the_full_pointer_chain_and_large_payload(self):
        # A valid 154-byte payload wraps the 82-word ring; the old 128-byte
        # probe bound silently omitted this case.
        text = ('nse2_native_host_packet: previous=0000 next=0006 header=0a05 words=0000,0000,0000,0000,0000 t=0.4 observer_writes=0\n'
                'nse2_native_host_packet: previous=0006 next=0002 header=9a3c words=' +
                ','.join(['ffff'] * 77) + ' t=0.5 observer_writes=0\n'
                'nse2_native_transport: t=30.0 pc=09ac imr=035f ifr=0020 '
                'words=0852:0002:0,0853:0002:2 mcu_packets=0000 observer_writes=0')
        result = diagnose_host_packets(text)
        self.assertEqual(result['largest_payload_bytes'], 154)
        self.assertFalse(result['host_type4b_observed'])
        for old, new in (('next=0002', 'next=0003'), ('0853:0002:2', '0853:0002:3'),
                         ('header=9a3c', 'header=983c'), ('0852:0002', '0852:0003')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                diagnose_host_packets(text.replace(old, new))

    def test_queue_selector_uses_shift_carry_bit_one_not_bit_two(self):
        for global_flags, node_flags, offset in ((3, 3, 0x10), (2, 2, 0x10), (4, 4, 0x28),
                                                (1, 1, 0x28), (2, 4, 0x28), (0xff, 2, 0x10)):
            with self.subTest(global_flags=global_flags, node_flags=node_flags):
                self.assertEqual(timer_queue_offset(global_flags, node_flags), offset)

    def test_timer_queue_uses_active_fiq_bank_not_interrupted_user_lr(self):
        text = transcript() + '\n' + '\n'.join((
            'nse2_timer_queue: kind=insert ticks=0b9b node_flags=03 global_flags=03 head10=001008dc head28=001008e8 mode=1f active_lr=00254e5f user_lr=00254e5f r4=001d r6=100020 active_r8=0b9b t=4.7018 observer_writes=0',
            'nse2_timer_queue_select: node_flags=03 global_flags=03 selected_base=00100020 t=4.7019 observer_writes=0',
            'nse2_timer_queue: kind=expire ticks=0000 node_flags=03 global_flags=03 head10=001003c0 head28=00100198 mode=11 active_lr=00255535 user_lr=00295d79 r4=0b9b r6=0f8e active_r8=0f11 t=31.0001 observer_writes=0'))
        result = diagnose_timer_queue(text)
        self.assertEqual(result['elapsed_beyond_target_ticks'], 886)
        self.assertFalse(result['queue_migration_required'])
        self.assertFalse(result['hardware_timing_fault_proved'])
        for old, new in (('active_lr=00255535', 'active_lr=00295d79'), ('mode=11', 'mode=1f'),
                         ('r6=0f8e', 'r6=0f11'), ('r4=0b9b', 'r4=0b9c'),
                         ('selected_base=00100020', 'selected_base=00100038')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                diagnose_timer_queue(text.replace(old, new))

    def test_timer_terminal_order_is_not_a_clock_fault_proof(self):
        text = transcript() + '\n' + '\n'.join((
            'nse2_receive_timer: ticks=09cd lr=0020fead node_ticks=0000 node_state=01 t=4.7018 observer_writes=0',
            'nse2_fiq5_status: status=22 current=7fff destination=7fff pc=00295644 t=31.0000 observer_writes=0'))
        result = diagnose_timer_boundary(text)
        self.assertAlmostEqual(result['elapsed_start_to_policy'], 26.299)
        self.assertFalse(result['hardware_timing_fault_proved'])
        for old, new in (('status=22', 'status=02'), ('current=7fff', 'current=0000'),
                         ('t=4.7018', 't=0.1'), ('t=31.0000 ', 't=31.01 ')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                diagnose_timer_boundary(text.replace(old, new))

    def test_progressing_measurement_is_not_a_completed_result(self):
        rows = []
        for i, outer in enumerate(range(7, 0, -1)):
            rows.append(f'nse2_native_scan_loop: address=09c4 pc=09c5 ar5=ffff ar6={outer:04x} repeat=0001 t={6.6+i*3.866} observer_writes=0')
            if outer > 1:
                rows.append(f'nse2_native_scan_loop: address=09c4 pc=09c5 ar5=ffff ar6={outer:04x} repeat=0000 t={8.45+i*3.866} observer_writes=0')
                rows.append(f'nse2_native_scan_loop: address=09cb pc=09cc ar5=ffff ar6={outer:04x} repeat=ffff t={8.451+i*3.866} observer_writes=0')
        text = '\n'.join(rows)
        result = diagnose_scan_progress(text)
        self.assertTrue(result['native_measurement_progressing'])
        self.assertFalse(result['scan_completed'])
        for old, new in (('ar6=0006', 'ar6=0007'), ('repeat=ffff', 'repeat=0000'), ('ar5=ffff', 'ar5=0000')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                diagnose_scan_progress(text.replace(old, new))

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

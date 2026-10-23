import struct
import unittest

from tools.nse2_native_result_check import (FRAGMENTS, MCU_REQUEST_FRAGMENTS, NV_POLICY_FRAGMENTS,
                                           inspect_control_trace, inspect_mcu_request, inspect_program,
                                           inspect_request_lifecycle, inspect_scan_pacing,
                                           inspect_completion_counters, compare_scan_budget, inspect_nv_policy,
                                           inspect_publisher_runtime, PUBLISHER_TYPE_LOADS)


class ResultTest(unittest.TestCase):
    def publisher_trace(self):
        active = {0x4a11,0x4a21,0x4b1a,0x4ba8}
        rows = [f'rom4_dispatch_count: address={address:04x} count={4 if address==0x37ce else int(address in active)}'
                for address in (*PUBLISHER_TYPE_LOADS,0x9404,0x37ce)]
        rows.extend(f'rom4_publisher_entry: n={index} type=0074 length={length:04x} '
                    f't={0.4+index*0.1} observer_writes=0'
                    for index,length in enumerate((16,52,2,4),1))
        for second in (1,7,15,30):
            cursor, packets = ('0015',4) if second==1 else ('0041',0)
            rows.append(f'nse2_native_transport: t={second}.0 pc=09ac imr=035f ifr=0020 '
                        f'words=0852:{cursor}:0,0853:{cursor}:21,08e4:00a9:4,08e5:00a9:0 '
                        f'mcu_packets={packets:04x} observer_writes=0')
        rows.append('nse2_packet_count: value=0000 mask=ffff pc=0020feae lr=00254e67 t=4.7 observer_writes=0')
        return '\n'.join(rows)

    def test_publisher_runtime_attributes_initial_control_responses(self):
        result = inspect_publisher_runtime(self.publisher_trace())
        self.assertEqual(result['packet_types'], ['74']*4)
        self.assertIn('4810',result['inactive_direct_sites'])

    def test_publisher_runtime_missing_changed_or_extra_entries_fail(self):
        text = self.publisher_trace()
        for changed in (text.replace('address=314d','address=ffff'),
                        text.replace('address=4810 count=0','address=4810 count=1'),
                        text.replace('type=0074','type=008b'),
                        text.replace('n=4 type=', 'n=5 type='),
                        text.replace('t=30.0', 't=29.0')):
            with self.subTest(changed=changed), self.assertRaises(ValueError):
                inspect_publisher_runtime(changed)

    def nv_image(self):
        image = bytearray(0xc0000)
        for address, encoded in NV_POLICY_FRAGMENTS.items():
            value = bytes.fromhex(encoded)
            image[address - 0x200000:address - 0x200000 + len(value)] = value
        struct.pack_into('>II', image, 0x90670, 0x2b3764, 0x2b3768)
        image[0xb3764:0xb3766] = bytes((1, 255))
        struct.pack_into('>HH', image, 0xb3768, 0x401, 0x607)
        return image

    def test_nv_policy_has_table_driven_ff_initializer(self):
        result = inspect_nv_policy(self.nv_image())
        self.assertEqual(result['records'][1], {'record': '0607', 'value': 'ff'})
        self.assertFalse(result['record_0607_reader_only'])
        self.assertFalse(result['zero_factory_value_proved'])
        self.assertFalse(result['initialization_runtime_observed'])

    def test_nv_policy_pairing_pointer_branch_and_truncation_fail(self):
        for offset in (0xb3765, 0xb376b, 0x90670, 0x35656, 0x9049c, 0x902f4):
            image = self.nv_image()
            image[offset] ^= 1
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                inspect_nv_policy(image)
        with self.assertRaises(ValueError):
            inspect_nv_policy(self.nv_image()[:0xb376b])

    def budget_trace(self):
        rows = [self.pacing_trace(), self.completion_trace(),
                'nse2_native_identity: n=1 data=0004 mask=ffff pc=0f0f t=0.007 observer_writes=0',
                'nse2_reset_reason: value=68 pc=0028db6e lr=00290361 t=31.0008 observer_writes=0',
                'nse2_reset_entry: reason=00000068 lr=0023616d stack=000000d5 t=31.9694 observer_writes=0',
                'nse2_native_identity: n=2 data=0004 mask=ffff pc=0f0f t=31.9748 observer_writes=0',
                'nse2_receive_timer: ticks=09cd lr=0020fead node_ticks=0000 node_state=01 t=4.7018 observer_writes=0',
                'nse2_fiq5_status: status=22 current=7fff destination=7fff pc=00295644 t=31.0000 observer_writes=0']
        for index, outer in enumerate(range(7, 0, -1)):
            for address, repeat, time in [('09c4', 1, 6.6 + index * 3.866)] + (
                    [('09c4', 0, 8.45 + index * 3.866), ('09cb', 0xffff, 8.451 + index * 3.866)]
                    if outer > 1 else []):
                rows.append(f'nse2_native_scan_loop: address={address} pc={int(address, 16)+1:04x} '
                            f'ar5=ffff ar6={outer:04x} repeat={repeat:04x} t={time:.9f} observer_writes=0')
        return '\n'.join(rows)

    def test_budget_is_projection_not_completion_or_cause(self):
        result = compare_scan_budget(self.budget_trace())
        self.assertEqual(result['iterations'], 1984)
        self.assertGreater(result['projected_end_without_outer_gaps'], result['observed_policy_time'])
        self.assertFalse(result['root_cause_proved'])
        self.assertFalse(result['scan_completion_observed'])

    def test_budget_requires_same_trace_boundary_and_progress(self):
        text = self.budget_trace()
        for changed in (text.replace('nse2_reset_reason:', 'missing:'),
                        text.replace('ar6=0006', 'ar6=0007'),
                        text.replace('address=0990 n=1 ', 'address=0990 n=9 ')):
            with self.subTest(changed=changed), self.assertRaises(ValueError):
                compare_scan_budget(changed)

    def completion_trace(self):
        period = 1351 * 12 / 13_000_000
        frames = [(12, 1, 1), (12, 0, 1), (12, 2, 0), (12, 1, 0), (12, 0, 1),
                  (4, 2, 0), (4, 1, 0), (4, 0, 4), (4, 0, 3), (4, 0, 2), (4, 0, 1)]
        rows, counts = [], {}
        def row(address, mode, skip, accumulation, time):
            counts[address] = counts.get(address, 0) + 1
            rows.append(f'nse2_native_scan_completion: address={address:04x} n={counts[address]} '
                        f'mode={mode:04x} skip={skip:04x} accumulation={accumulation:04x} '
                        f'ar2=0060 t={time:.9f} observer_writes=0')
        for index, (mode, skip, accumulation) in enumerate(frames):
            time = 4.744 + index * period
            row(0x3204, mode, skip, accumulation, time)
            row(0x32c5 if skip else 0x326d, mode, skip, accumulation, time + 0.000001)
            if index == 4:
                row(0x32b3, mode, 2, 0, time + 0.000002)
        row(0x32be, 4, 0, 0, time + 0.000003)
        return '\n'.join(rows)

    def test_completion_counter_progress_is_not_physical_validation(self):
        result = inspect_completion_counters(self.completion_trace())
        self.assertEqual(result['frame_interrupts'], 11)
        self.assertFalse(result['missing_frame_in_observed_wait'])
        self.assertFalse(result['physical_timing_validated'])

    def test_missing_changed_and_duplicated_completion_frames_fail(self):
        text = self.completion_trace()
        for changed in ('', '\n'.join(text.splitlines()[1:]),
                        text + '\n' + text.splitlines()[0],
                        text.replace('address=32be', 'address=ffff'),
                        text.replace('accumulation=0004', 'accumulation=0005'),
                        text.replace('t=4.744000000', 't=4.740000000')):
            with self.subTest(changed=changed), self.assertRaises(ValueError):
                inspect_completion_counters(changed)

    def pacing_trace(self):
        period = 1351 * 12 / 13_000_000
        rows = []
        for index in range(1, 9):
            start = 4.74 + (index - 1) * 12 * period
            for address, delta, mode, work in ((0x990, 0, 4, 0x821),
                                              (0x99d, period - 0.000001, 4, 0x821),
                                              (0x9a6, period, 12, 0x830),
                                              (0x9b1, 12 * period - 0.000002, 4, 0x821),
                                              (0x9c2, 12 * period - 0.000001, 4, 0x821)):
                rows.append(f'nse2_native_scan_stage: address={address:04x} n={index} '
                            f'pc={address + 1:04x} mode={mode:04x} work={work:04x} '
                            f't={start + delta:.9f} observer_writes=0')
        return '\n'.join(rows)

    def test_scan_pacing_is_not_hardware_validation(self):
        result = inspect_scan_pacing(self.pacing_trace())
        self.assertEqual(result['modeled_periods_per_measurement'], 12)
        self.assertFalse(result['hardware_clock_fault_proved'])

    def test_scan_pacing_incomplete_changed_and_duplicate_fail(self):
        text = self.pacing_trace()
        for changed in ('', '\n'.join(text.splitlines()[1:]),
                        text + '\n' + text.splitlines()[0],
                        text.replace('mode=000c', 'mode=0004'),
                        text.replace('work=0821', 'work=0820'),
                        text.replace('pc=0991', 'pc=0999')):
            with self.subTest(changed=changed), self.assertRaises(ValueError):
                inspect_scan_pacing(changed)

    def request_lifecycle(self):
        return '\n'.join([
            'nse2_request_mode: value=00 pc=0020010c t=0.002 observer_writes=0',
            'nse2_request_mode: value=01 pc=0020cafc t=1.12 observer_writes=0',
            'nse2_request_mode: value=02 pc=0020feb6 t=4.70 observer_writes=0',
            'nse2_request_summary: mode=02 mode_writes=3 constructor_entries=0 t=30.0 observer_writes=0'])

    def test_request_lifecycle_mode_two_is_not_request_four(self):
        result = inspect_request_lifecycle(self.request_lifecycle())
        self.assertFalse(result['required_mode_observed'])

    def test_request_lifecycle_incomplete_or_changed_fails(self):
        text = self.request_lifecycle()
        for changed in ('', text.replace('mode_writes=3', 'mode_writes=4'),
                        text.replace('value=01', 'value=04'),
                        text.replace('constructor_entries=0', 'constructor_entries=1'),
                        text.replace('t=30.0', 't=29.0'),
                        text + '\nnse2_request_constructor: mode=02'):
            with self.subTest(changed=changed), self.assertRaises(ValueError):
                inspect_request_lifecycle(changed)

    def mcu_image(self):
        image = bytearray(0x80000)
        for address, encoded in MCU_REQUEST_FRAGMENTS.items():
            value = bytes.fromhex(encoded)
            image[address - 0x200000:address - 0x200000 + len(value)] = value
        struct.pack_into('>I', image, 0x16388, 0x1094b7)
        for address in (0x216250, 0x21764a):
            displacement = 0x2160a8 - address - 4
            struct.pack_into('>HH', image, address - 0x200000,
                             0xf000 | ((displacement >> 12) & 0x7ff),
                             0xf800 | ((displacement >> 1) & 0x7ff))
        return image

    def test_mcu_constructor_is_not_boot_reachability(self):
        result = inspect_mcu_request(self.mcu_image())
        self.assertEqual(result['payload'], '10')
        self.assertFalse(result['normal_boot_reachability_proved'])

    def test_mcu_changed_gate_literal_and_callers_fail(self):
        for offset in (0x160f4, 0x16168, 0x16388, 0x16250, 0x77fb8):
            image = self.mcu_image()
            image[offset] ^= 1
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                inspect_mcu_request(image)

    def control_trace(self, values=('0000', '0000', '0001', '0000'), count=4):
        rows = [f'nse2_native_ring: address=06e3 data={value} mask=ffff pc=30cb '
                f't={index + 0.25} observer_writes=0' for index, value in enumerate(values)]
        rows.append(f'nse2_native_transport: t=30.000000000 pc=09ac words=06e3:0000:{count} '
                    'mcu_packets=0000 observer_writes=0')
        return '\n'.join(rows)

    def test_complete_control_write_coverage(self):
        result = inspect_control_trace(self.control_trace())
        self.assertEqual(result['writes'], 4)
        self.assertFalse(result['request_bit_observed'])

    def test_control_coverage_fails_closed(self):
        for text in (self.control_trace(count=5), self.control_trace(values=('0020', '0000')),
                     self.control_trace().replace('t=30.', 't=29.'),
                     self.control_trace().replace('06e3:0000:4', '06e3:0001:4'), ''):
            with self.subTest(text=text), self.assertRaises(ValueError):
                inspect_control_trace(text)

    def images(self):
        program, data = bytearray(0x20000), bytearray(0x20000)
        for address, encoded in FRAGMENTS.items():
            value = bytes.fromhex(encoded)
            program[address * 2:address * 2 + len(value)] = value
        struct.pack_into('>H', data, (0xb00f + 0x4b) * 2, 0x3c92)
        struct.pack_into('>H', data, (0xb00f + 0x07) * 2, 0x3c04)
        return program, data

    def test_static_paths_do_not_claim_runtime_or_heartbeat(self):
        result = inspect_program(*self.images())
        self.assertEqual(result['type8b']['payload_bytes'], 162)
        self.assertEqual(result['host_query']['type'], '4b')
        self.assertFalse(result['runtime_result_observed'])
        self.assertFalse(result['unconditional_heartbeat_proved'])
        self.assertFalse(result['indirect_producers_closed'])
        self.assertEqual(result['conditional_processing']['setter'], '67a3')
        self.assertEqual(result['conditional_processing']['control_word'], '06e3 mask 0020')
        self.assertEqual(result['conditional_processing']['host_request_type'], '07')
        self.assertFalse(result['conditional_processing']['setter_runtime_observed'])

    def test_changed_caller_gate_table_and_truncation_fail(self):
        for address in (0xa17, 0x4812, 0x6d1b, 0x6792, 0x6796, 0x67a3, 0x3c04, 0x3c17):
            program, data = self.images()
            program[address * 2] ^= 1
            with self.subTest(address=address), self.assertRaises(ValueError):
                inspect_program(program, data)
        program, data = self.images()
        struct.pack_into('>H', data, (0xb00f + 0x4a) * 2, 0x3c92)
        with self.assertRaises(ValueError):
            inspect_program(program, data)
        with self.assertRaises(ValueError):
            inspect_program(program[:-2], data)

    def test_changed_request_dispatch_fails(self):
        program, data = self.images()
        struct.pack_into('>H', data, (0xb00f + 7) * 2, 0x3d44)
        with self.assertRaises(ValueError):
            inspect_program(program, data)


if __name__ == '__main__':
    unittest.main()

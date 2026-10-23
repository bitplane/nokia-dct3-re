import struct
import unittest

from tools.nse2_native_result_check import (FRAGMENTS, MCU_REQUEST_FRAGMENTS,
                                           inspect_control_trace, inspect_mcu_request, inspect_program,
                                           inspect_request_lifecycle, inspect_scan_pacing,
                                           inspect_completion_counters)


class ResultTest(unittest.TestCase):
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

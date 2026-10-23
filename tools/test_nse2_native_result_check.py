import struct
import unittest

from tools.nse2_native_result_check import (FRAGMENTS, MCU_REQUEST_FRAGMENTS,
                                           inspect_control_trace, inspect_mcu_request, inspect_program)


class ResultTest(unittest.TestCase):
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

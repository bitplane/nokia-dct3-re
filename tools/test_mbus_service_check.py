import unittest
from tools.mbus_protocol import Frame
from tools.mbus_service_check import verify


class MbusServiceCheckTest(unittest.TestCase):
    def capture(self):
        frames = [Frame(0x10, 0, 0x7f, 1), Frame(0x10, 0, 0xd2, 2, b'version')]
        lines = []
        for frame in frames:
            lines.extend(f'mbus_service_wire: data={byte:02x}' for byte in frame.encode())
            lines.append(f'mbus_service_frame: hex={frame.encode().hex()}')
        for index, byte in enumerate(Frame(0, 0x10, 0x7f, 2).encode()):
            lines.append(f'mbus_service_ack: index={index + 1} data={byte:02x} t=20.1')
            lines.append(f'mbus_fixture: data={byte:02x} accepted={int(index < 5)}')
            if index < 5:
                lines.append(f'mbus_device: event=rx_read data={byte:02x}')
        lines.append('mbus_service_ack: complete')
        return '\n'.join(lines)

    def test_sequence_accepted_checksum_not_consumed(self):
        self.assertEqual(verify(self.capture()), b'version')

    def test_lost_sequence_or_duplicate_response_fails(self):
        text = self.capture()
        for bad in (text.replace('data=02 accepted=1', 'data=02 accepted=0'),
                    text + '\nmbus_service_frame: hex=' + Frame(0x10, 0, 0xd2, 2).encode().hex()):
            with self.assertRaises(ValueError):
                verify(bad)

    def test_wire_loss_collision_and_client_failure_fail(self):
        text = self.capture()
        for bad in (text.replace('mbus_service_wire: data=1f', 'missing', 1),
                    text + '\nmbus_device: event=tx_collision',
                    text + '\n[LUA ERROR]',
                    text.replace('mbus_service_ack: complete', '')):
            with self.assertRaises(ValueError):
                verify(bad)

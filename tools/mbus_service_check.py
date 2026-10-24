"""Validate the version exchange and NPE-3's five-byte ACK receive contract."""
import argparse
from pathlib import Path
import re

try:
    from .mbus_protocol import Frame, decode
except ImportError:
    from mbus_protocol import Frame, decode


def verify(text):
    if '[LUA ERROR]' in text:
        raise ValueError('Lua client failed')
    frames = [decode(bytes.fromhex(value)) for value in
              re.findall(r'mbus_service_frame: hex=([0-9a-f]+)', text)]
    if len(frames) != 2 or frames[0] != Frame(0x10, 0, 0x7f, 1):
        raise ValueError('expected one request ACK and one version reply')
    reply = frames[1]
    if (reply.destination, reply.source, reply.kind) != (0x10, 0, 0xd2):
        raise ValueError('unexpected response endpoints/type')
    wire = bytes(int(value, 16) for value in
                 re.findall(r'mbus_service_wire: data=([0-9a-f]{2})', text))
    if wire != b''.join(frame.encode() for frame in frames):
        raise ValueError('wire bytes differ from complete frames')
    ack = Frame(reply.source, reply.destination, 0x7f, reply.sequence).encode()
    rows = list(re.finditer(r'mbus_service_ack: index=(\d+) data=([0-9a-f]{2}) t=([0-9.]+)', text))
    if len(rows) != len(ack) or text.count('mbus_service_ack: complete') != 1:
        raise ValueError('host ACK did not complete')
    for index, (row, byte) in enumerate(zip(rows, ack)):
        if (int(row[1]), int(row[2], 16)) != (index + 1, byte):
            raise ValueError('host ACK does not match received sequence')
        end = rows[index + 1].start() if index + 1 < len(rows) else text.index('mbus_service_ack: complete')
        interval = text[row.end():end]
        accepted = re.findall(r'mbus_fixture: data=([0-9a-f]{2}) accepted=(\d+)', interval)
        reads = re.findall(r'mbus_device: event=rx_read data=([0-9a-f]{2})\b', interval)
        # Own NPE-3 MCU 0x4c861e..0x4c864a closes RX on ACK sequence,
        # before the external six-byte frame's checksum. Do not hide the drop.
        expected_accept = '0' if index == len(ack) - 1 else '1'
        expected_reads = [] if expected_accept == '0' else [f'{byte:02x}']
        if accepted != [(f'{byte:02x}', expected_accept)] or reads != expected_reads:
            raise ValueError(f'host ACK byte {index + 1} rejected or not read')
    if 'event=tx_collision' in text:
        raise ValueError('wire collision observed')
    return reply.payload


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    try:
        print('MBUS service PASS:', verify(args.log.read_text()).hex())
    except (OSError, ValueError) as error:
        parser.exit(1, f'MBUS service FAIL: {error}\n')


if __name__ == '__main__':
    main()

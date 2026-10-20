"""Check own NSK-1 captured record exchange against shared native ROM4 arithmetic."""
import argparse
import json
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.dct3_msid_codec import decode_msid
from tools.dct3_rom4_record_codec import decode_record,retained_record_transform


REQUEST = re.compile(r'nsk1_record_request: command=([0-9a-f]{2}) bytes=([0-9a-f]+) caller=[0-9a-f]{8} t=[0-9.]+ observer_writes=0')
REPLY = re.compile(r'nsk1_record_reply: consumer=[0-9a-f]{8} envelope=([0-9a-f]{16}) payload=([0-9a-f]+) t=[0-9.]+ observer_writes=0')


def check(text, *, minimum=2, initial_eeprom=None):
    if minimum < 1:
        raise ValueError('minimum exchange count must be positive')
    if initial_eeprom is not None and len(initial_eeprom) != 2048:
        raise ValueError('initial own EEPROM must contain 2048 bytes')
    pending, identity, records = None, None, []
    for line in text.splitlines():
        request,reply = REQUEST.search(line),REPLY.search(line)
        if 'nsk1_record_request:' in line and not request or 'nsk1_record_reply:' in line and not reply:
            raise ValueError('unrecognized own record trace format')
        if request:
            packet = bytes.fromhex(request[2])
            if len(packet) < 6 or packet[3] != 0x70 or packet[2] != len(packet)-4:
                raise ValueError('invalid own request envelope')
            command,size = packet[4:6]
            if command != int(request[1],16) or size != len(packet)-6 or size != {0x13:4,0x14:12,0x15:20,0x16:24}.get(command):
                raise ValueError('invalid own request command/size')
            if command == 0x13:
                if pending is not None:
                    raise ValueError('overlapping record sessions')
                pending,identity = {},None
            if pending is None or command in pending or command != 0x13+len(pending):
                raise ValueError('record requests are missing, repeated or reordered')
            pending[command] = packet[6:]
        elif reply:
            envelope,payload = bytes.fromhex(reply[1]),bytes.fromhex(reply[2])
            if (envelope[:4] != bytes.fromhex('00000074') or envelope[6:] != bytes.fromhex('0100')
                    or int.from_bytes(envelope[4:6],'big') != len(payload)+2):
                raise ValueError('invalid own internal reply envelope')
            if pending is None or len(pending) != 4:
                raise ValueError('reply lacks complete own requests')
            if len(payload) == 16 and payload[:4] == bytes.fromhex('340e0082'):
                if identity is not None:
                    raise ValueError('duplicate identity reply')
                identity = decode_msid(payload[3:])
                if identity[:4] != pending[0x13]:
                    raise ValueError('MSID does not reproduce own requested flash value')
            elif len(payload) == 52 and payload[:4] == bytes.fromhex('35320000'):
                if identity is None:
                    raise ValueError('record reply precedes identity')
                encoded,chip = pending[0x16],identity[4:8]
                blocks = [decode_record(encoded[start:start+12],chip) for start in (0,12)]
                decoded = b''.join(block[:10]+bytes(2) for block in blocks)
                if payload[4:28] != decoded or payload[28:] != encoded:
                    raise ValueError('native record transform or encoded echo differs')
                source = 'not checked'
                if initial_eeprom is not None:
                    if pending[0x14] != initial_eeprom[0x14:0x20] or pending[0x15] != initial_eeprom[:0x14]:
                        raise ValueError('own low-record request differs from initial storage')
                    raw = initial_eeprom[0x20:0x38]
                    retained = retained_record_transform(raw,initial_eeprom[:12])
                    if encoded not in (raw,retained):
                        raise ValueError('record source is neither own raw nor retained transform')
                    source = 'raw' if encoded == raw else 'retained transform'
                records.append({'msid_decoded':identity.hex(), 'chip_field':chip.hex(),
                                'request':encoded.hex(), 'decoded':decoded.hex(),
                                'private_markers':[block[10:].hex() for block in blocks],
                                'storage_source':source})
                pending,identity = None,None
            else:
                raise ValueError('unknown own reply format/family')
    if pending is not None or len(records) < minimum:
        raise ValueError('missing or incomplete own record exchanges')
    return {'exchanges':records, 'scope':'native arithmetic and own input mapping; not fitted-mask, provisioning or lock-policy validity'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log',type=Path)
    parser.add_argument('--initial-erased',action='store_true',help='check the explicitly erased 2-KiB starting input, not a factory profile')
    args = parser.parse_args()
    try:
        report = check(args.log.read_text(),initial_eeprom=bytes([255])*2048 if args.initial_erased else None)
    except (OSError,ValueError) as error:
        parser.exit(1,f'NSK-1 record comparison failed: {error}\n')
    print(json.dumps(report,indent=2))


if __name__ == '__main__':
    main()

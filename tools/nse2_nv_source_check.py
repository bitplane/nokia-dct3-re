"""Bound NSE-2 empty-field record writes before EEPROM serialization."""
import re

CODE = {
    0x28a1d0: 'b5f01c0e2200703278022a00d031',
    0x28a242: 'bdf0',
    0x28a17c: 'b53078032b20db012320e0012b00d01b',
    0x28a1c4: '20007008bd30',
    0x24599a: '1c10a901f044fc17',
    0x2459d0: 'a805a901c90cc00cc90cc00c',
    0x245ba4: 'a90146402210f055f8ad46401c29f044fae3',
    0x293c8e: 'b5f04643464cb4181c161c041c0d2005',
}
WRITE = re.compile(r'nse2_nv_source: address=([0-9a-f]{4}) pointer=([0-9a-f]{8}) '
    r'length=(\d+) caller=([0-9a-f]{8}) data=([0-9a-f]*) t=([0-9.]+) observer_writes=0')
ENCODE = re.compile(r'nse2_nv_encode: source=([0-9a-f]{8}) first=([0-9a-f]{2}) '
    r'output=([0-9a-f]{8}) caller=([0-9a-f]{8}) t=([0-9.]+) observer_writes=0')


def check(text, image, bank):
    for address, encoded in CODE.items():
        expected = bytes.fromhex(encoded)
        if image[address-0x200000:address-0x200000+len(expected)] != expected:
            raise ValueError('own empty encoder/decoder/copy/writer code differs')
    if len(bank) != 8192:
        raise ValueError('complete own EEPROM required')
    writes, encodes = [], []
    for line in text.splitlines():
        if 'nse2_nv_source:' in line:
            match = WRITE.search(line)
            if not match:
                raise ValueError('malformed source observation')
            address, pointer, length, caller, data, time = match.groups()
            address, length = int(address, 16), int(length)
            if 0x6d0 <= address < 0x922:
                if (pointer, length, caller) != ('0010d618', 33, '00290e17'):
                    raise ValueError('record write ownership/extent differs')
                raw = bytes.fromhex(data)
                if len(raw) != 33 or raw != bank[address:address+33] or raw[0] != 0:
                    raise ValueError('source is not the saved empty-field record')
                writes.append((address, float(time)))
        elif 'nse2_nv_encode:' in line:
            match = ENCODE.search(line)
            if not match:
                raise ValueError('malformed encoder observation')
            source, first, output, caller, time = match.groups()
            if caller == '002459a3':
                if (source, first, output) != ('0010d68c', '00', '0010d608'):
                    raise ValueError('record input is not the observed empty string')
                encodes.append(float(time))
    if [address for address, _ in writes] != list(range(0x6d0, 0x922, 33)) or len(encodes) != 18:
        raise ValueError('complete eighteen-record sequence required')
    for index, (start, (_, end)) in enumerate(zip(encodes, writes)):
        if not start < end or (index and start <= writes[index-1][1]):
            raise ValueError('encoder/write order differs')
    return dict(records=18, empty_lengths=True, source_equals_saved=True,
                unused_payload_offsets=list(range(1, 16)), record_validity_proved=False)

"""Own NSE-5 v5.00 control producer and native pending-port boundary."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_BIG_ENDIAN

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nse5_fullbackup_check import IMAGES, check_run
from tools.c54x_rom4_codec_contract import check_live_serial_helpers

FRAGMENTS = {
    0x3af24e: 'b5302040f00cff051c0520001c292240f13cfb691c28213cf0e2f81124006be94288d10024011c28f00cfdd71c20bd30',
    0x3d6e9c: 'b5701c0e1c0548e5700670452d00d10df7ffffb848ba78002800d121f774fa4f200021002200f00cfe3abd70',
    0x3d72ac: '2009210422012301f7fffe2320002100f7fffdee',
    0x3e35a8: 'b5701c0c1c062008f7d8fd561c012002800820047088204c70c82500710d714e718c200a71c82003f7d8f856',
    0x4d7f12: 'b5f01c1f1c141c0d1c062008f6e4f89f1c012002800820047088204c70c8710e714d718c71cf2003f6e3fba1bdf0',
    0x3bb680: 'b5f04642464b4654',
}
PPM_PREFIX = b'PPM\x00V 05.00\n09-11-00\nNSE-5\n'


def inspect(image):
    if len(image) != 0x400000 or image[0x310000:0x310000+len(PPM_PREFIX)] != PPM_PREFIX:
        raise ValueError('complete own v5.00 image required')
    for address, encoded in FRAGMENTS.items():
        expected = bytes.fromhex(encoded)
        if image[address-0x200000:address-0x200000+len(expected)] != expected:
            raise ValueError(f'own control/checksum code differs at {address:06x}')
    decoder = Cs(CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_BIG_ENDIAN)
    calls = {address: [] for address in (0x3e35a8,0x3e3b3a,0x3d6e9c,0x4d7f12)}
    # Aligned direct-BL candidates in the MCU extent only. Literal/code
    # ambiguity and indirect/computed callers remain outside this census.
    for offset in range(0, 0x310000-4, 2):
        if int.from_bytes(image[offset:offset+2], 'big') & 0xf800 != 0xf000:
            continue
        instructions = list(decoder.disasm(image[offset:offset+4],0x200000+offset))
        if len(instructions) != 1 or instructions[0].mnemonic != 'bl':
            continue
        target = int(instructions[0].op_str.removeprefix('#'), 0)
        if target in calls:
            calls[target].append(instructions[0].address)
    return {f'{address:06x}': [f'{site:06x}' for site in sites] for address,sites in calls.items()}


def check_trace(text):
    if re.search(r'illegal|unsupported|unhandled|fatal|LUA ERROR', text, re.I):
        raise ValueError('native execution or observer error')
    checksum = re.findall(r'nse5_v500_low_checksum: computed=([0-9a-f]{4}) stored=([0-9a-f]{8}) t=([0-9.]+) observer_writes=0', text)
    if (len(checksum) != 2 or [(a,b) for a,b,_ in checksum] != [('1988','00001988')]*2
            or text.count('nse5_v500_low_checksum:') != 2):
        raise ValueError('live own low-record checksum differs')
    entries = re.findall(r'nse5_v500_entry: site=([0-9a-f]{8}) index=(\d+) r0=([0-9a-f]{8}) r1=([0-9a-f]{8}) r2=([0-9a-f]{8}) r3=([0-9a-f]{8}) caller=([0-9a-f]{8}) t=([0-9.]+) observer_writes=0', text)
    if len(entries) != text.count('nse5_v500_entry:'):
        raise ValueError('own entry observation malformed')
    selected = []
    for site, args, caller in (
            ('003d6efe', ('00000009','00000004','00000001','00000001'), '003d72b9'),
            ('003d6e9c', ('00000000','00000000'), '003d72c1'),
            ('003e3b3a', ('00000000','00000000','00000000'), '003d6ec7'),
            ('003e35a8', ('00000000','00000001'), '003e3661')):
        found = [row for row in entries if row[0] == site]
        if len(found) != 1 or found[0][1] != '1' or tuple(found[0][2:2+len(args)]) != args or found[0][6] != caller:
            raise ValueError('own initializer/delegation contract differs')
        selected.append(float(found[0][7]))
    posts = re.findall(r'nse5_v500_control_post: bytes=([0-9a-f]{16}) caller=([0-9a-f]{8}) t=([0-9.]+) observer_writes=0', text)
    if len(posts) != 1 or posts[0][:2] != ('0002044c0000010a','003e35d5') or text.count('nse5_v500_control_post:') != 1:
        raise ValueError('own type4c payload or producer differs')
    states = re.findall(r'nse5_v500_serial_state: flags=([0-9a-f]{4}) pending=([0-9a-f]{4}) timing=([0-9a-f]{4}),([0-9a-f]{4}) rt0=([0-9a-f]{4}) rt1=([0-9a-f]{4}) t=([0-9.]+) observer_writes=0', text)
    if (len(states) != 1 or states[0][:6] != ('0000','0800','0876','0015','9258','3bcf')
            or text.count('nse5_v500_serial_state:') != 1):
        raise ValueError('native parameter or return-chain observation differs')
    times = [float(checksum[-1][2]), *selected, float(posts[0][2]), float(states[0][-1])]
    if times != sorted(times) or not 2 < times[-1] < 3:
        raise ValueError('own checksum/producer/consumer ordering differs')
    check_live_serial_helpers(text)
    port36 = re.findall(r'rom4_port36: direction=(read|write) value=([0-9a-f]{4}) pc=([0-9a-f]{4}) t=([0-9.]+)', text)
    writes = [(value,pc) for direction,value,pc,_ in port36 if direction == 'write']
    if writes != [('0080','8d98'),('0080','90bc')]*2 + [('809c','9132')]:
        raise ValueError('native port36 write sequence differs')
    polls = [(value,pc) for direction,value,pc,_ in port36 if direction == 'read' and pc == '9140']
    if polls != [('809c','9140')]*4:
        raise ValueError('pending restart readback differs')
    port0 = re.findall(r'rom4_serial_port: port=00 direction=(read|write) value=([0-9a-f]{4}) pc=(924c|9251|9135|913a) t=([0-9.]+)', text)
    if [(direction,value,pc) for direction,value,pc,_ in port0] != [
            ('read','001d','924c'), ('write','000d','9251'),
            ('read','000d','9135'), ('write','001d','913a')]:
        raise ValueError('native port0 clear/restore sequence differs')
    count_rows = re.findall(r'rom4_port36_count: site=([0-9a-f]{4}) count=(\d+)', text)
    counters = dict(count_rows)
    if (len(counters) != 8 or len(count_rows) != 8 or text.count('rom4_port36_count:') != 8
            or any(int(counters.get(site,'0')) <= 0 for site in ('3bcd','9232','9129','9140'))
            or counters.get('9144') != '0'):
        raise ValueError('missing native call-edge controls or unexpected completion')
    return dict(control_payload='0000,010a', low_record_checksum_matches=True,
                native_pending='0800', native_timing=['0876','0015'],
                polled_value='809c', completion_mask='4000', completion_observed=False,
                physical_port_identity_proved=False, baud_and_latency_proved=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('run', type=Path)
    parser.add_argument('--roms', type=Path, default=Path(__file__).resolve().parents[1] / 'roms')
    args = parser.parse_args()
    try:
        check_run(args.directory, args.roms, args.run, '500-full', serial_window=True)
        image = (args.directory / IMAGES['500-full'][0]).read_bytes()
        if hashlib.sha256(image).hexdigest() != IMAGES['500-full'][1]:
            raise ValueError('own whole backup differs')
        calls = inspect(image)
        result = check_trace((args.run / 'error.log').read_text())
    except (OSError, ValueError, KeyError) as error:
        parser.exit(1, f'NSE-5 v5.00 serial boundary FAIL: {error}\n')
    print(json.dumps(dict(boundary=result, direct_bl_candidates=calls), indent=2))


if __name__ == '__main__':
    main()

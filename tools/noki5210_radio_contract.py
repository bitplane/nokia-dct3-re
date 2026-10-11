"""Own NSM-5 background-measurement receive contract, not DSP silicon proof."""
import argparse
import hashlib
import json
from pathlib import Path
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_BIG_ENDIAN


def direct_bl_candidates(image, target, base=0x200000):
    """All aligned old-Thumb BL encodings, including possible data matches."""
    sites = []
    for offset in range(0, len(image) - 3, 2):
        first = int.from_bytes(image[offset:offset + 2], 'big')
        second = int.from_bytes(image[offset + 2:offset + 4], 'big')
        if first & 0xf800 != 0xf000 or second & 0xf800 != 0xf800:
            continue
        displacement = ((first & 0x7ff) << 12) | ((second & 0x7ff) << 1)
        if displacement & (1 << 22):
            displacement -= 1 << 23
        if base + offset + 4 + displacement == target:
            sites.append(base + offset)
    return sites


def verify(image):
    if hashlib.sha1(image).hexdigest() != 'f6c11cb013468c1d5e8550a2903f58ffa467a001':
        raise ValueError('requires acquired NSM-5 v5.40 PPM E')
    def read(address, size):
        return image[address - 0x200000:address - 0x200000 + size]
    decoder = Cs(CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_BIG_ENDIAN)
    def instructions(address, size):
        return [(ins.mnemonic, ins.op_str) for ins in decoder.disasm(read(address, size), address)]
    if instructions(0x3de77a, 14) != [
            ('movs', 'r0, #0x80'), ('subs', 'r0, r1, r0'),
            ('cmp', 'r0, #0'), ('beq', '#0x3de834'),
            ('subs', 'r0, #3'), ('cmp', 'r0, #0xc'), ('bls', '#0x3de796')]:
        raise ValueError('own RX 83..8f table-index namespace differs')
    table = [int.from_bytes(read(0x3de7a0 + index * 4, 4), 'big') for index in range(13)]
    if table[8] != 0x3de7f4 or instructions(0x3de7f4, 8) != [
            ('adds', 'r0, r4, #0'), ('bl', '#0x37019c'), ('b', '#0x3de89c')]:
        raise ValueError('own RX 8b dispatch differs')
    if instructions(0x3701b0, 8) != [
            ('movs', 'r0, #0xc'), ('bl', '#0x2ed2e8'), ('pop', '{pc}')]:
        raise ValueError('own task-12 forwarding differs')
    if instructions(0x2e337a, 6) != [
            ('ldrb', 'r0, [r0]'), ('bl', '#0x2bd782')]:
        raise ValueError('own measurement completion consumer differs')
    if instructions(0x2bd7b8, 6) != [
            ('movs', 'r1, #0x27'), ('mvns', 'r6, r1'), ('str', 'r0, [sp]')]:
        raise ValueError('own forty-record bound differs')
    if instructions(0x2bd7c4, 4) != [('adds', 'r6, #1'), ('adds', 'r5, #4')]:
        raise ValueError('own four-byte measurement stride differs')
    if instructions(0x2bd7de, 18) != [
            ('ldrb', 'r0, [r5, #7]'), ('ldrb', 'r1, [r5, #6]'),
            ('lsls', 'r1, r1, #8'), ('adds', 'r0, r0, r1'),
            ('lsls', 'r0, r0, #0x10'), ('lsrs', 'r7, r0, #0x10'),
            ('ldrb', 'r1, [r5, #9]'), ('lsls', 'r0, r1, #0x18'),
            ('asrs', 'r0, r0, #0x18')]:
        raise ValueError('own big-endian carrier/signed-RSSI lanes differ')
    if int.from_bytes(read(0x2e2168, 4), 'big') != 0x13e78c:
        raise ValueError('own request-root literal differs')
    if instructions(0x2e1e20, 6) != [
            ('str', 'r0, [r1, #8]'), ('movs', 'r0, #0'), ('str', 'r0, [r1, #0xc]')]:
        raise ValueError('own queued-to-current promotion differs')
    if instructions(0x2e1e2c, 6) != [
            ('ldrb', 'r1, [r0, #0xc]'), ('cmp', 'r1, #1'), ('bne', '#0x2e1e46')]:
        raise ValueError('own request flag offset differs')
    selector_table = [int.from_bytes(read(0x2e23c8 + index * 4, 4), 'big') for index in range(11)]
    if selector_table[5:7] != [0x2e24c2, 0x2e24b0]:
        raise ValueError('own readiness selector table differs')
    if instructions(0x2e24b0, 18) != [
            ('cmp', 'r6, #0'), ('bne', '#0x2e24be'),
            ('adr', 'r0, #0x298'), ('bl', '#0x3558e0'),
            ('movs', 'r7, #2'), ('b', '#0x2e24ce'),
            ('movs', 'r0, #6'), ('b', '#0x2e2946')]:
        raise ValueError('own state-6 argument-zero continuation differs')
    if int.from_bytes(read(0x2e2830, 4), 'big') != 0x13c52e:
        raise ValueError('own promotion eligibility flag differs')
    if instructions(0x2e24ce, 20) != [
            ('ldr', 'r0, [pc, #0x360]'), ('ldrb', 'r0, [r0]'),
            ('cmp', 'r0, #0'), ('bne', '#0x2e24d8'), ('b', '#0x2e29f4'),
            ('adr', 'r0, #0x35c'), ('bl', '#0x3558e0'), ('bl', '#0x2e1df4')]:
        raise ValueError('own eligibility-gated promotion path differs')
    direct_sites = [(0x232064, 3), (0x2322f2, 2), (0x234f3e, 0),
                    (0x2355d6, 0), (0x2358a0, 0), (0x2e2f1a, 1),
                    (0x2e2f54, 1), (0x2e3362, 1)]
    if direct_bl_candidates(image, 0x2e23ac) != [site for site, _ in direct_sites]:
        raise ValueError('own aligned direct selector call census differs')
    for site, argument in direct_sites:
        if instructions(site - 2, 6) != [('movs', f'r0, #{argument}'), ('bl', '#0x2e23ac')]:
            raise ValueError('own direct selector argument differs')
    if int.from_bytes(read(0x235004, 4), 'big') != 0x1802:
        raise ValueError('own receive-context message identifier differs')
    if instructions(0x234efe, 24) != [
            ('movs', 'r0, #4'), ('ldrsh', 'r1, [r0, r4]'),
            ('ldr', 'r0, [pc, #0x100]'), ('cmp', 'r1, r0'),
            ('beq', '#0x234f0a'), ('b', '#0x2351b2'),
            ('ldrb', 'r0, [r4]'), ('cmp', 'r0, #0x89'),
            ('beq', '#0x234f12'), ('b', '#0x2351b2'),
            ('ldr', 'r5, [pc, #0x16c]'), ('b', '#0x234f30')]:
        raise ValueError('own message-1802/type-89 receive branch differs')
    if int.from_bytes(read(0x2e2370, 4), 'big') != 0x03ea:
        raise ValueError('own current-request discriminator base differs')
    if instructions(0x2e200c, 34) != [
            ('ldr', 'r2, [r4, #8]'), ('movs', 'r0, #0'),
            ('ldrsh', 'r0, [r2, r0]'), ('ldr', 'r1, [pc, #0x35c]'),
            ('subs', 'r0, r0, r1'), ('cmp', 'r0, #0'), ('beq', '#0x2e20e2'),
            ('subs', 'r0, #1'), ('cmp', 'r0, #0'), ('beq', '#0x2e20dc'),
            ('subs', 'r0, #1'), ('cmp', 'r0, #0'), ('beq', '#0x2e202e'),
            ('subs', 'r0, #1'), ('cmp', 'r0, #0'), ('beq', '#0x2e20e6'),
            ('b', '#0x2e20ec')]:
        raise ValueError('own current-request four-way dispatch differs')
    if instructions(0x2e20dc, 6) != [('bl', '#0x2e1f5e'), ('b', '#0x2e20ec')]:
        raise ValueError('own request-03eb continuation differs')
    if instructions(0x2e1fcc, 6) != [('adds', 'r0, r4, #0'), ('bl', '#0x2bd3d8')]:
        raise ValueError('own initial-request packed-state setter differs')
    if instructions(0x2bd3e4, 14) != [
            ('lsls', 'r1, r4, #0x15'), ('asrs', 'r1, r1, #0x10'),
            ('ldrb', 'r2, [r0, #0xd]'), ('lsls', 'r2, r2, #0x1b'),
            ('lsrs', 'r2, r2, #0x1b'), ('orrs', 'r1, r2'), ('strb', 'r1, [r0, #0xd]')]:
        raise ValueError('own record upper-three-bit setter differs')
    if instructions(0x2e1ff0, 16) != [
            ('bl', '#0x2bd424'), ('ldr', 'r0, [pc, #0x268]'),
            ('ldr', 'r1, [r0]'), ('movs', 'r0, #1'), ('strb', 'r0, [r1, #4]'),
            ('add', 'sp, #0xc'), ('pop', '{r4, r5, pc}')]:
        raise ValueError('own initial-request finish differs')
    return {'product': '5210', 'type_8b_destination_task': 12,
            'records': 40, 'record_stride': 4,
            'carrier_message_offsets': [6, 7], 'signed_rssi_message_offset': 9,
            'request_root': '13e78c', 'current_offset': 8, 'queued_offset': 12,
            'request_flag_offset': 12, 'state6_promote_argument': 0,
            'receive_context_message': '1802',
            'current_request_dispatch': {'03ea': '2e20e2', '03eb': '2e20dc',
                                         '03ec': '2e202e', '03ed': '2e20e6'},
            'initial_request_continuation': '2e1f5e',
            'initial_record_update': {'setter': '2bd3d8', 'byte_offset': 13,
                                      'preserved_mask': 31, 'value_shift': 5},
            'direct_selector_calls': [{'site': f'{site:06x}', 'argument': argument}
                                      for site, argument in direct_sites],
            'direct_census_coverage': {'aligned_pairs': (len(image) - 2) // 2,
                                       'arguments_recovered': 8,
                                       'indirect_and_tail_entries_closed': False},
            'scope': 'ROM-derived receive grammar; runtime request ownership checked separately'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(verify(args.image.read_bytes()), indent=2))
    except (OSError, ValueError) as error:
        parser.exit(1, f'5210 radio contract FAIL: {error}\n')


if __name__ == '__main__':
    main()

"""Recover NPE-3 encoding and paired mode data, not PCM clock semantics."""
import hashlib
import argparse
import json
from pathlib import Path
import re
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_BIG_ENDIAN


def parse_mode_pairs(data):
    if len(data) != 9 * 4:
        raise ValueError('paired mode table requires nine four-byte records')
    return [{'mode': index + 1,
             'command8': int.from_bytes(data[index * 4:index * 4 + 2], 'big'),
             'command9': int.from_bytes(data[index * 4 + 2:index * 4 + 4], 'big')}
            for index in range(9)]


def parameter0_pair(value, context, state):
    """Two recovered branches only; field meaning is not a hardware claim."""
    if not 0 <= value <= 0xffff or not 0 <= context <= 0xff or state not in (0, 1):
        raise ValueError('requires a halfword, context byte and decoded state0/1')
    selected = context == (1 if state == 1 else 2)
    return ((value | 0x20) & 0xfeff) if selected else ((value & 0xfedf) | 0x100)


def recover(image):
    if hashlib.sha1(image).hexdigest() != '3d9ea319503e78ec69b60d72cda23e461e118ea9':
        raise ValueError('requires acquired NPE-3 v5.56 PPM C')

    def read(address, size):
        offset = address - 0x200000
        if offset < 0 or offset + size > len(image):
            raise ValueError('control contract address outside image')
        return image[offset:offset + size]

    def u32(address):
        return int.from_bytes(read(address, 4), 'big')

    expected = {
        0x303c30: ('push', '{r4, r5, lr}'),
        0x303c3e: ('cmp', 'r5, #0xa'),
        0x303c40: ('bge', '#0x303c8e'),
        0x303c42: ('cmp', 'r5, #0'),
        0x303c44: ('beq', '#0x303c90'),
        0x303c6e: ('ldr', 'r1, [pc, #0x378]'),
        0x303c70: ('lsls', 'r0, r5, #2'),
        0x303c72: ('adds', 'r4, r1, r0'),
        0x303c74: ('subs', 'r4, #4'),
        0x303c76: ('ldrh', 'r1, [r4]'),
        0x303c78: ('movs', 'r0, #8'),
        0x303c7c: ('bl', '#0x426eb4'),
        0x303c80: ('ldrh', 'r1, [r4, #2]'),
        0x303c82: ('movs', 'r0, #9'),
        0x303c86: ('bl', '#0x426eb4'),
        0x305668: ('ldrb', 'r0, [r4, #9]'),
        0x30566a: ('bl', '#0x303c30'),
        0x305272: ('ldrb', 'r5, [r4, #8]'),
        0x30528c: ('adds', 'r2, r5, #0'),
        0x30532c: ('movs', 'r0, #0x73'),
        0x30532e: ('subs', 'r0, r2, r0'),
        0x305336: ('subs', 'r0, #3'),
        0x305338: ('cmp', 'r0, #0x36'),
        0x30533a: ('bls', '#0x3053a6'),
        0x3053a6: ('adr', 'r1, #8'),
        0x3053a8: ('lsls', 'r0, r0, #2'),
        0x3053aa: ('ldr', 'r0, [r1, r0]'),
        0x3053ac: ('mov', 'pc, r0'),
        0x3059de: ('ldrb', 'r0, [r4, #3]'),
        0x3059e6: ('subs', 'r0, r0, #2'),
        0x3059ec: ('subs', 'r0, #0x11'),
        0x3059f2: ('subs', 'r0, #6'),
        0x3059f8: ('subs', 'r0, #6'),
        0x3059fe: ('subs', 'r0, #0x1a'),
        0x305a04: ('subs', 'r0, #7'),
        0x305a08: ('beq', '#0x305aaa'),
        0x305ab8: ('bl', '#0x305258'),
        0x303cba: ('movs', 'r1, #0x80'),
        0x303cbe: ('bl', '#0x302688'),
        0x303cc2: ('strb', 'r5, [r0, #9]'),
        0x303cc6: ('bl', '#0x3026d8'),
        0x30268c: ('adds', 'r5, r1, #0'),
        0x3026b8: ('movs', 'r2, #0x40'),
        0x3026ba: ('strb', 'r2, [r0, #3]'),
        0x3026d2: ('strb', 'r5, [r0, #8]'),
        0x420bfc: ('ldrh', 'r1, [r5]'),
        0x420bfe: ('ldrh', 'r0, [r7]'),
        0x420c00: ('cmp', 'r0, r1'),
        0x420c02: ('beq', '#0x420c20'),
        0x420c04: ('ldr', 'r0, [pc, #0x368]'),
        0x420c06: ('ldrb', 'r0, [r0]'),
        0x420c08: ('movs', 'r2, #1'),
        0x420c0a: ('bl', '#0x426eb4'),
        0x420c38: ('ldrb', 'r0, [r0, r6]'),
        0x420c3a: ('ldrh', 'r1, [r5, #2]'),
        0x420c3e: ('bl', '#0x426eb4'),
        0x420c54: ('cmp', 'r6, #0xb'),
        0x420c56: ('blt', '#0x420c28'),
        0x4209b6: ('ldr', 'r1, [pc, #0x390]'),
        0x42099a: ('ldrb', 'r3, [r4, #0xa]'),
        0x42099c: ('adds', 'r1, r3, #0'),
        0x42099e: ('cmp', 'r1, #0'),
        0x4209a0: ('beq', '#0x420a10'),
        0x4209a2: ('subs', 'r1, #1'),
        0x4209a4: ('cmp', 'r1, #0'),
        0x4209a6: ('beq', '#0x4209b6'),
        0x4209ba: ('ldrb', 'r1, [r1, #0xc]'),
        0x4209bc: ('cmp', 'r1, #1'),
        0x4209be: ('bne', '#0x4209ce'),
        0x4209c0: ('movs', 'r1, #0x20'),
        0x4209c6: ('ldr', 'r2, [pc, #0x2ec]'),
        0x4209ce: ('ldr', 'r2, [pc, #0x2e8]'),
        0x4209d4: ('movs', 'r1, #0xff'),
        0x4209d6: ('adds', 'r1, #1'),
        0x4209de: ('strh', 'r1, [r5]'),
        0x420a10: ('ldrb', 'r1, [r4, #0xc]'),
        0x420a12: ('cmp', 'r1, #2'),
        0x420a14: ('bne', '#0x420a38'),
        0x420a16: ('movs', 'r2, #0x20'),
        0x420a1c: ('ldr', 'r1, [pc, #0x294]'),
        0x420a38: ('ldr', 'r2, [pc, #0x27c]'),
        0x420a3e: ('movs', 'r1, #0xff'),
        0x420a40: ('adds', 'r1, #1'),
        0x420a48: ('strh', 'r1, [r5]'),
        0x426efa: ('cmp', 'r0, #0x36'),
        0x426efe: ('adr', 'r2, #8'),
        0x4271d4: ('lsls', 'r2, r4, #9'),
        0x4271d6: ('rsbs', 'r2, r2, #0'),
        0x4271da: ('lsrs', 'r2, r2, #0x1f'),
        0x4271dc: ('lsls', 'r2, r2, #9'),
        0x4271e2: ('ldr', 'r2, [pc, #0x368]'),
        0x42719e: ('ldrh', 'r5, [r3, #2]'),
        0x42719c: ('ldr', 'r3, [pc, #0x70]'),
        0x4271a0: ('ands', 'r2, r5'),
        0x42718a: ('strh', 'r4, [r3, #2]'),
        0x42722e: ('ldr', 'r2, [pc, #0x344]'),
        0x427230: ('lsls', 'r3, r4, #0x14'),
        0x427232: ('lsrs', 'r3, r3, #0x14'),
        0x427234: ('orrs', 'r2, r3'),
        0x42723a: ('ldr', 'r2, [pc, #0x33c]'),
    }
    decoder = Cs(CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_BIG_ENDIAN)
    for address, value in expected.items():
        instruction = next(decoder.disasm(read(address, 4), address), None)
        if instruction is None or (instruction.mnemonic, instruction.op_str) != value:
            raise ValueError(f'control instruction differs at {address:x}')
    table = [u32(0x426f08 + 4 * index) for index in range(55)]
    if table[0x11] != 0x4271d4 or table[8] != 0x42722e:
        raise ValueError('control selector routing differs')
    if u32(0x42754c) != 0xfdff or u32(0x427574) != 0xffff8000:
        raise ValueError('field keep-mask or command encoding differs')
    if u32(0x427210) != 0x16ffe4 or u32(0x427578) != 0x16ffe6:
        raise ValueError('parameter-shadow ownership differs')
    if u32(0x303fe8) != 0x262444:
        raise ValueError('paired mode-table pointer differs')
    if u32(0x420f70) != 0x263884 or read(0x263884, 11) != bytes.fromhex('08091b252021222324282d'):
        raise ValueError('normal parameter-block selector table differs')
    if (u32(0x420d48), u32(0x420cb4), u32(0x420cb8)) != (0x16ed50, 0xfeff, 0xfedf):
        raise ValueError('parameter0 context or pair masks differ')
    modes = parse_mode_pairs(read(0x262444, 36))
    dispatch = [u32(0x3053b0 + index * 4) for index in range(55)]
    if [index for index, target in enumerate(dispatch) if target == 0x305668] != [10]:
        raise ValueError('paired mode consumer dispatch differs')
    calls = []
    mode_calls = []
    consumer_calls = []
    for offset in range(0, len(image) - 3, 2):
        first = int.from_bytes(image[offset:offset + 2], 'big')
        second = int.from_bytes(image[offset + 2:offset + 4], 'big')
        if first & 0xf800 != 0xf000 or second & 0xf800 != 0xf800:
            continue
        displacement = ((first & 0x7ff) << 12) | ((second & 0x7ff) << 1)
        if displacement & 0x400000:
            displacement -= 0x800000
        address = 0x200000 + offset
        target = address + 4 + displacement
        if target == 0x426eb4:
            calls.append(address)
        if target == 0x303c30:
            mode_calls.append(address)
        if target == 0x305258:
            consumer_calls.append(address)
    expected_calls = [0x303278, 0x303c7c, 0x303c86, 0x41ab0e, 0x41b07a,
                      0x420c0a, 0x420c3e, 0x420c66, 0x4272ae, 0x45dbfc, 0x46b9ee]
    if calls != expected_calls:
        raise ValueError('direct Thumb BL candidate inventory differs')
    if mode_calls != [0x30566a]:
        raise ValueError('paired mode direct-call inventory differs')
    if consumer_calls != [0x305ab8]:
        raise ValueError('paired mode consumer direct-call inventory differs')
    return {'compiler': 0x426eb4, 'selector_count': 55,
            'field_selector': 0x11, 'field_mask': 0x0200,
            'keep_mask': 0xfdff, 'parameter_selector': 8,
            'parameter_shadow': 0x16ffe6,
            'direct_bl_candidates': calls,
            'indirect_call_coverage': False,
            'paired_mode_publisher': 0x303c30,
            'paired_mode_table': 0x262444,
            'paired_modes': modes,
            'paired_mode_semantics_validated': False,
            'negative_mode_input_rejected': False,
            'paired_mode_message_byte': 9,
            'paired_mode_command_byte': 8,
            'paired_mode_command': 0x73 + 3 + 10,
            'paired_mode_consumer': 0x305258,
            'paired_mode_message_class': 0x2 + 0x11 + 6 + 6 + 0x1a + 7,
            'paired_mode_consumer_direct_bl_candidates': consumer_calls,
            'paired_mode_command80_reply_constructor': 0x303cbe,
            'paired_mode_reply_echoes_mode': True,
            'normal_parameter_publish_call': 0x420c0a,
            'normal_parameter_selectors': list(read(0x263884, 11)),
            'parameter0_context': 0x16ed50,
            'parameter0_context_byte': 12,
            'parameter0_state_byte': 10,
            'parameter0_pair_bits': [0x20, 0x100],
            'parameter0_pair_semantics_validated': False,
            'paired_mode_dispatch_table': 0x3053b0,
            'paired_mode_dispatch_records_checked': 55,
            'paired_mode_direct_bl_candidates': mode_calls,
            'speech_semantics_validated': False}


def verify_call_field(log):
    """Pin observed call/teardown selection, not PCM or native DSP execution."""
    cursor = 0
    for pattern in (
            r'6210_call_physical: action=send\b',
            r'dsp_control_write: data=870b pc=0042727c r4=0000870b r7=00000008\b',
            r'6210_call_physical: action=end\b',
            r'dsp_control_write: data=850a pc=0042727c r4=0000850a r7=00000008\b'):
        match = re.search(pattern, log[cursor:])
        if not match:
            raise ValueError('missing ordered physical call field evidence: ' + pattern)
        cursor += match.end()
    return {'parameter_command': 8, 'call_field': 0x0200,
            'pcm_validated': False, 'native_speech_validated': False}


def verify_mode_observation(log):
    verify_call_field(log)
    cursor = 0
    for pattern in (
            r'6210_control_compiler: selector=08 value=0102 publish=1 lr=00420c0f',
            r'6210_call_physical: action=send\b',
            r'6210_control_compiler: selector=08 value=070b publish=1 lr=00420c0f',
            r'dsp_control_write: data=870b pc=0042727c',
            r'6210_call_physical: action=end\b',
            r'6210_control_compiler: selector=08 value=050a publish=1 lr=00420c0f',
            r'dsp_control_write: data=850a pc=0042727c'):
        match = re.search(pattern, log[cursor:])
        if match is None:
            raise ValueError('missing positive ordered mode-observer control: ' + pattern)
        cursor += match.end()
    if '6210_paired_mode:' in log or '6210_service_command:' in log:
        raise ValueError('paired/service route differs from this normal-call fixture')
    context = verify_parameter_context(log)
    return {'normal_publish_call': 0x420c0a,
            'paired_mode_observed': False, 'service_consumer_observed': False,
            'parameter_context': context,
            'scope': 'one coherent physical outgoing call; not universal absence',
            'pcm_validated': False}


def verify_parameter_context(log):
    writes = re.findall(r'6210_parameter_context_write: pc=([0-9a-f]{8}) data=([0-9a-f]{8})', log)
    if ('0041ac32', '00000000') not in writes or any(int(value, 16) != 0 for _, value in writes):
        raise ValueError('context watch lacks initialization or differs from zero-write fixture')
    setters = re.findall(r'6210_parameter_context_set: value=([0-9a-f]+) lr=([0-9a-f]{8})', log)
    if any(int(value, 16) != 0 for value, _ in setters):
        raise ValueError('context setter differs from unattached-call fixture')
    rows = re.findall(
        r'6210_control_compiler: selector=08 value=([0-9a-f]{4}) publish=1 '
        r'lr=00420c0f context=([0-9a-f]{2}) state=([0-9a-f]{2}) '
        r'source=([0-9a-f]{8}) cache=([0-9a-f]{8})', log)
    if len(rows) < 3:
        raise ValueError('missing positive parameter-context observations')
    blocks = set()
    values = set()
    for value, context, state, source, cache in rows:
        value, context, state, source, cache = (int(item, 16) for item in (value, context, state, source, cache))
        if context != 0 or value not in (0x0102, 0x050a, 0x070b):
            raise ValueError('parameter context differs from unattached-call fixture')
        if state != (0 if value == 0x0102 else 1) or parameter0_pair(value, context, state) != value:
            raise ValueError('observed parameter0 does not satisfy its decoded branch')
        if not (0x100000 <= source < 0x180000 - 22 and
                0x100000 <= cache < 0x180000 - 22 and
                source % 2 == cache % 2 == 0 and abs(source - cache) >= 22):
            raise ValueError('source/cache parameter blocks overlap or lie outside RAM')
        blocks.add((source, cache))
        values.add(value)
    if len(blocks) != 1 or values != {0x0102, 0x050a, 0x070b}:
        raise ValueError('parameter context lacks stable blocks and all three phases')
    source, cache = next(iter(blocks))
    return {'source': source, 'cache': cache, 'context': 0,
            'observations': len(rows), 'pair': 0x100,
            'context_writes': len(writes), 'context_setter_calls': len(setters),
            'clock_semantics_validated': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('--log', type=Path)
    parser.add_argument('--mode-probe', action='store_true')
    args = parser.parse_args()
    if args.mode_probe and not args.log:
        parser.error('--mode-probe requires --log')
    try:
        result = {'static': recover(args.image.read_bytes())}
        if args.log:
            result['physical_call'] = verify_call_field(args.log.read_text(errors='replace'))
            if args.mode_probe:
                result['mode_observation'] = verify_mode_observation(args.log.read_text(errors='replace'))
        print(json.dumps(result, indent=2))
    except (OSError, ValueError) as error:
        parser.exit(1, f'NPE-3 DSP control FAIL: {error}\n')


if __name__ == '__main__':
    main()

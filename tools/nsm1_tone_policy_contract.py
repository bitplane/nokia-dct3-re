"""Own NSM-1 tone admission and data-driven settings sequence; no field labels."""
from tools.nsm1_v523_static_check import instruction, literal, read32, decode_group7


def contract(image):
    anchors = {
        0x2A8736: ('ldrb', 'r3, [r1, #4]'),
        0x2A873C: ('ldrb', 'r0, [r0, #0xe]'),
        0x2A8740: ('bne', '#0x2a874a'),
        0x2A8744: ('lsrs', 'r0, r0, #1'),
        0x2A8746: ('bhs', '#0x2a874a'),
        0x2A8748: ('b', '#0x2a88e8'),
        0x291FD2: ('ldrb', 'r2, [r0, #5]'),
        0x291FD4: ('strb', 'r2, [r1, #7]'),
        0x2923C2: ('cmp', 'r4, #7'),
        0x292C10: ('cmp', 'r0, #1'),
        0x292C30: ('movs', 'r0, #6'),
        0x2BD3B2: ('strb', 'r0, [r5, r6]'),
        0x296578: ('bl', '#0x2c0118'),
        0x2A1E6E: ('movs', 'r0, #0x39'),
        0x2A1E70: ('lsls', 'r0, r0, #5'),
        0x2A1E72: ('bl', '#0x2bd830'),
        0x280C20: ('bl', '#0x2a1e64'),
        0x2BB824: ('movs', 'r0, #0'),
        0x2BB826: ('bl', '#0x2c32f4'),
        0x280B60: ('bl', '#0x2bb822'),
        0x280B6C: ('blt', '#0x280b70'),
        0x280B6E: ('b', '#0x28051e'),
        0x28051E: ('movs', 'r1, #0x39'),
        0x280520: ('lsls', 'r1, r1, #4'),
    }
    for address, expected in anchors.items():
        insn = instruction(image, address)
        if (insn.mnemonic, insn.op_str) != expected:
            raise ValueError(f'tone policy instruction differs at {address:#x}')
    expected_literals = {
        0x291FB8: 0x111EE0, 0x291FBC: 0x11FC00,
        0x2923AA: 0x111EE0, 0x2923B2: 0x762,
        0x2923B4: 0x7FF0, 0x2BD268: 0x2CA91C,
        0x2BD2DA: 0x2CB954, 0x2BD39C: 0x2CB954,
        0x2BD3A8: 0x11FC00,
        0x280B68: 0x312,
    }
    for address, expected in expected_literals.items():
        if literal(image, address) != expected:
            raise ValueError(f'tone policy literal differs at {address:#x}')
    record = next(r for r in decode_group7(image) if r['id'] == 0x762)
    if record != {'id': 0x762, 'offset': 0x3F9C, 'size': 12}:
        raise ValueError('settings record descriptor differs')
    callback = 0x2CB954 + 0xA5 * 8
    if image[callback-0x200000:callback-0x200000+8] != bytes.fromhex('c0ff3f0062b90000'):
        raise ValueError('callback a5 record differs')
    sequence = 0x2CA91C + (0x62B9 & 0x1FFF) * 4
    if sequence != 0x2CB400 or [read32(image, sequence+i*4) for i in range(3)] != [0x4194, 1, 0xDC]:
        raise ValueError('channel-disable sequence differs')
    if read32(image, 0x2DB038) != 0x2DAD68 or image[0xDB03C:0xDB040] != bytes.fromhex('01000000'):
        raise ValueError('tone descriptor differs')
    for index, raw in ((0x1E2, '8e3f4100ffff0000'), (0x1E8, '8e3f4000ffff0000')):
        offset = 0xCB954 + index*8
        if image[offset:offset+8] != bytes.fromhex(raw):
            raise ValueError('selector set/clear transition differs')
    return dict(callback=0xA5, sequence=0x62B9, event=0x194, channel=1,
                admission=0x111922, settings_key=0x762, settings_size=12,
                settings_offset=0x3F9C, settings_variants=7,
                selector_set_event=0x720, selector_clear_event=0x721,
                selector_event_arguments=0,
                analog_selector=0, analog_thresholds=[0x312, 0x390],
                physical_channel_identity_proved=False,
                settings_field_semantics_proved=False)

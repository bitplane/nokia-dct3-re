"""Inspect own NSK-1 startup/catalogue structure; no runtime or mask claim."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.dct3_eeprom_descriptor import eeprom_geometry


PROFILES = {
    'standard': ('a2a99ec6c52104eebcbae281cf8e845bdc8ea1d9',0x296d10),
    '2915': ('64068df6b7950b47f34ac141d0ef9c51dc666cca',0x296d00),
}


def read_eeprom_output_contract(image, variant):
    """Check own START/STOP GPIO operations, not fitted storage or input wiring."""
    shift = {'standard': 0, '2915': -0x10}[variant]
    fragments = (
        (0x29095a, '22012380029b33207819431170197919431171192004781943017019'),
        (0x290998, '781943917019'),
        (0x2909ba, '781943817019'),
        (0x2909cc, '22012380029b33207819439170197919431171192004781943017019'),
        (0x290a08, '781943117019'),
    )
    for address, encoded in fragments:
        expected = bytes.fromhex(encoded)
        offset = address + shift - 0x200000
        if image[offset:offset+len(expected)] != expected:
            raise ValueError('EEPROM START/STOP output instruction contract differs')
    return {'start_entry': hex(0x2909c6+shift),
            'stop_entry': hex(0x290954+shift),
            'output_register': '0x20020', 'direction_register': '0x20024',
            'sda_output_bit': 0, 'scl_output_bit': 2,
            'input_wiring_or_fitted_part_proved': False}


def read_peripheral_contract(image, variant):
    """Own GPIO input and serial instruction fragments, not electrical timing."""
    shift = {'standard': 0, '2915': -0x10}[variant]
    fragments = (
        (0x29088e, '25802601270446a6200002ab3320791a781c43b2711a'),
        (0x2908c8, '433c701c'),
        (0x2908e6, '78194231d000432843bc701c086dd1d6'),
        (0x291830, '4835222821225411222a21044321541121295c0908c9d3fb212d5c08'),
        (0x291908, '00020000'),
        (0x292172, '4a7621295c890849d3fb212c548846f7'),
        (0x2921d8, '4a5c21295c890849d3fb212b548846f7'),
        (0x29234c, '00020000'),
        (0x2933f8, '2031231f5503202f26e0550620305d0006c00ec006000e0125ff291f'),
        (0x293424, '203143d7550720305d0006c00ec006000e004681281f'),
        (0x29346a, '2805dbe8203155021c4806000e012905dbcf'),
        (0x293558, '00020000'),
        (0x281008, '213820ff550820c07220f00afd6c7aa006400e4072a0f00afd73f00afd6420bf7aa1400872a0f00afd6b213a20035508f00afd5920207b6143087360f00afd60213d201855082012550820065508213e201a550820105508200455082039213255012001'),
        (0x281188, '00020000'),
        (0x280c22, '203919012080780a43107008'),
    )
    for address, encoded in fragments:
        offset = address+shift-0x200000
        expected = bytes.fromhex(encoded)
        if image[offset:offset+len(expected)] != expected:
            raise ValueError('peripheral input/serial instruction contract differs')
    return {'eeprom_read_entry': hex(0x290886+shift),
            'eeprom_sda_input_register': '0x20020', 'eeprom_sda_input_bit': 0,
            'ccont_select_offset': '0x28', 'ccont_select_value': '0x22',
            'ccont_command_offset': '0x2a', 'ccont_receive_offset': '0x2d',
            'serial_status_offset': '0x29', 'ccont_ready_bit': 2,
            'lcd_data_offset': '0x2b', 'lcd_command_offset': '0x2c',
            'lcd_ready_bit': 0, 'keypad_scan_entry': hex(0x2933c4+shift),
            'keypad_rows': 5, 'keypad_row_signal_offset': '0x31',
            'keypad_row_direction_offset': '0x2f', 'keypad_column_input_offset': '0x30',
            'simi_init_entry':hex(0x280ff8+shift),'simi_control_offset':'0x39',
            'simi_iir_offset':'0x38','simi_rx_fifo_control_offset':'0x3d','simi_tx_fifo_control_offset':'0x3e',
            'simi_clock_enable_bit':5,'simi_initial_fiq_mask_bits':[6,7],
            'simi_control_activate_bit':7,
            'physical_key_mapping_proved': False, 'electrical_timing_proved': False}


def phonecode_contract(image, variant):
    """Own retry gate and record directory; no credential/provisioning inference."""
    shift = {'standard':0,'2915':-0x10}[variant]
    start = 0x8f4e0+shift
    if image[start:start+12] != bytes.fromhex('b510493e78092905db05202c'):
        raise ValueError('own phone-code retry gate differs')
    def word(address):
        offset = address+shift-0x200000
        if offset < 0 or offset+4 > len(image):
            raise ValueError('phone-code literal outside image')
        return struct.unpack_from('>I',image,offset)[0]
    if word(0x28f5e8) != 0x709:
        raise ValueError('own retry record identifier differs')
    directory = word(0x28ca64)
    group = directory-0x200000+7*8
    if group < 0 or group+8 > len(image):
        raise ValueError('own security directory outside image')
    pointer,limit,_ = struct.unpack_from('>IHH',image,group)
    base = pointer-0x200000
    if limit < 9 or base < 0 or base+(limit+1)*8 > len(image):
        raise ValueError('own security records outside image')
    records = {}
    for index,size in ((1,8),(8,4),(9,1)):
        identifier,offset,length = struct.unpack_from('>IHH',image,base+index*8)
        if identifier != 0x700+index or length != size or offset+length > 2048:
            raise ValueError('own security record identity or extent differs')
        records[identifier] = (offset,length)
    context,retry = word(0x28f5d4),word(0x28f5dc)
    if (not 0x100000 <= context <= 0x110000-8 or retry != context+4
            or records[0x708][0] != records[0x701][0]
            or records[0x709][0] != records[0x701][0]+4):
        raise ValueError('own retry field does not match security context')
    return {'validator':hex(0x28f534+shift),'retry_gate':hex(0x28f4e0+shift),
            'retry_read':hex(0x28f4e4+shift),'retry_ram':hex(retry),
            'retry_record':'0x709','retry_eeprom':hex(records[0x709][0]),
            'encoded_code_eeprom':hex(records[0x708][0]),'rejected_count_at_least':5,
            'scope':'own retry/record layout, not a factory credential or retry default'}


def read_keymap_contract(image, variant):
    shift = {'standard':0,'2915':-0x10}[variant]
    decoder = bytes.fromhex('b500f7fefd7128ffd101203ebd0009c12900d00949167809008a188918404913180838807800bd0021194a1178124351184049105c08bd00')
    start = 0x2948dc+shift-0x200000
    if image[start:start+len(decoder)] != decoder:
        raise ValueError('own keypad decoder instruction contract differs')
    start = 0x2951be+shift-0x200000
    if image[start:start+8] != bytes.fromhex('f7fef9012884d1f9'):
        raise ValueError('own power-key raw-84 comparison differs')
    normal,special,selector = (struct.unpack_from('>I',image,address+shift-0x200000)[0]
                               for address in (0x294950,0x294948,0x29494c))
    expected = bytes.fromhex('3e3e3e3e3e3e1a0102033e19040506183e0708093e170c0a0b')
    for pointer,size in ((normal,25),(special,5)):
        if pointer < 0x200000 or pointer+size > 0x200000+len(image):
            raise ValueError('own key table outside image')
    if (image[normal-0x200000:normal-0x200000+25] != expected
            or image[special-0x200000:special-0x200000+5] != bytes.fromhex('3e3e3e3e0d')
            or not 0x100000 <= selector < 0x110000):
        raise ValueError('own key table or layout selector differs')
    return {'normal_table':hex(normal),'special_table':hex(special),'layout_selector':hex(selector),
            'raw_formula':'row * 5 + column','layout_zero_logical_keys':list(expected),
            'power_raw':'0x84','power_logical':'0x0d','power_column_mask':'0x10',
            'runtime_key_forwarding_proved':False}


def read_catalogue(image,table):
    offset = table-0x200000
    if offset < 8 or offset+112 > len(image):
        raise ValueError('catalogue table outside image')
    size,destination = struct.unpack_from('>II',image,offset-8)
    if size != 112 or not 0x100000 <= destination <= 0x110000-size:
        raise ValueError('catalogue initializer bounds differ')
    pointers = struct.unpack_from('>28I',image,offset)
    if [i for i,pointer in enumerate(pointers) if not pointer] != [11,26,27]:
        raise ValueError('catalogue null-slot layout differs')
    entries = []
    for index,pointer in enumerate(pointers):
        if not pointer:
            continue
        start = pointer-0x200000
        if start < 0 or start+12 > len(image) or pointer & 1:
            raise ValueError('descriptor pointer outside image or unaligned')
        header = struct.unpack_from('>6H',image,start)
        end = start+12+header[2]*2
        if end > len(image) or header[0]+header[2] > 0x10000:
            raise ValueError('descriptor payload or declared destination outside bounds')
        entries.append({'slot':index,'descriptor':hex(pointer),'header':list(header),
                        'payload_sha1':hashlib.sha1(image[start+12:end]).hexdigest()})
    return {'initializer_destination':hex(destination),'slots':28,'nonzero':len(entries),
            'entries':entries,'execution_or_mask_identity_proved':False}


def inspect(image,variant):
    if variant not in PROFILES:
        raise ValueError('unknown NSK-1 variant')
    digest,table = PROFILES[variant]
    if len(image) != 0x100000 or hashlib.sha1(image).hexdigest() != digest:
        raise ValueError('requires own pinned NSK-1 normalized variant')
    catalogue = read_catalogue(image,table)
    if catalogue['entries'][-1]['header'] != [0x0f00,0,186,0x0700,180,0]:
        raise ValueError('bootstrap descriptor differs')
    # Own ARM ADD/BX pair enters Thumb at 2000ec, not at flash reset entry.
    if image[0xe4:0xec] != bytes.fromhex('e28f0001e12fff10'):
        raise ValueError('startup interworking boundary differs')
    return {'variant':variant,'sha1':digest,'catalogue':catalogue,
            'eeprom_output_contract':read_eeprom_output_contract(image,variant),
            'peripheral_contract':read_peripheral_contract(image,variant),
            'keymap_contract':read_keymap_contract(image,variant),
            'phonecode_contract':phonecode_contract(image,variant),
            'arm_entry':'0x200040','thumb_entry':'0x2000ec',
            'firmware_eeprom_geometry':eeprom_geometry(image[5]),
            'stack_literal':hex(int.from_bytes(image[0x16c:0x170],'big')),
            'boot_or_fitted_hardware_proved':False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image',type=Path)
    parser.add_argument('--variant',choices=PROFILES,default='standard')
    args = parser.parse_args()
    try:
        result = inspect(args.image.read_bytes(),args.variant)
    except (OSError,ValueError,struct.error) as error:
        parser.exit(1,f'NSK-1 static check failed: {error}\n')
    print(json.dumps(result,indent=2))


if __name__ == '__main__':
    main()

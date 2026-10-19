"""Read-only own-ROM equipment-identity contract; not an NV provisioner."""
import argparse
import hashlib
import json
import re
from pathlib import Path
import struct
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.noki5510_nv_reads_check import records, source_contract as nv_source_contract, check_reads


def equipment_nibbles(packed):
    if len(packed) != 8:
        raise ValueError('expected the eight-byte NV read at 000c')
    return [nibble for byte in packed[:7] for nibble in (byte >> 4, byte & 15)]


def phone_code_from_nv(packed):
    """29f876 first-five-digit validation; sixth nibble is not validated."""
    if len(packed) != 3:
        raise ValueError('expected three phone-code NV bytes at 0110')
    digits = [n for byte in packed for n in (byte >> 4,byte & 15)][:5]
    valid = all(n < 10 for n in digits)
    return {'code':''.join(map(str,digits)) if valid else '12345',
            'stored_digits_valid':valid}


def check_phone_code_capture(text, cache):
    if len(cache) != 0x3800:
        raise ValueError('expected complete logical NV cache')
    entries = records(text,'5510_phone_code_entry')
    ends = records(text,'5510_phone_code_return')
    if not 0 < len(entries) == len(ends) < 16:
        raise ValueError('missing, unmatched or capped phone-code calls')
    expected = phone_code_from_nv(cache[0x110:0x113])
    previous = -1
    for ordinal,(entry,end) in enumerate(zip(entries,ends),1):
        start = text.index(f'5510_phone_code_entry: ordinal={ordinal} ')
        finish = text.index(f'5510_phone_code_return: ordinal={ordinal} ')
        reads = records(text[start:finish],'5510_nv_read')
        if (not previous < start < finish or len(reads) != 1
                or reads[0]['offset'] != '00000110' or reads[0]['bytes'] != '00000003'
                or reads[0]['owner'] != '0029f88f'
                or entry['ordinal'] != str(ordinal) or end['ordinal'] != str(ordinal)
                or entry['destination'] != end['destination']
                or int(entry['sp'],16) != int(end['sp'],16)+20
                or end['stored_valid'] != str(int(expected['stored_digits_valid']))
                or end['output'] != expected['code'].encode('ascii').hex()
                or end['terminator'] != '00'):
            raise ValueError('phone-code read/frame/result differs from own source')
        previous = finish
    return {'calls':len(entries),**expected,
            'scope':'Observed firmware getter, not code entry, unlock or valid product NV'}


def factory_request_word_source(firmware):
    """Own 39cc20(0)/39ce0e source equations, not a general PPM validator."""
    if len(firmware) != 0x350000:
        raise ValueError('expected complete NPM-5 logical flash extent')
    pointer = struct.unpack_from('>I',firmware,0x24d34c)[0]
    start = pointer-0x200000+4
    if not 0 <= start <= len(firmware)-36:
        raise ValueError('PPM selector probe outside acquired image')
    nonerased = any(value != 255 for value in firmware[start:start+36])
    address = 0x54fffc if nonerased else 0x200038
    return {'ppm_pointer':f'{pointer:08x}', 'probe_bytes':36,
            'probe_has_nonerased_byte':nonerased,
            'selected_flash_address':f'{address:08x}',
            'selected_word':firmware[address-0x200000:address-0x200000+4].hex(),
            'scope':'Own source selector, not PPM checksum validity or chip identity'}


def record_reply_entry_eligible(message, record_index, state):
    """29ffe8..2a005c only: not the later record acceptance decision."""
    if len(message) < 0x3e or not 0 <= record_index <= 255 or not 0 <= state <= 255:
        raise ValueError('requires a complete inspected message and byte state')
    eligible = record_index < 10
    if state == 1 or not eligible or message[9] == 0x32:
        return eligible
    if message[9] != 0x34:
        return False
    total = sum(struct.unpack_from('>25H', message, 10)) & 0xffff
    return struct.unpack_from('>H', message, 0x3c)[0] == (total ^ 0xffff)


def record_reply_nibble_filter(packed):
    """2a01dc..2a0218 only; permissive record syntax, not decimal validity."""
    if len(packed) != 8:
        raise ValueError('expected reply offsets 0c..13')
    for value in packed[:7]:
        if value >= 0xc0 and value >> 4 != 15:
            return False
        if value & 12 == 12 and value & 15 != 15:
            return False
    return not (packed[7] >= 0xc0 and packed[7] >> 4 != 15)


def record_reply_slot_binding(message, cache, record_index):
    """2a02aa..2a02e8 equality only, after its state/feature preconditions."""
    if len(message) < 0x3c or len(cache) != 0x3800 or not 0 <= record_index < 10:
        raise ValueError('requires complete reply, logical cache and slot 0..9')
    offset = 0x20 + 24*record_index
    return (message[0x24:0x30] == cache[offset:offset+12]
            and message[0x30:0x3c] == cache[offset+12:offset+24])


def record_scan_tail(index, armed, retained):
    """2a04ba..2a052a branch only, after earlier handler state updates."""
    if any(not isinstance(value,int) or not 0 <= value <= 255
           for value in (index,armed,retained)):
        raise ValueError('scan-tail inputs must be byte state')
    if index >= 10 or armed != 1:
        return {'request_index':None,'stored_index':index,'armed':0}
    if retained != 0:
        return {'request_index':None,'stored_index':index,'armed':armed}
    next_index = index + 1
    return {'request_index':next_index % 10,'stored_index':next_index,
            'armed':armed,'state':2}


def check_record_capture(text, flash_word=None):
    """Qualify the observed erased-NV frontier, not a general reply protocol."""
    scopes = records(text, '5510_records_scope')
    ends = records(text, '5510_records_terminal')
    if scopes != [{'reply34':'0029fdc4','reply35':'0029ffe8',
                   'state':'001204dc','cap':'16','observer_writes':'0'}]:
        raise ValueError('missing or changed record observer scope')
    if ends != [{'reply34':'0','reply35':'0','state':'02','index':'00',
                 'receipt34':'00','cap':'16','observer_writes':'0'}] or records(text,'5510_records_reply'):
        raise ValueError('capture differs from the qualified no-reply frontier')
    expected = [(6,'1304',4),(14,'140c',12),(22,'1514',20),(26,'1618',24)]
    streams = []
    for prefix, extra in [('dsp_hle: TX packet','radio_phase=\\S+'),
                          ('dspif_transport: TX consume','consumer=[0-9a-f]+')]:
        rows = re.findall(re.escape(prefix) + r' type=70 payload=(\d+) words=(\d+) ' + extra +
                          r' data=([0-9a-f]+) t=(\d+\.\d+)', text)
        selected = [(int(n),int(words),data,float(t)) for n,words,data,t in rows if data[:4] in
                    {item[1] for item in expected}]
        if len(selected) != 4:
            raise ValueError('missing or repeated factory-record request')
        for (length,words,data,t),(wanted,header,count) in zip(selected,expected):
            if (length != wanted or words != (length+2)//2
                    or len(data) != length*2 or not data.startswith(header)):
                raise ValueError('factory-record request order or size changed')
            if header != '1304' and data[4:] != 'ff'*count:
                raise ValueError('factory-record payload is not the qualified erased NV')
        if any(a[3] >= b[3] for a,b in zip(selected,selected[1:])):
            raise ValueError('factory-record request times not increasing')
        streams.append(selected)
    if streams[0] != streams[1]:
        raise ValueError('HLE and transport factory-record observations disagree')
    if flash_word is not None:
        words = records(text,'5510_records_word')
        if (len(words) != 1 or words[0].get('value') != flash_word
                or words[0].get('flash_tail') != flash_word
                or streams[0][0][2] != '1304'+flash_word):
            raise ValueError('record request word differs from the own flash-tail observation')
    return {'requests': [{'payload':data,'words':words,'time':t} for _,words,data,t in streams[0]],
            'reply34_entries':0,'reply35_entries':0,
            'scope':'Bounded erased-NV capture; not reply necessity or boot acceptance'}


def wire_identity(packed, selector, svn=6):
    """Original byte operations, including malformed NV; no decimal validation."""
    equipment_nibbles(packed)
    if not 0 <= selector <= 0xffffffff or not 0 <= svn < 100:
        raise ValueError('reference covers word selectors and decimal two-digit SVN')
    result = bytearray([(packed[0] & 0xf0) | (3 if selector == 2 else 10)])
    result.extend((packed[index] & 15) | (packed[index+1] & 0xf0) for index in range(6))
    result.append((packed[6] & 15) | ((svn // 10) << 4 if selector == 2 else 0))
    if selector == 2:
        result.append(0xf0 | (svn % 10))
    return bytes(result)


def display_identity(packed, flags, svn=6):
    """Only the four decoded decimal-input display forms, not every flag combination."""
    digits = equipment_nibbles(packed)
    if any(digit > 9 for digit in digits) or flags not in (1,3,5,9) or not 0 <= svn < 100:
        raise ValueError('display reference requires decimal identity and a qualified form')
    text = ''.join(map(str,digits))
    if flags == 3:
        total = sum(digits[::2]) + sum(sum(divmod(2*digit,10)) for digit in digits[1::2])
        text += str((-total) % 10)
    elif flags == 5:
        text += '0'
    elif flags == 9:
        text += f'{svn:02d}'
    return text


def identity_contract(firmware):
    # This validates the complete own image and its reader/wrapper reference census.
    contract = nv_source_contract(firmware)
    pointer = struct.unpack_from('>I',firmware,0xa06a8)[0]
    if pointer != 0x44d844 or firmware[pointer-0x200000] != 6:
        raise ValueError('own software-version source changed')
    if firmware[0x9fbf4:0x9fbfa] != b'12345\0':
        raise ValueError('own phone-code fallback changed')
    character_table = struct.unpack_from('>I',firmware,0x1b03d8)[0]-0x200000
    if [bool(firmware[character_table+i] & 4) for i in range(0x30,0x40)] != [True]*10+[False]*6:
        raise ValueError('own digit classifier domain changed')
    return {'nv_contract': contract, 'nv_offset': '000c', 'nv_read_bytes': 8,
            'equipment_digits': 14, 'software_version_address': f'{pointer:08x}',
            'software_version_decimal': 6,
            'formatter': '002a05cc', 'imei_reader': '002a066a', 'imeisv_reader': '002a0640',
            'display_formatter': '0029fc00',
            'phone_code': {'nv_offset':'0110','nv_bytes':3,'getter':'0029f876',
                           'fallback_address':'0029fbf4','fallback':'12345',
                           'validated_digits':5,'sixth_nibble_validated':False},
            'factory_request13_sources': {
                'selector':'0039cc20', 'fallback_flash_address':'00200038',
                'fallback_flash_word':firmware[0x38:0x3c].hex(),
                'alternate_getter':'0039ce0e', 'alternate_flash_address':'0054fffc',
                'alternate_flash_word':firmware[-4:].hex(),
                'source_selection':factory_request_word_source(firmware),
                'scope':'Flash-derived words, not a recovered chip-ID register'},
            'factory_record_continuation': {
                'constructor':'0029ecf8', 'type':'70', 'subcommand':'17',
                'body_bytes':48, 'send_call':'0029edd6', 'destination_argument':3,
                'nv_record_base':'0020', 'nv_record_stride':24,
                'record_half_order':'normal unless fifth argument is 1, then swapped',
                'scope':'Source constructor only; not a qualified reply encoder or observed continuation'},
            'factory_record_reply_dispatch': {
                'selector_offset': 8,
                '34': {'branch': '0024c9d8', 'call': '0024cadc',
                       'consumer': '0029fdc4', 'source_offset': 11,
                       'copy_bytes': 13, 'destination_state_offset': 32,
                       'receipt_flag_offset': 3},
                '35': {'branch': '0024c9fc', 'call': '0024cad4',
                       'consumer': '0029ffe8',
                       'scope': 'Entry mapped; full state-dependent validation not yet qualified'},
                'scope': 'Own MCU dispatcher; not executed replies or a qualified DSP encoder'},
            'division_abi': '003ad490: R0 remainder, R1 quotient; positive decimal operands here',
            'code_sha256': {f'{first:08x}..{last:08x}': hashlib.sha256(
                firmware[first-0x200000:last-0x200000]).hexdigest()
                for first,last in ((0x2a05cc,0x2a0692),(0x29fc00,0x29fd14),
                                   (0x3ad490,0x3ad542),(0x24c9d2,0x24cafc),
                                   (0x29fdc4,0x29fdde),(0x29ffe8,0x2a005c),
                                   (0x29ecf8,0x29ede6),(0x2a01dc,0x2a021e),
                                   (0x39cc20,0x39cc4e),(0x39ce0e,0x39ce14),
                                   (0x2a02aa,0x2a02ea),(0x3aea44,0x3aeabc),
                                   (0x2a04ba,0x2a053a),(0x29f876,0x29f8e4),
                                   (0x3b03cc,0x3b03dc))},
            'scope': 'Pinned own source and bounded reference equations; not encrypted-record binding, valid product NV or boot acceptance'}


def check_formatting(text, contract):
    scopes, entries, returned, terminals = (records(text,marker) for marker in
        ('5510_identity_scope','5510_identity_entry','5510_identity_return','5510_identity_terminal'))
    if (len(scopes) != 1 or scopes[0] != {'entry':'002a05cc','cap':'16',
            'identity':'debugger_entry','operands':'context_only','observer_writes':'0'}
            or len(terminals) != 1 or terminals[0].get('cap') != '16'
            or not 0 < len(entries) == len(returned) == int(terminals[0]['entries']) < 16):
        raise ValueError('missing, repeated or capped identity formatting capture')
    reports = []
    previous_end = -1
    for ordinal,(entry,end) in enumerate(zip(entries,returned),1):
        selector = int(entry['selector'])
        if (int(entry['ordinal']) != ordinal or int(end['ordinal']) != ordinal
                or end['selector'] != entry['selector']
                or int(entry['sp'],16) != int(end['sp'],16)+16
                or any(entry[field] != end[field] for field in ('source','destination'))
                or end['address'] != ('002a063e' if selector == 2 else '002a060a')):
            raise ValueError('identity entry/return frame mismatch')
        start_marker = f'5510_identity_entry: ordinal={ordinal} '
        end_marker = f'5510_identity_return: ordinal={ordinal} '
        first,last = text.index(start_marker),text.index(end_marker)
        if not previous_end < first < last:
            raise ValueError('overlapping or reversed identity calls')
        previous_end = last
        preceding = records(text[:first],'5510_nv_read')
        if (not preceding or preceding[-1]['offset'] != '0000000c'
                or preceding[-1]['bytes'] != '00000008'
                or preceding[-1]['destination'] != entry['source']):
            raise ValueError('identity input not paired with its preceding NV request')
        packed = bytes.fromhex(entry['packed'])
        expected = wire_identity(packed,selector,contract['software_version_decimal'])
        destination, stack = int(entry['destination'],16), int(end['sp'],16)
        if not (0x100000 <= destination < destination+len(expected) <= 0x180000
                and 0x100000 <= stack < stack+16 <= 0x180000):
            raise ValueError('identity output/frame outside observed RAM domain')
        if bytes.fromhex(end['output']) != expected:
            raise ValueError('original identity output differs from source byte model')
        digits = equipment_nibbles(packed)
        valid = all(digit <= 9 for digit in digits)
        reports.append({'selector':selector,'packed':packed.hex(),'wire':expected.hex(),
                        'decimal_equipment_digits': ''.join(map(str,digits)) if valid else None,
                        'decimal_domain_valid': valid})
    return {'calls':reports, 'scope':'Ordered original formatter input/output contexts and prior NV request; '
            'not copy retirement, unobserved selector execution, valid provisioning or registration acceptance'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--firmware',type=Path,required=True)
    parser.add_argument('--records',action='store_true',help='require bounded factory-record capture')
    parser.add_argument('--phone-code',action='store_true',help='require original phone-code getter capture')
    args = parser.parse_args()
    try:
        contract = identity_contract(args.firmware.read_bytes())
        text = (args.directory/'error.log').read_text()
        inventory = check_reads(text,(args.directory/'npm5_nv_cache.bin').read_bytes(),contract['nv_contract'])
        result = check_formatting(text,contract)
        if args.records:
            result['record_capture'] = check_record_capture(text,
                contract['factory_request13_sources']['source_selection']['selected_word'])
        if args.phone_code:
            result['phone_code_capture'] = check_phone_code_capture(text,
                (args.directory/'npm5_nv_cache.bin').read_bytes())
        result.update(source_contract=contract,nv_requests=inventory['requests'],
                      valid_product_nv=False, unobserved_selector_execution_claim=False)
        print(json.dumps(result,indent=2))
    except (ValueError,KeyError,OSError) as error:
        parser.exit(1,f'NPM-5 identity contract FAIL: {error}\n')


if __name__ == '__main__':
    main()

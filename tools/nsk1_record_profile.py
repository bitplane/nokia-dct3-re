"""Constrained external-input experiment, not factory NSK-1 provisioning.

The two numeric lock-record examples were published by myke2002 in the
2003 DCT3 MBUS discussion. Codec implementation is the independently
recovered native word arithmetic, not code transcribed from that post.
No NSE-1 IMEI, MSID, FAID, retry or calibration records are imported.
"""
import hashlib
import json

from tools.dct3_rom4_record_codec import encode_record,retained_record_transform,LOCK_TABLE_82,ENCODE_SCHEDULE
from tools.nsk1_record_exchange_check import check
from tools.nsk1_static_check import phonecode_contract
from tools.dct3_lock_record_example import SOURCE,EXAMPLE_RECORDS


def repair_security_checksum(image):
    """Own normal-allocation path at 22c158; no semantic field reconstruction."""
    if len(image) != 2048:
        raise ValueError('security checksum requires own 2048-byte EEPROM')
    result = bytearray(image)
    checksum = (sum(image[0x40:0x11e])-sum(image[0x74:0x76])) & 0xffff
    result[0x11e:0x120] = checksum.to_bytes(2,'big')
    return bytes(result)


def initialize_phone_retry(image, flash):
    if len(image) != 2048:
        raise ValueError('phone retry fixture requires own 2048-byte EEPROM')
    contract = phonecode_contract(flash,'standard')
    result = bytearray(image)
    result[int(contract['retry_eeprom'],16)] = 0
    return bytes(result)


def build_profile(chip):
    """Supply only the published record pair and its own low-block checksum."""
    image = bytearray([255])*2048
    encoded = b''.join(encode_record(EXAMPLE_RECORDS[start:start+12],chip) for start in (0,12))
    # Own cold staging applies this involution using the twelve bytes at zero.
    image[0x20:0x38] = retained_record_transform(encoded,image[:12])
    # Own 25ac1a compares sum16(0000..003b) with the BE32 field at 003c.
    image[0x3c:0x40] = (sum(image[:0x3c]) & 0xffff).to_bytes(4,'big')
    return bytes(image)


def build_from_capture(source, expected_inputs):
    manifest = json.loads((source/'input_manifest.json').read_text())
    expected = [{'source':name,'sha1':digest} for name,digest in expected_inputs]
    if (manifest.get('inputs') != expected or manifest.get('peer_completions') is not False
            or manifest.get('physical_keys') is not False
            or manifest.get('adc2_source',1023) != 1023
            or manifest.get('inserted_synthetic_sim',False) is not False
            or manifest.get('external_eeprom') != 'own erased 2048 bytes; no donor provisioning'):
        raise ValueError('record profile requires qualified own erased NSK-1 capture')
    for name,digest in expected_inputs:
        if hashlib.sha1((source/'inputs/nsk1r4t'/name.rsplit('/',1)[-1]).read_bytes()).hexdigest() != digest:
            raise ValueError('record-profile capture inputs changed')
    summary = json.loads((source/'summary.json').read_text())
    if (summary.get('variant') != 'standard' or summary.get('unchanged_flash') is not True
            or summary.get('own_slot25_payload_observed') is not True
            or summary.get('native_illegal_observed') is not False):
        raise ValueError('record profile requires own standard-family unchanged-flash observation')
    storage = source/'nvram/nsk1r4t'
    if (hashlib.sha1((storage/'flash').read_bytes()).hexdigest() != expected_inputs[0][1]
            or len((storage/'eeprom').read_bytes()) != 2048
            or (storage/'eeprom').read_bytes()[:64] != bytes([255])*64):
        raise ValueError('captured own flash/low EEPROM no longer matches the erased control')
    drom = (source/'inputs/nsk1r4t/nse1_rom4_dsp_data.bin').read_bytes()
    if (drom[0xb6df*2:0xb6df*2+12] != LOCK_TABLE_82
            or drom[0xb6f7*2:0xb6f7*2+24] != b''.join(word.to_bytes(2,'big') for word in ENCODE_SCHEDULE)
            or drom[0xb703*2:0xb703*2+2] != bytes.fromhex('54c2')):
        raise ValueError('captured resident data does not match the independently checked codec constants')
    trace_bytes = (source/'error.log').read_bytes()
    trace = trace_bytes.decode('utf-8')
    records = check(trace,initial_eeprom=bytes([255])*2048)['exchanges']
    chips = {row['chip_field'] for row in records}
    if len(chips) != 1:
        raise ValueError('captured modeled chip field is not stable')
    chip = bytes.fromhex(chips.pop())
    profile = build_profile(chip)
    return profile, {'classification':'diagnostic published lock-record example, not factory data',
                     'numeric_example_source':SOURCE, 'chip_field':chip.hex(),
                     'capture_trace_sha256':hashlib.sha256(trace_bytes).hexdigest(),
                     'eeprom_sha256':hashlib.sha256(profile).hexdigest(),
                     'written_ranges':['0020..0037','003c..003f'],
                     'identity_and_calibration':'remain erased',
                     'decoded_record_example':EXAMPLE_RECORDS.hex()}


def check_observation(summary, initial_eeprom, final_eeprom):
    """Require the real accepted-copy path, not only a format-valid intermediate."""
    expected = b''.join(EXAMPLE_RECORDS[start:start+10]+bytes(2) for start in (0,12)).hex()
    accepts = summary.get('record_accepts',[])
    exchanges = summary.get('record_arithmetic',{}).get('exchanges',[])
    if (len(initial_eeprom) != 2048 or len(final_eeprom) != 2048
            or final_eeprom[:64] != initial_eeprom[:64]
            or len(accepts) != 1 or accepts[0].get('copied') != expected
            or summary.get('startup_reasons') != ['0a']*5 or summary.get('releases') != 2
            or summary.get('native_illegal_observed') is not False
            or any(row.get('startup_reason') != '0a' for row in summary.get('late_samples',[]))
            or len(exchanges) != 1 or exchanges[0].get('decoded') != expected
            or exchanges[0].get('private_markers') != ['54c2','54c2']):
        raise ValueError('diagnostic record acceptance/restart/storage boundary changed; inspect before re-banking')

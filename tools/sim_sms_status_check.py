"""Correlate laboratory A/5551234 submission with firmware-owned EF_SMSS."""
import re


def verify(text, storage, *, validity_period):
    if not isinstance(validity_period, int) or not 0 <= validity_period <= 255:
        raise ValueError('relative TP-VP must be one octet')
    # The product checkers establish the composer and CP/RP/RR lifecycle.
    # This exact fixture grammar only extracts its handset-owned TP-MR;
    # it is not a general-purpose SMS parser or a fixed initial-reference rule.
    submit = re.search(r'GSM service uplink sapi=3 pd=09 message=01 length=27 '
                       r'data=390118000100069121436587090d11(?P<mr>[0-9a-f]{2})'
                       rf'0781551532f40000{validity_period:02x}0141\b', text)
    if not submit:
        raise ValueError('missing own SMS-SUBMIT reference')
    read = text.find('read-binary fid=6f43 offset=0 length=2')
    update = text.find('update-binary fid=6f43 offset=0 length=2')
    if read < 0 or read > submit.start() or update < submit.end():
        raise ValueError('SMS status was not read before submission and written afterward')
    reference = int(submit['mr'], 16)
    if len(storage) not in (3562, 3570) or storage[3560:3562] != bytes((reference, 0xff)):
        raise ValueError('retained SMSS differs from submitted TP-MR/capacity state')
    return reference

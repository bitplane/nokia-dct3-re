"""Own NPE-3 physical A/5551234 SMS submission and CP/RP/RR closure."""
import re
from tools.radio_call_lifecycle_common import require_ordered


def verify(text, *, rejected=False):
    if '[LUA ERROR]' in text:
        raise ValueError('fixture error')
    # TP-MR is maintained by the handset. NPE-3 selects relative TP-VP ff
    # (63 weeks), unlike the a7 fixture used by some sibling products.
    require_ordered(text, (
        ('physical A', re.compile(r'6210_sms_send_physical: action=text_A')),
        ('physical Send', re.compile(r'6210_sms_send_physical: action=confirm_send')),
        ('Send decode', re.compile(r'6210_keypad_decoded: key=19\b')),
        ('own SMS-SUBMIT', re.compile(r'GSM service uplink sapi=3 pd=09 message=01 length=27 data=390118000100069121436587090d11[0-9a-f]{2}0781551532f40000ff0141\b')),
        ('accepted submission', re.compile(r'gsm_sms_submit: cp=39 rp=01 smsc=1234567890 destination=5551234 alphabet=0 user_length=1 outcome=0 status_report=0')),
        ('network CP-ACK', re.compile(r'GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2')),
        ('network RP result', re.compile(r'GSM service downlink kind=19 sapi=3 pd=09 message=01 length=7'
                                        if rejected else r'GSM service downlink kind=18 sapi=3 pd=09 message=01 length=5')),
        ('handset CP-ACK', re.compile(r'GSM service uplink sapi=3 pd=09 message=04 length=2 data=3904')),
        ('RR release', re.compile(r'LAPDm service Channel Release acknowledged')),
        ('return to paging', re.compile(r'PCH no-identity fill')),
    ), '6210 outgoing SMS')
    if text.count('gsm_sms_submit:') != 1:
        raise ValueError('expected one accepted SMS submission')
    if rejected and 'GSM service downlink kind=18 sapi=3' in text:
        raise ValueError('success RP ACK appeared during rejection')


def verify_status(text, storage):
    submit = re.search(r'GSM service uplink sapi=3 pd=09 message=01 length=27 '
                       r'data=390118000100069121436587090d11(?P<mr>[0-9a-f]{2})'
                       r'0781551532f40000ff0141\b', text)
    if not submit:
        raise ValueError('missing own SMS-SUBMIT reference')
    read = text.find('read-binary fid=6f43 offset=0 length=2')
    update = text.find('update-binary fid=6f43 offset=0 length=2')
    if read < 0 or read > submit.start() or update < submit.end():
        raise ValueError('SMS status was not read before submission and written afterward')
    reference = int(submit['mr'], 16)
    if len(storage) != 3562 or storage[3560:] != bytes((reference, 0xff)):
        raise ValueError('retained SMSS differs from submitted TP-MR/capacity state')
    return reference

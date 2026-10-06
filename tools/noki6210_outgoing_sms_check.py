"""Own NPE-3 physical A/5551234 SMS submission and CP/RP/RR closure."""
import re
from tools.radio_call_lifecycle_common import require_ordered


def verify(text):
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
        ('network RP-ACK', re.compile(r'GSM service downlink kind=18 sapi=3 pd=09 message=01 length=5')),
        ('handset CP-ACK', re.compile(r'GSM service uplink sapi=3 pd=09 message=04 length=2 data=3904')),
        ('RR release', re.compile(r'LAPDm service Channel Release acknowledged')),
        ('return to paging', re.compile(r'PCH no-identity fill')),
    ), '6210 outgoing SMS')
    if text.count('gsm_sms_submit:') != 1:
        raise ValueError('expected one accepted SMS submission')

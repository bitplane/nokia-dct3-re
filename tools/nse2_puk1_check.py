"""Check nonterminal physical PUK1 failure and byte-identical cold continuation."""
import hashlib
import re

from tools.nse2_sim_check import HEADER, PIN_BLOCKED_FRAME, PUK_PROMPT_FRAME, verify_pin_input
from tools.sim_security_trace_check import ATTEMPTS_OFFSET, statuses
REJECTED_FRAME='5bb12516ae50295115e1026e3ec8de760e952c95c7b38a580c3940188fb93c5a'


def input_remaining(card,*,permanent=False):
    if len(card)!=3570:
        raise ValueError('complete PIN1-blocked card required')
    remaining=card[ATTEMPTS_OFFSET+2]
    if remaining not in ((0,) if permanent else range(1,11)):
        raise ValueError('PUK1 input retry domain differs')
    normalized=bytearray(card)
    normalized[ATTEMPTS_OFFSET+2]=10
    verify_pin_input(normalized,blocked=True)
    return remaining


def check_wrong(text,before,after,prompt_pixels,result_pixels):
    remaining=input_remaining(before)
    expected=bytearray(before)
    expected[ATTEMPTS_OFFSET+2]-=1
    if after!=expected:
        raise ValueError('wrong PUK1 changed credentials or unrelated storage')
    headers=[m.groups() for m in HEADER.finditer(text) if int(m[2],16) in (0x20,0x24,0x26,0x28,0x2c,0x04,0x44,0xdc,0xd6)]
    if len(headers)!=1 or headers[0][:6]!=('a0','2c','00','00','10','6fad') or not 18.2<float(headers[0][-1])<18.5:
        raise ValueError('wrong PUK1 requires one physical absolute CHV1 UNBLOCK')
    if statuses(text,0x2c)!=[(0x9840 if remaining==1 else 0x9804,0,remaining-1)]:
        raise ValueError('wrong PUK1 status/retry result differs')
    check_contacts(text)
    final_frame=REJECTED_FRAME if remaining==1 else PUK_PROMPT_FRAME
    if hashlib.sha256(prompt_pixels).hexdigest()!=PUK_PROMPT_FRAME or hashlib.sha256(result_pixels).hexdigest()!=final_frame:
        raise ValueError('wrong PUK1 prompt/result frame differs')
    return dict(wrong_puk1_rejected=True,puk1_attempts_remaining=remaining-1,
                pin1_unblocked=False,native_registration_verified=False)


def check_contacts(text):
    edges=re.findall(r'nse2_puk_contact: event=(\d+) down=([01]) t=([0-9.]+) physical_only=1',text)
    starts=[11.2+i*.3 for i in range(8)]+[13.8]+[14.4+i*.35 for i in range(4)]+[15.9]+[16.6+i*.35 for i in range(4)]+[18.2]
    if len(edges)!=38:
        raise ValueError('wrong PUK1 physical contact count differs')
    for i,(event,down,when) in enumerate(edges):
        if int(event)!=i+1 or int(down)!=1-i%2 or abs(float(when)-(starts[i//2]+i%2*.15))>.04:
            raise ValueError('wrong PUK1 physical contact timing/order differs')


def check_permanent(text,before,after,before_pixels,after_pixels):
    input_remaining(before,permanent=True)
    if before!=after:
        raise ValueError('permanent PUK1 cold boot changed card storage')
    if any(int(m[2],16) in (0x20,0x24,0x26,0x28,0x2c,0x04,0x44,0xdc,0xd6) for m in HEADER.finditer(text)):
        raise ValueError('permanent PUK1 refusal admitted a security/write command')
    check_contacts(text)
    if any(hashlib.sha256(pixels).hexdigest()!=REJECTED_FRAME for pixels in (before_pixels,after_pixels)):
        raise ValueError('permanent PUK1 rejection frame differs')
    return dict(permanent_puk1_loss=True,cold_card_unchanged=True,credential_input_refused=True,
                native_registration_verified=False)


def check_cold(text,before,after,blocked_pixels,prompt_pixels):
    remaining=input_remaining(before)
    if after!=before:
        raise ValueError('cold blocked-card boot changed persistent storage')
    if any(int(m[2],16) in (0x20,0x24,0x26,0x28,0x2c,0x04,0x44,0xdc,0xd6) for m in HEADER.finditer(text)):
        raise ValueError('cold PUK wait issued a security/write command')
    if 'nse2_puk_contact:' in text or 'nse2_pin_contact:' in text:
        raise ValueError('cold PUK wait submitted physical credentials')
    if hashlib.sha256(blocked_pixels).hexdigest()!=PIN_BLOCKED_FRAME or hashlib.sha256(prompt_pixels).hexdigest()!=PUK_PROMPT_FRAME:
        raise ValueError('cold blocked notice/PUK prompt differs')
    return dict(puk1_attempts_remaining=remaining,cold_card_unchanged=True)

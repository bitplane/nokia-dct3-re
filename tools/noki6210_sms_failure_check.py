"""NPE-3 host rejection and physical recovery; no native DSP claim."""
import re
from tools.noki6210_outgoing_sms_check import verify as verify_submission
from tools.noki6250_sms_failure_check import check_frames
from tools.radio_call_lifecycle_common import require_ordered


def verify(text, frames=None):
    verify_submission(text, rejected=True)
    patterns = (
        ('host wrong ID rejected', r'gsm_call_adapter: sms decision id=2 outcome=1 result=rejected'),
        ('host error accepted', r'gsm_call_adapter: sms decision id=1 outcome=1 result=accepted'),
        ('host duplicate rejected', r'gsm_call_adapter: sms decision id=1 outcome=1 result=rejected'),
        ('RP error', r'GSM service downlink kind=19 sapi=3 pd=09 message=01 length=7'),
        ('release', r'LAPDm service Channel Release acknowledged'),
        ('physical End', r'6210_sms_recovery_physical: key=End'),
        ('End decode', r'6210_keypad_decoded: key=0f\b'),
        ('second End', r'6210_sms_recovery_physical: key=End'),
        ('second End decode', r'6210_keypad_decoded: key=0f\b'),
        ('physical Menu', r'6210_sms_recovery_physical: key=Left Softkey / Menu'),
        ('Menu decode', r'6210_keypad_decoded: key=19\b'),
    )
    require_ordered(text, tuple((name, re.compile(pattern)) for name, pattern in patterns), '6210 rejection')
    if frames is not None:
        # Independently reviewed NPE-3 has the same 96x60 text geometry.
        check_frames(frames, product='6210')

"""NPE-3 host rejection and physical recovery; no native DSP claim."""
import re
from tools.noki6210_outgoing_sms_check import verify as verify_submission
from tools.noki6250_sms_failure_check import check_frames
from tools.radio_call_lifecycle_common import require_ordered
from tools.noki8850_outgoing_sms_check import verify_silence


def verify(text, frames=None, *, rp_silence=False):
    if rp_silence:
        # This shared lifecycle checker validates the exact A payload;
        # NPE-3's independently decoded TP-VP is ff, not its default a7.
        verify_silence(text, product='6210',
                       submit='390118000100069121436587090d11010781551532f40000ff0141')
    else:
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
    if rp_silence:
        patterns = (('release', r'TX packet type=02 .*radio_phase=release_channel_change'),) + patterns[5:]
    require_ordered(text, tuple((name, re.compile(pattern)) for name, pattern in patterns), '6210 SMS failure')
    if frames is not None:
        # Independently reviewed NPE-3 has the same 96x60 text geometry.
        check_frames(frames, product='6210', rp_silence=rp_silence)

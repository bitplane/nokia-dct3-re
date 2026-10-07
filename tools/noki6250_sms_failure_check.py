"""Verify NHM-3 host RP rejection and physical recovery, not native DSP."""
import argparse
import hashlib
from pathlib import Path
import re
import sys
from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.radio_call_lifecycle_common import require_ordered


def verify(text, *, rp_silence=False):
    patterns = (
        ('physical Send', r'6250_sms_input: step=25 pressed=1'),
        ('exact Hi submission', r'GSM service uplink sapi=3 pd=09 message=01 length=28 data=390119000100069121436587090e11010781551532f40000ff02c834\b'),
        ('wrong request rejected', r'gsm_call_adapter: sms decision id=2 outcome=1 result=rejected'),
        ('RP error accepted', r'gsm_call_adapter: sms decision id=1 outcome=1 result=accepted'),
        ('duplicate rejected', r'gsm_call_adapter: sms decision id=1 outcome=1 result=rejected'),
        ('network RP error', r'GSM service downlink kind=19 sapi=3 pd=09 message=01 length=7'),
        ('handset CP ACK', r'GSM service uplink sapi=3 pd=09 message=04 length=2 data=3904'),
        ('channel release', r'LAPDm service Channel Release acknowledged'),
        ('physical End', r'6250_sms_recovery_physical: key=End'),
        ('End scan', r'6250_raw_matrix_key: value=0f\b'),
        ('second End', r'6250_sms_recovery_physical: key=End'),
        ('second End scan', r'6250_raw_matrix_key: value=0f\b'),
        ('physical Menu', r'6250_sms_recovery_physical: key=Left Softkey / Menu'),
        ('Menu scan', r'6250_raw_matrix_key: value=06\b'),
    )
    if rp_silence:
        patterns = patterns[:2] + (
            ('network CP ACK', r'GSM service downlink kind=17 sapi=3 pd=09 message=04'),
            ('host silence accepted', r'gsm_call_adapter: sms decision id=1 outcome=3 result=accepted'),
            ('mobile main-link DISC', r'TX packet type=1b .*data=0080015301'),
            ('network UA', r'RX enqueue type=80 .*data=80[0-9a-f]{18}017301'),
            ('deconfiguration', r'TX packet type=02 .*radio_phase=release_channel_change'),
            ('correlated host end', r'gsm_call_adapter: sms state id=1 epoch=1 phase=ended'),
            ('resumed paging', r'PCH no-identity fill'),
        ) + patterns[8:]
    require_ordered(text, tuple((name, re.compile(pattern)) for name, pattern in patterns), '6250')
    if text.count('gsm_sms_submit:') != 1:
        raise ValueError('expected one outgoing submission')
    if 'GSM service downlink kind=18 sapi=3' in text:
        raise ValueError('success RP ACK appeared during rejection')
    if rp_silence and 'GSM service downlink kind=19 sapi=3' in text:
        raise ValueError('RP error appeared during silence')


def check_frames(directory, *, rp_silence=False, product='6250'):
    def digest(path, crop):
        with Image.open(path) as image:
            if image.size != (96, 60):
                raise ValueError('unexpected handset geometry')
            return hashlib.sha256(image.convert('L').crop(crop).tobytes()).hexdigest()
    # Exclude the animated result icon to the right of the failure text.
    failure_hash = ('d3600f56eb6f1027a571d0a17681f9aab9975f5f4a51bf1ab91076a17553a1fd' if rp_silence else
                    'a12e293440c8b5e0bb81df8ae96602d710e200042c720594ae664f93658b5a14')
    if not any(digest(path, (0, 0, 72, 60)) == failure_hash
               for path in directory.glob(product + '_sms_reject_*.png')):
        raise ValueError('missing reviewed SMS failure presentation')
    if digest(directory / (product + '_sms_recovery_menu.png'), (0, 0, 84, 16)) != (
            '91f4829ae667137ccc27fa2e3a266161d4b6518d2ed6f10470abf7de6ce7e372'):
        raise ValueError('missing reviewed Messages recovery menu')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('frames', type=Path)
    parser.add_argument('--rp-silence', action='store_true')
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors='replace'), rp_silence=args.rp_silence)
        check_frames(args.frames, rp_silence=args.rp_silence)
    except (ValueError, OSError) as error:
        parser.exit(1, f'6250 SMS rejection FAIL: {error}\n')
    print('6250 host RP ' + ('silence' if args.rp_silence else 'rejection') +
          ' and physical recovery PASS; native DSP unproved')

"""Verify the observed NSB-6 laboratory registration and release contract."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.radio_call_lifecycle_common import require_ordered


CHECKPOINTS = (
    ('own candidate window', r'TX packet type=56 payload=160 .*data=003c'),
    ('candidate channel', r'TX packet type=02 payload=20 .*radio_phase=candidate_channel_change data=04000000000000505000003c'),
    ('random access', r'TX packet type=0c .*radio_phase=random_access'),
    ('assigned confirmation', r'RX enqueue type=89 payload=8 .*data=0100000000000000'),
    ('own Location Updating Request', r'TX packet type=1b .*data=0080013f4905087000f000fffe23080910101032547698'),
    ('correlated contention UA', r'RX enqueue type=80 payload=34 .*data=80[0-9a-f]{18}01734905087000f000fffe23080910101032547698'),
    ('accept acknowledgement', r'LAPDm Location Updating Accept acknowledged nr=1'),
    ('handset accept RR', r'TX packet type=1b .*data=0080032101'),
    ('release acknowledgement', r'LAPDm Channel Release acknowledged nr=2'),
    ('handset release RR', r'TX packet type=1b .*data=0080034101'),
    ('stored LAI', r'sim_device: update-binary fid=6f7e offset=4 length=5'),
    ('stored status', r'sim_device: update-binary fid=6f7e offset=10 length=1'),
    ('own deconfiguration', r'TX packet type=02 payload=20 .*radio_phase=release_channel_change data=040000000000001a6000003c0000000f00000000'),
    ('idle confirmation', r'RX enqueue type=89 payload=8 .*data=0000000000000000'),
    ('post-release paging', r'RX enqueue type=80 payload=34 .*data=60[0-9a-f]{18}1506210001f0'),
)


def verify(text, *, pcs1900=False, configured_gsm900=False, preserved_location=False):
    if '[LUA ERROR]' in text:
        raise ValueError('fixture error')
    checkpoints = list(CHECKPOINTS)
    if pcs1900 and configured_gsm900:
        raise ValueError('PCS1900 and configured GSM900 are distinct compositions')
    if preserved_location:
        if pcs1900:
            raise ValueError('preserved-location acceptance is not established for PCS1900')
        # The restarted own handset sends 72 and its retained LAI, not the
        # cold unlocated 70/00f000fffe request. Keep both exact grammars.
        checkpoints = [(label, pattern.replace('05087000f000fffe23', '05087200f110000123'))
                       for label, pattern in checkpoints]
    if configured_gsm900:
        checkpoints[1] = ('configured GSM900 candidate channel',
            r'TX packet type=02 payload=20 .*radio_phase=candidate_channel_change data=04120200000000505000003c')
        checkpoints[12] = ('configured GSM900 deconfiguration',
            r'TX packet type=02 payload=20 .*radio_phase=release_channel_change data=041202000000001a6000003c0000000f00000000')
        checkpoints.insert(1, ('configured carrier SCH',
            r'RX enqueue type=80 payload=14 .*data=4012[0-9a-f]{8}003c000048'))
    if pcs1900:
        checkpoints[1] = ('PCS candidate channel',
            r'TX packet type=02 payload=20 .*radio_phase=candidate_channel_change data=041202000000005050000258')
        checkpoints[12] = ('PCS deconfiguration',
            r'TX packet type=02 payload=20 .*radio_phase=release_channel_change data=041202000000001a600002580000000f00000000')
        # PCS changes the handset's power-class field from 23 to 20.
        checkpoints = [(label, pattern.replace('fffe230809', 'fffe200809'))
                       for label, pattern in checkpoints]
        checkpoints[1:1] = [
            ('GSM scan request', r'TX packet type=55 payload=4 .*data=01140000'),
            ('PCS scan request', r'TX packet type=55 payload=4 .*data=04080000'),
            ('PCS measurements', r'RX enqueue type=8b .*data=0010025800c3025900b9'),
            ('firmware PCS candidate window', r'TX packet type=56 payload=160 .*data=02580259'),
        ]
        checkpoints.insert(6, ('PCS SI1 band indicator',
            r'RX enqueue type=80 payload=34 .*data=5012[0-9a-f]{8}02580000590619[0-9a-f]{38}6b00'))
    if preserved_location:
        checkpoints = [(label, pattern) for label, pattern in checkpoints
                       if label not in ('stored LAI', 'stored status')]
        stored_read = re.search(r'sim_device: read-binary fid=6f7e offset=0 length=11\b', text)
        request = re.search(r'TX packet type=1b .*data=0080013f49050872', text)
        if not stored_read or not request or stored_read.start() >= request.start():
            raise ValueError('preserved location was not read before registration')
        if 'sim_device: update-binary fid=6f7e' in text:
            raise ValueError('preserved location was unexpectedly rewritten')
    require_ordered(text, tuple((label, re.compile(pattern)) for label, pattern in checkpoints),
                    '8890 registration')
    if len(re.findall(r'TX packet type=1b .*data=0080013f490508', text)) != 1:
        raise ValueError('expected one handset Location Updating Request')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--pcs1900', action='store_true',
                        help='require independent PCS scan, ARFCN 600 and SI1 band indication')
    parser.add_argument('--configured-gsm900', action='store_true',
                        help='require configured ARFCN60 SCH and recovered channel parameters')
    parser.add_argument('--preserved-location', action='store_true',
                        help='require the separately observed retained-LAI request on configured GSM900')
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors='replace'), pcs1900=args.pcs1900,
               configured_gsm900=args.configured_gsm900, preserved_location=args.preserved_location)
    except (OSError, ValueError) as error:
        parser.exit(1, f'8890 registration FAIL: {error}\n')
    print('8890 laboratory registration/EF_LOCI/release/paging PASS; calls unproved')

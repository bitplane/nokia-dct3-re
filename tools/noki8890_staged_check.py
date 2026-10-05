"""Verify NSB-6 native uploads and the fail-closed missing-mask boundary."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsm3d_catalogue import catalogue
from tools.radio_call_lifecycle_common import require_ordered


def verify(text, runtime=False, selftest=False):
    require_ordered(text, tuple((name, re.compile(pattern)) for name, pattern in (
        ('native verifier', r'release entry=0f00 words=223 prom_input=0006 clock=13000000 stage=verifier'),
        ('native publication', r'publication word0=0000 word1=0006 word2=0006 word3=0006'),
        ('native loader', r'release entry=0f00 words=126 prom_input=0006 clock=13000000 stage=loader'),
        ('own second-loader request', r'request selector=0014 ack=0000'),
        ('verified second loader', r'loader2_verified words=613 entry=0a00'),
        ('missing resident call', r'outside_uploaded_code pc=2c75'),
        ('ownership boundary', r'runtime_hle_handoff pc=2c75 native_suspended=1' if runtime
         else r'observation_halt pc=2c75 ownership_retained=1'),
    )), '8890 staged boundary')
    if len(re.findall(r'request selector=0001\b', text)) != 118:
        raise ValueError('expected 118 product-local chunk requests')
    if '[LUA ERROR]' in text or (not runtime and 'runtime_hle_handoff' in text):
        raise ValueError('observer error or unexpected native ownership handoff')
    if runtime:
        require_ordered(text, tuple((name, re.compile(pattern)) for name, pattern in (
            ('own parameter commit', r'8890_service_encoder: command=0032 argument=3fff commit=0001'),
            ('discovery request', r'TX pending type=05 payload=10 data=1eff00d000030101e000'),
            ('discovery echo', r'RX enqueue type=8e payload=10 producer=[0-9a-f]+ data=1e0002d000030101e000'),
            ('discovery response', r'RX enqueue type=8e payload=10 producer=[0-9a-f]+ data=1e0002d000030401c100'),
            ('firmware discovery acknowledgement', r'TX pending type=05 payload=10 data=1e0200d0000305014100'),
            ('identity request', r'TX pending type=70 payload=6 data=1304eca05beb'),
            ('record request', r'TX pending type=70 payload=26 data=16184bc0613636443a235d792fd4ba7e6b71defabf9b19e00f43'),
            ('self-test request', r'TX pending type=70 payload=2 data=0d00'),
        )), '8890 runtime request boundary')
    if selftest:
        if not runtime:
            raise ValueError('self-test acceptance requires explicit runtime HLE')
        require_ordered(text, tuple((name, re.compile(pattern)) for name, pattern in (
            ('self-test request', r'TX pending type=70 payload=2 data=0d00'),
            ('request-correlated reply', r'RX enqueue type=74 payload=2 producer=[0-9a-f]+ data=0d00'),
            ('own armed consumer', r'8890_selftest_reply: command=0d faults=00 flag=c4'),
            ('own fault update', r'8890_service_return: command=0d faults=00/00/00'),
        )), '8890 compact self-test')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('flash', type=Path)
    parser.add_argument('--runtime', action='store_true', help='check explicit research HLE handoff and request-derived discovery')
    parser.add_argument('--selftest', action='store_true', help='also require the own compact self-test consumer')
    args = parser.parse_args()
    try:
        entries = catalogue(args.flash.read_bytes(), '8890')
        if entries[20]['sha1'] != '7fc1c5a9435664f15b7064de1cf129f764ab21ac':
            raise ValueError('wrong own second loader')
        verify(args.log.read_text(errors='replace'), args.runtime, args.selftest)
    except (OSError, ValueError) as error:
        parser.exit(1, f'8890 staged FAIL: {error}\n')
    print('8890 compact self-test PASS; identity/record replies and UI unproved' if args.selftest else
          '8890 runtime request boundary PASS; identity/record replies unproved' if args.runtime
          else '8890 native verifier/loaders PASS; absent routine 2c75 remains fail-closed')


if __name__ == '__main__':
    main()

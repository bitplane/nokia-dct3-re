"""Verify NSB-6 native uploads and the fail-closed missing-mask boundary."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsm3d_catalogue import catalogue
from tools.radio_call_lifecycle_common import require_ordered


def verify(text):
    require_ordered(text, tuple((name, re.compile(pattern)) for name, pattern in (
        ('native verifier', r'release entry=0f00 words=223 prom_input=0006 clock=13000000 stage=verifier'),
        ('native publication', r'publication word0=0000 word1=0006 word2=0006 word3=0006'),
        ('native loader', r'release entry=0f00 words=126 prom_input=0006 clock=13000000 stage=loader'),
        ('own second-loader request', r'request selector=0014 ack=0000'),
        ('verified second loader', r'loader2_verified words=613 entry=0a00'),
        ('missing resident call', r'outside_uploaded_code pc=2c75'),
        ('retained ownership', r'observation_halt pc=2c75 ownership_retained=1'),
    )), '8890 staged boundary')
    if len(re.findall(r'request selector=0001\b', text)) != 118:
        raise ValueError('expected 118 product-local chunk requests')
    if '[LUA ERROR]' in text or 'runtime_hle_handoff' in text:
        raise ValueError('observer error or unexpected native ownership handoff')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('flash', type=Path)
    args = parser.parse_args()
    try:
        entries = catalogue(args.flash.read_bytes(), '8890')
        if entries[20]['sha1'] != '7fc1c5a9435664f15b7064de1cf129f764ab21ac':
            raise ValueError('wrong own second loader')
        verify(args.log.read_text(errors='replace'))
    except (OSError, ValueError) as error:
        parser.exit(1, f'8890 staged FAIL: {error}\n')
    print('8890 native verifier/loaders PASS; absent routine 2c75 remains fail-closed')


if __name__ == '__main__':
    main()

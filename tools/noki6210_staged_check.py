"""Check NPE-3's own upload execution and explicit ownership boundary."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.radio_call_lifecycle_common import require_ordered


def verify(text, runtime=False, selftest=False):
    patterns = (
        ('native verifier', 'release entry=0f00 words=223 prom_input=0006 clock=13000000 stage=verifier'),
        ('native publication', 'publication word0=0000 word1=0006 word2=0006 word3=0006'),
        ('own MCU consumption', '6210_verifier_result: result0=0000 result1=0006'),
        ('native loader', 'release entry=0f00 words=126 prom_input=0006 clock=13000000 stage=loader'),
        ('loader request', 'request selector=0014 ack=0000'),
        ('own second loader', 'loader2_verified words=629 entry=0a00'),
        ('installed program', 'installed_program words=422 first=0590 last=0735'),
        ('missing mask routine', 'outside_uploaded_code pc=2c75'),
        ('ownership boundary', 'runtime_hle_handoff pc=2c75 native_suspended=1' if runtime
         else 'observation_halt pc=2c75 ownership_retained=1'),
    )
    require_ordered(text, tuple((name, re.compile(re.escape(value))) for name, value in patterns),
                    '6210 native upload')
    if len(re.findall(r'request selector=0001\b', text)) != 156:
        raise ValueError('expected 156 own selector-1 requests')
    if len(re.findall(r'request selector=0014\b', text)) != 1:
        raise ValueError('expected one own second-loader request')
    if '[LUA ERROR]' in text or (not runtime and 'runtime_hle_handoff' in text):
        raise ValueError('observer error or unintended runtime ownership')
    if runtime:
        require_ordered(text, tuple((name, re.compile(re.escape(value))) for name, value in (
            ('own discovery', 'TX pending type=05 payload=10 data=1eff00d000030101e000'),
            ('peer response', 'data=1e0002d000030401c100'),
            ('MCU acknowledgement', 'TX pending type=05 payload=10 data=1e0200d0000305014100'),
        )), '6210 runtime discovery')
    if selftest:
        if not runtime:
            raise ValueError('self-test acceptance requires explicit runtime HLE')
        require_ordered(text, tuple((name, re.compile(re.escape(value))) for name, value in (
            ('self-test request', 'TX pending type=70 payload=2 data=0d00'),
            ('own armed consumer', '6210_selftest_reply: command=0d faults=00 flag=c4'),
            ('own fault fields', '6210_service_return: command=0d faults=00/00/00'),
            ('accepted nominal analogue samples', '6210_analog_window: accepted=01'),
        )), '6210 compact peer and nominal battery')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--runtime', action='store_true')
    parser.add_argument('--selftest', action='store_true')
    args = parser.parse_args()
    try:
        with args.log.open(errors='replace') as stream:
            text = ''.join(line for line in stream if 'staged_dsp:' in line
                           or '6210_' in line or 'dspif_transport:' in line or '[LUA ERROR]' in line)
        verify(text, args.runtime, args.selftest)
    except (OSError, ValueError) as error:
        parser.exit(1, f'6210 staged FAIL: {error}\n')
    print('6210 own uploads PASS; fragment-derived inputs are not measured silicon')


if __name__ == '__main__':
    main()

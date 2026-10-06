"""Check the product-local 8210 native upload and missing-resident boundary."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsm3d_catalogue import catalogue
from tools.radio_call_lifecycle_common import require_ordered


def verify(text, runtime=False, selftest=False, base_record=False):
    patterns = (
        ('own verifier descriptor', '8210_verifier_descriptor: pointer=0031bcf0'),
        ('native verifier', 'release entry=0f00 words=223 prom_input=0006 clock=13000000 stage=verifier'),
        ('native publication', 'publication word0=0000 word1=0006 word2=0006 word3=0006'),
        ('MCU consumption', '8210_verifier_result: result0=0000 result1=0006'),
        ('native loader', 'release entry=0f00 words=126 prom_input=0006 clock=13000000 stage=loader'),
        ('second-loader request', 'request selector=0014 ack=0000'),
        ('own second loader', 'loader2_verified words=623 entry=0a00'),
        ('installed program', 'installed_program words=422 first=0590 last=0735'),
        ('missing resident code', 'outside_uploaded_code pc=2c75'),
        ('ownership boundary', 'runtime_hle_handoff pc=2c75 native_suspended=1' if runtime
         else 'observation_halt pc=2c75 ownership_retained=1'),
    )
    require_ordered(text, tuple((name, re.compile(re.escape(pattern)))
                              for name, pattern in patterns), '8210 native boundary')
    if len(re.findall(r'request selector=0001\b', text)) != 133:
        raise ValueError('expected 133 product-local selector-1 requests')
    if len(re.findall(r'request selector=0014\b', text)) != 1:
        raise ValueError('expected one second-loader request')
    if '[LUA ERROR]' in text or (not runtime and 'runtime_hle_handoff' in text):
        raise ValueError('observer error or unexpected ownership handoff')
    if runtime:
        require_ordered(text, tuple((name, re.compile(re.escape(pattern)))
                                  for name, pattern in (
            ('own discovery request', 'TX pending type=05 payload=10 data=1eff00d000030101e000'),
            ('discovery response', 'data=1e0002d000030401c100'),
            ('firmware discovery acknowledgement', 'TX pending type=05 payload=10 data=1e0200d0000305014100'),
        )), '8210 runtime discovery')
    if selftest:
        if not runtime:
            raise ValueError('self-test acceptance requires runtime HLE')
        require_ordered(text, tuple((name, re.compile(re.escape(pattern)))
                                  for name, pattern in (
            ('own self-test request', 'TX pending type=70 payload=2 data=0d00'),
            ('own armed consumer', '8210_selftest_reply: command=0d faults=00 flag=' +
             ('c4' if base_record else '84')),
            ('own cleared faults', '8210_service_return: command=0d faults=00/00/00'),
        )), '8210 compact self-test')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('flash', type=Path)
    parser.add_argument('--runtime', action='store_true')
    parser.add_argument('--selftest', action='store_true')
    parser.add_argument('--base-record', action='store_true',
                        help='expect the labelled checksum-valid base-record fixture')
    args = parser.parse_args()
    try:
        entries = catalogue(args.flash.read_bytes(), '8210')
        if entries[20]['sha1'] != '8e9e4aefa311375ae090b90a607f00cb8e7059ca':
            raise ValueError('wrong product-local second loader')
        # Discard verbose instruction traces before matching the contract.
        with args.log.open(errors='replace') as stream:
            text = ''.join(line for line in stream if 'staged_dsp:' in line
                           or '8210_verifier_' in line or '[LUA ERROR]' in line
                           or '8210_selftest_' in line or '8210_service_return:' in line
                           or 'dspif_transport:' in line)
        verify(text, args.runtime, args.selftest, args.base_record)
    except (OSError, ValueError) as error:
        parser.exit(1, f'8210 staged FAIL: {error}\n')
    print('8210 compact self-test PASS; UI and native resident execution unproved' if args.selftest
          else '8210 runtime discovery PASS; native resident execution unproved' if args.runtime
          else '8210 native uploads PASS; absent resident 2c75 remains fail-closed')


if __name__ == '__main__':
    main()

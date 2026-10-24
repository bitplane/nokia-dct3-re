import unittest

from tools.nsb3_toolkit_envelope_check import check_transport


def transport_trace():
    words = ['00000000'] * 16
    words[4], words[6] = '00280869', '00280c3d'
    return '\n'.join((
        'nsb3_toolkit_envelope_tx: pc=002a16a6 lr=002a1661 sp=0010f7a8 stack=' + ','.join(words) + ' observer_writes=0',
        'nsb3_toolkit_transfer: stage=return source=0010832d header=a0c20000 result=07 observer_writes=0',
        'nsb3_toolkit_transfer: stage=return source=00108332 header=d3070202 result=07 observer_writes=0'))


class Nsb3EnvelopeBoundaryTest(unittest.TestCase):
    def test_header_and_body_complete_on_the_observed_caller_stack(self):
        check_transport(transport_trace())

    def test_wrong_status_missing_body_or_caller_context_fails(self):
        good = transport_trace()
        for bad in (good.replace('result=07', 'result=08', 1),
                    good.replace('header=d3070202', 'header=00000000'),
                    good.replace('00280869', '0028086b'),
                    good.replace('pc=002a16a6', 'pc=002a16a8'),
                    good.replace('sp=0010f7a8', 'sp=00200000')):
            with self.assertRaises(ValueError):
                check_transport(bad)


if __name__ == '__main__':
    unittest.main()

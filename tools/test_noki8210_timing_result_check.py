import unittest

from tools.noki8210_timing_result_check import verify


def fixture():
    lines = [
        'dsp_hle: directed timing failure requested arfcn=824 mode=4 t=12.262665',
        'dsp_hle: TX packet type=11 payload=12 data=040000000000033800000000',
        'dspif_transport: RX enqueue type=88 payload=10 data=01000000033800000001',
        'dsp_hle: directed timing failure published arfcn=824 mode=4 status=1',
    ]
    for site, record, carrier, control, errors, time in (
            ('dispatch', '00119690', '0337', '00c5c60c', '00', '12.263048'),
            ('receiver', '00119690', '0337', '00c5c60c', '00', '12.263055'),
            ('update', '001196a0', '0338', 'ffc22664', '00', '12.263085'),
            ('returned', '001196a0', '0338', 'ffc12664', '01', '12.263111')):
        lines.append(f'8210_timing_result: site={site} ordinal=1 message=0011327c error=01 target=0338 record={record} carrier={carrier} control={control} errors={errors} t={time} observer_writes=0')
    lines += ['LAPDm Location Updating Accept acknowledged nr=1',
              'neighbour measurement instruction arfcn=824 bsic=0 validated=0 accepted=1']
    return '\n'.join(lines)


class TimingResultTests(unittest.TestCase):
    def test_failure_is_not_native_success(self):
        report = verify(fixture())
        self.assertEqual(report['firmware_selector'], [2, 1])
        self.assertFalse(report['native_dsp_claim'])
        self.assertFalse(report['physical_rf_claim'])

    def test_missing_and_duplicated_reply_rejected(self):
        text = fixture()
        for bad in (text.replace('RX enqueue type=88', 'ignored'),
                    text + '\nRX enqueue type=88 payload=10',
                    text.replace('site=receiver', 'site=dispatch')):
            with self.assertRaises(ValueError):
                verify(bad)

    def test_forced_or_successful_result_rejected(self):
        text = fixture()
        for bad in (text.replace('observer_writes=0', 'observer_writes=1'),
                    text.replace('error=01', 'error=00'),
                    text.replace('control=ffc12664', 'control=ffc32664'),
                    text.replace('control=ffc12664', 'control=00c12664')):
            with self.assertRaises(ValueError):
                verify(bad)

    def test_identity_target_and_counter_required(self):
        text = fixture()
        for bad in (text.replace('target=0338', 'target=0038'),
                    text.replace('errors=01', 'errors=00'),
                    text.replace('record=001196a0', 'record=00119690'),
                    text.replace('site=returned ordinal=1 message=0011327c',
                                 'site=returned ordinal=1 message=00113280')):
            with self.assertRaises(ValueError):
                verify(bad)

    def test_registration_must_follow_result(self):
        text = fixture().replace('LAPDm Location Updating Accept acknowledged nr=1', '')
        with self.assertRaises(ValueError):
            verify('LAPDm Location Updating Accept acknowledged nr=1\n' + text)

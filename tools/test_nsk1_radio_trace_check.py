import unittest

from tools.nsk1_radio_trace_check import check, check_scan_progress, diagnose_liveness_reset


def fixture():
    lines = []
    for index, (kind, size) in enumerate([('51', 80)] * 6 + [('51', 16), ('1a', 68), ('4a', 4)], 1):
        lines.append(f'nsk1_radio_packet: direction=tx index={index} type={kind} length={size} data=' + '00' * size)
    lines.append('nsk1_radio_packet: direction=rx index=1 type=8b length=162 data=' + '00' * 162)
    lines.append('nsk1_radio_summary: tx=9 rx=1 cap=128 start=2 end=59 observer_writes=0')
    lines.extend('nsk1_radio_native_entry: pc=' + pc for pc in ('3000', '3076', '3d5e'))
    writes = [('06d0', '0000'), ('06d1', '0000')]
    for index, state in [('0001', '0002'), ('0002', '0006'), ('0008', '0106'), ('0003', '010e')]:
        writes.extend([('06d0', index), ('06d1', state)])
    lines.extend(f'nsk1_radio_native_control_write: address={address} value={value}' for address, value in writes)
    lines.append('nsk1_radio_native_rx_stack: pc=3805 sp=1e82 words=0e13,008b,00a2,4814,0a2e,30ed,38ca,3976')
    return '\n'.join(lines)


class NativeRadioTraceTest(unittest.TestCase):
    def test_live_gateway_not_resident_only_target_is_required(self):
        entries = [('3103','03ff'),('311a','03ff'),('312c','0008'),('3130','0008'),('0a71','0008')]
        rows = '\n'.join(f'nsk1_rssi_work: pc={pc} ar3={ar3} pointer1983=03ff word03ff=0374 control06be=0020 control06e3=0000' for pc,ar3 in entries)
        gateway = 'nsk1_rssi_gateway: word2420=f073 word2421=0a71 word0a5c=891a word0a71=76f8'
        text = fixture()+'\n'+rows+'\n'+gateway
        self.assertTrue(check(text)['post_rssi_channel_work_observed'])
        with self.assertRaisesRegex(ValueError, 'gateway'):
            check(text.replace('word2421=0a71', 'word2421=0a5c'))
        with self.assertRaisesRegex(ValueError, 'cursor/control'):
            check(text.replace('word03ff=0374', 'word03ff=7fff'))

    def test_post_rssi_return_is_checked_when_observed(self):
        rows = '\n'.join(f'nsk1_rssi_return: pc={pc} st0=081f st1=6103 control06be=0020 control06e1=0000 control06e3=0000 list11f9=03ff list11fa=03ff' for pc in ('0a2e','0a2f','30ed','30f0','30f2'))
        self.assertTrue(check(fixture()+'\n'+rows)['post_rssi_return_observed'])
        with self.assertRaisesRegex(ValueError, 'TC/control'):
            check(fixture()+'\n'+rows.replace('st0=081f','st0=181f',1))

    def liveness_fixture(self):
        return '\n'.join([
            'nsk1_supervisor_write: address=0010fdea value=01',
            'nsk1_supervisor_write: address=0010fdea value=00',
            'nsk1_dsp_liveness_check: count=0001',
            'nsk1_dsp_liveness_check: count=0001',
            'nsk1_dsp_liveness_check: count=0000',
            'nsk1_dsp_liveness_fault: kind=1 caller=00294191',
            'nsk1_supervisor_reason_set: reason=68 caller=0028c351',
            'nsk1_supervisor_reset_path: event=d5 reason=68',
            'nsk1_radio_reset_request: reason=00000068 caller=0022f117'])

    def test_liveness_diagnosis_is_explicitly_a_failure(self):
        result = diagnose_liveness_reset(self.liveness_fixture())
        self.assertFalse(result['long_run_stability_proved'])
        self.assertFalse(result['idle_counter_threshold_cause'])

    def test_liveness_diagnosis_requires_the_empty_window(self):
        with self.assertRaises(ValueError):
            diagnose_liveness_reset(self.liveness_fixture().replace('count=0000', 'count=0001'))

    def test_liveness_diagnosis_rejects_idle_threshold_and_wrong_caller(self):
        for old, new in [('value=01', 'value=0f'), ('caller=00294191', 'caller=00294195')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                diagnose_liveness_reset(self.liveness_fixture().replace(old, new))

    def test_qualified_boundary_is_not_registration(self):
        result = check(fixture())
        self.assertEqual([1, 2, 8, 3], result['pending_indexes'])
        self.assertFalse(result['registration_proved'])

    def test_rejects_wrong_cpu_entries(self):
        with self.assertRaisesRegex(ValueError, 'CPU scope'):
            check(fixture().replace('pc=3d5e', 'pc=28bb00'))

    def test_rejects_changed_mailbox_order(self):
        with self.assertRaisesRegex(ValueError, 'request order'):
            check(fixture().replace('address=06d0 value=0008', 'address=06d0 value=0004'))

    def test_rejects_changed_completion_and_stack(self):
        for old, new, error in [('value=0106', 'value=0116', 'progression'),
                                ('4814,0a2e', '4814,0a19', 'publisher stack')]:
            with self.subTest(old=old), self.assertRaisesRegex(ValueError, error):
                check(fixture().replace(old, new))

    def test_rejects_truncated_or_reordered_packets(self):
        with self.assertRaisesRegex(ValueError, 'payload length'):
            check(fixture().replace('length=162 data=00', 'length=162 data='))
        with self.assertRaisesRegex(ValueError, 'packet order'):
            check(fixture().replace('type=8b', 'type=8a'))


class ScanProgressTests(unittest.TestCase):
    def test_progress_is_not_registration_or_an_empty_queue_stall(self):
        text = '\n'.join(f'nsk1_scan_progress: t={40+5*i:.6f} counter_hi=ffff counter_lo={0xc296+1000*i:04x} qualified=0000 flags=0000 producer=1a00 consumer=1a00 observer_writes=0' for i in range(5))
        self.assertEqual(check_scan_progress(text), {'samples':5,'counter_advance':4000,'qualified_results_observed':False})
        for before,after in [('60.000000','61.000000'),('counter_lo=c67e','counter_lo=c296'),('qualified=0000','qualified=0001'),('consumer=1a00','consumer=1a04')]:
            with self.subTest(before=before), self.assertRaises(ValueError):
                check_scan_progress(text.replace(before,after))


if __name__ == '__main__':
    unittest.main()

import unittest

from tools.nsk1_radio_trace_check import check, diagnose_liveness_reset


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


if __name__ == '__main__':
    unittest.main()

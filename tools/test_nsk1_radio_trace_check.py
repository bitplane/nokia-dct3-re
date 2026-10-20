import unittest

from tools.nsk1_radio_trace_check import check, check_scan_progress, check_scan_qualification, check_serial_publishers, check_serial_queue, check_tuning_provenance, diagnose_liveness_reset


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
    def test_tuning_requires_own_source_before_native_upload_and_ports(self):
        text = '\n'.join([
            'nsk1_tuning_source: address=00298228 value=000417bf mask=ffffffff pc=00296104 t=0.1 observer_writes=0',
            'nsk1_tuning_source: address=0029822c value=17971d00 mask=ffffffff pc=00296104 t=0.1 observer_writes=0',
            *[f'nsk1_tuning_upload: space=program address={address} value={value} pc=31d7 t=0.2 observer_writes=0' for address,value in [('216a','0004'),('216b','17bf'),('216c','1797')]],
            'nsk1_tuning_pair: port=31 value=3633 pc=4023 channel=0374 flags=0020 addend=17bf a=0000303633 t=38.8 observer_writes=0',
            'nsk1_tuning_pair: port=32 value=0030 pc=4028 channel=0374 flags=0020 addend=17bf a=0000303633 t=38.9 observer_writes=0'])
        self.assertEqual(check_tuning_provenance(text)['packed_word'],'0x303633')
        for before,after in [('17bf','13a6'),('value=3633','value=3632'),('t=0.2','t=40.2'),('mask=ffffffff','mask=0000ffff')]:
            with self.subTest(before=before), self.assertRaises(ValueError):
                check_tuning_provenance(text.replace(before,after))

    def test_serial_queue_is_separate_and_requires_complete_port_evidence(self):
        text = '\n'.join(f'nsk1_publisher_entry: address=3f50 index={i} ar4=0000 ar5=0004 serial06b6=0000 t={2+i*0.005:.6f} observer_writes=0' for i in range(1,5))
        text += '\nnsk1_publisher_entry: address=4810 index=1 ar4=008b ar5=03ff serial06b6=0000 t=38.791415 observer_writes=0\nrom4_serial_queue_summary: cpu_bus_writes=2 nonzero=0 observer_writes=0\n'
        text += '\n'.join(f'rom4_serial_summary: port={port} direction={direction} count={count} last_value={value} last_pc={pc}' for port,direction,count,value,pc in [('35','read',0,'0000','0000'),('35','write',0,'0000','0000'),('36','read',4,'0080','9183'),('36','write',2,'0080','90bc')])
        self.assertEqual(check_serial_queue(text)['data_reads'],0)
        for before,after in [('nonzero=0','nonzero=01'),('direction=read count=0','direction=read count=1'),('port=36','port=37')]:
            with self.subTest(before=before), self.assertRaises(ValueError):
                check_serial_queue(text.replace(before,after))

    def test_serial_negative_requires_its_positive_control(self):
        text = '\n'.join(f'nsk1_publisher_entry: address=3f50 index={i} ar4=0000 ar5=0004 serial06b6=0000 t={2+i*0.005:.6f} observer_writes=0' for i in range(1,5))
        positive = 'nsk1_publisher_entry: address=4810 index=1 ar4=008b ar5=03ff serial06b6=0000 t=38.791415 observer_writes=0'
        self.assertTrue(check_serial_publishers(text+'\n'+positive)['rssi_positive_control'])
        with self.assertRaises(ValueError):
            check_serial_publishers(text)
        with self.assertRaises(ValueError):
            check_serial_publishers(text+'\n'+positive.replace('4810','9404'))

    def test_zero_measurement_and_negative_timeout_are_bounded_observations(self):
        text = '\n'.join(f'nsk1_scan_metric: index={i+1} pc=0ae7 a=0000000000 threshold=0574 t={39+i*0.005:.6f} observer_writes=0' for i in range(8))
        text += '\n' + '\n'.join(f'nsk1_scan_late: t={65+5*i:.6f} counter_hi=ffff counter_lo={0xd7be+1000*i:04x} qualified=0000 flags=0000 observer_writes=0' for i in range(6))
        self.assertTrue(check_scan_qualification(text)['timeout_counter_still_negative'])
        for before,after in [('pc=0ae7','pc=0ae6'),('threshold=0574','threshold=0000'),('a=0000000000','a=0000000575'),('90.000000','89.000000'),('counter_hi=ffff','counter_hi=0000'),('qualified=0000','qualified=0001')]:
            with self.subTest(before=before), self.assertRaises(ValueError):
                check_scan_qualification(text.replace(before,after))

    def test_progress_is_not_registration_or_an_empty_queue_stall(self):
        text = '\n'.join(f'nsk1_scan_progress: t={40+5*i:.6f} counter_hi=ffff counter_lo={0xc296+1000*i:04x} qualified=0000 flags=0000 producer=1a00 consumer=1a00 observer_writes=0' for i in range(5))
        self.assertEqual(check_scan_progress(text), {'samples':5,'counter_advance':4000,'qualified_results_observed':False})
        for before,after in [('60.000000','61.000000'),('counter_lo=c67e','counter_lo=c296'),('qualified=0000','qualified=0001'),('consumer=1a00','consumer=1a04')]:
            with self.subTest(before=before), self.assertRaises(ValueError):
                check_scan_progress(text.replace(before,after))


if __name__ == '__main__':
    unittest.main()

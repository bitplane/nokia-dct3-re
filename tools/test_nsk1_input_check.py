import unittest

from tools.nsk1_input_check import check


class Nsk1InputCheckTest(unittest.TestCase):
    def trace(self):
        lines = []
        for mask,time in ((4,12),(16,14),(2,16)):
            for down,when in ((1,time),(0,time+0.2)):
                lines.append(f'nsk1_key_contact: mask={mask:02x} down={down} irq=00 irqmask=ce ctrl=05 column_mask=3f root_byte=06 checklist=0a layout=00 simi_reads=0 simi_writes=0 t={when} physical_only=1')
        lines.extend(('nsk1_task1_scalar: value=00000016 caller=0029195b cycles=0000000000ff1466 observer_writes=0',
                      'nsk1_checklist_compare: bits=0a cycles=0000000001213e96 observer_writes=0',
                      'nsk1_control_reply: payload=0d00 envelope=00000074/00040100 t=0.54 observer_writes=0'))
        return '\n'.join(lines)

    def test_physical_observation_is_not_ui_acceptance(self):
        result = check(self.trace())
        self.assertEqual(6,len(result['contacts']))
        self.assertEqual([],result['forwarded_keys'])
        self.assertEqual([4,4,16,16,2,2],[row['mask'] for row in result['contacts']])
        self.assertEqual(0xce,result['contacts'][0]['irq_mask'])
        self.assertEqual(0x0a,result['contacts'][0]['checklist'])
        self.assertEqual(0,result['contacts'][-1]['simi_reads'])
        self.assertEqual([0x16],[row['value'] for row in result['task1_scalars']])
        self.assertEqual(['0d00'],result['control_reply_payloads'])
        self.assertEqual([10],result['checklist_comparisons'])
        self.assertIn('not interactive',result['scope'])
        forwarded = '\nnsk1_key_forward: key=19 caller=00294800 cycles=0000000000000001 observer_writes=0'
        self.assertEqual([0x19],check(self.trace()+forwarded)['forwarded_keys'])

    def test_malformed_missing_or_reordered_observation_fails(self):
        text = self.trace()
        for changed in (text.replace('mask=04','mask=10',1),text.replace('t=12 ','t=11 ',1),
                        '\n'.join(text.splitlines()[1:]),text+'\nnsk1_key_contact: malformed',
                        text+'\nnsk1_key_forward: malformed',text+'\nnsk1_task1_scalar: changed',
                        text+'\nnsk1_control_reply: unknown',text+'\nnsk1_checklist_compare: unknown'):
            with self.assertRaises(ValueError):
                check(changed)


if __name__ == '__main__':
    unittest.main()

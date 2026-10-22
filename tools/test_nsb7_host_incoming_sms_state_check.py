from contextlib import ExitStack
from unittest.mock import patch
import unittest

from tools import nsb7_host_incoming_sms_state_check as target
from tools.sim_security_trace_check import AOC_NVRAM_SIZE


def fixture():
    lines=['incoming sms id=1 result=accepted','incoming sms state id=1 epoch=1 phase=queued',
           'GSM service downlink kind=16 sapi=3 pd=09 message=01',
           'nsb7_host_sms_save_trigger: pc=002645ec pd=09 message=01 t=21.480950000 observer_writes=0',
           'nsb7_state: event=saved pc=0030ed52 t=21.481000000']
    for phase,epoch in (('reference',1),('restored',2)):
        if epoch==2:lines+=['nsb7_state: event=restored']
        lines += [f'state_replay: phase={phase} event=begin']
        if epoch==2:lines+=['incoming sms state id=1 epoch=2 phase=queued']
        lines += ['GSM service uplink sapi=3 pd=09 message=04 ',
                  'sim_device: update fid=6f3c record=1 length=176',
                  'GSM service uplink sapi=3 pd=09 message=01 length=5 data=8901020240',
                  f'incoming sms state id=1 epoch={epoch} phase=delivered',
                  f'state_replay: phase={phase} event=end']
    lines+=['nsb7_sms_input: key=Menu state=1 t=26.000000000','nsb7_state_sms: complete']
    return '\n'.join(lines)


class IncomingStateTest(unittest.TestCase):
    def test_ingress_timing_epochs_and_both_complete_branches_are_required(self):
        with ExitStack() as stack:
            mocks={name:stack.enter_context(patch.object(target,name)) for name in
                   ('check_source','check_authentication','check_architecture','check_read','check_clock')}
            text=fixture()
            target.check(text,bytes(AOC_NVRAM_SIZE),b'final')
            continuous=mocks['check_read'].call_args.args[0]
            self.assertEqual(continuous.count('sim_device: update fid=6f3c'),1)
            mocks['check_clock'].assert_called_once_with(continuous,finish=35,control_values=('00',))
            for bad in (text.replace('t=21.481000000','t=21.490000000'),
                        text.replace('epoch=2 phase=delivered','epoch=1 phase=delivered'),
                        text.replace('sim_device: update fid=6f3c record=1 length=176','',1),
                        text.replace('GSM service uplink sapi=3 pd=09 message=04 ','',1),
                        text+'\nincoming sms id=2 result=accepted',
                        'sim_device: update fid=6f3c\n'+text,
                        text.replace('pd=09 message=01 t=21.480950000','pd=03 message=01 t=21.480950000')):
                with self.assertRaises(ValueError):target.check(bad,bytes(AOC_NVRAM_SIZE),b'final')


if __name__=='__main__':unittest.main()

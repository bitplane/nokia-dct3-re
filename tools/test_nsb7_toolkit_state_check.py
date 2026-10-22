import unittest
from unittest.mock import patch
from tools.dct3_toolkit_check import menu_events
from tools.nsb7_toolkit_state_check import check


def fixture():
    events=menu_events('nsb7')
    prefix='\n'.join(events[:events.index('nsb7_toolkit_interactive: action=menu_select')])
    cpu=','.join(['00000000']*37)
    state=f'pc=002f8300 sp=00130df8 ram=a6a910ae cpu={cpu} t=85.250000000'
    rows=[prefix,'nsb7_state: event=saved '+state]
    for phase in ('reference','restored'):
        if phase=='restored':
            rows+=['nsb7_state: event=restored '+state,
                'state_roundtrip: result=pass scenario=nsb7_toolkit_menu requested_at=85.250000000 t=85.250000000']
        rows += [f'state_replay: phase={phase} event=begin t=85.250000000',
            f'nsb7_toolkit_state: action=menu_select phase={phase}',
            'nsb7_key_decode: key=19 observer_writes=0',
            'sim_device: header cla=a0 ins=c2 p1=00 p2=00 p3=09',
            'sim_device: envelope data=d30702020181100101',
            'sim_device: menu selection item=1 accepted','SIM status ins=c2 sw=9000',
            'ccont_rtc: event=second time=13:49:11 day=0 t=86.000000000',
            f'state_replay: phase={phase} event=end t=88.250000000']
    rows += ['nsb7_toolkit_state_input: key=End state=1 t=90.000000000',
             'nsb7_key_decode: key=0f observer_writes=0',
             'nsb7_toolkit_state_input: key=End state=0 t=90.200000000']
    return '\n'.join(rows)


class ToolkitStateTest(unittest.TestCase):
    @patch('tools.nsb7_toolkit_state_check.check_authentication')
    def test_exact_replay(self,authentication):
        text=fixture()
        check(text)
        authentication.assert_called_once_with(text.split('nsb7_state: event=saved',1)[0])

    @patch('tools.nsb7_toolkit_state_check.check_authentication')
    def test_architecture_protocol_and_clock_changes_rejected(self,authentication):
        for bad in (fixture().replace('event=restored pc=002f8300','event=restored pc=002f8302'),
                    fixture().replace('d30702020181100101','d30702020181100102',1),
                    fixture().replace('time=13:49:11','time=13:49:12',1),
                    fixture().replace('cpu=00000000,','cpu=',1)):
            with self.subTest(text=bad),self.assertRaises(ValueError):
                check(bad)

    @patch('tools.nsb7_toolkit_state_check.check_authentication')
    def test_physical_input_and_once_per_phase_required(self,authentication):
        for bad in (fixture().replace('key=19','key=17',1),
                    fixture().replace('phase=reference','phase=restored',1),
                    fixture().replace('key=0f','key=19'),
                    fixture()+'\nsim_device: header cla=a0 ins=c2 p1=00 p2=00 p3=09'):
            with self.subTest(text=bad),self.assertRaises(ValueError):
                check(bad)

    def test_authentication_required(self):
        with self.assertRaises(ValueError):
            check(fixture())


if __name__=='__main__':
    unittest.main()

import unittest
from tools.sim_toolkit_display_check import check_observer_console,wire


class ObserverConsoleTest(unittest.TestCase):
    def test_refresh_cap_is_explicit_and_saturation_still_fails(self):
        row='nsm1_sim_wire: direction=RX data=90 t=1 observer_writes=0\n'
        with self.assertRaises(ValueError):
            wire(row*8192,'RX','nsm1')
        self.assertEqual(wire(row*8749,'RX','nsm1',limit=16384),b'\x90'*8749)
        with self.assertRaises(ValueError):
            wire(row*16384,'RX','nsm1',limit=16384)

    def test_ordinary_output_and_declared_machine_warning_are_not_failures(self):
        check_observer_console('Starting native backend\nTHIS MACHINE DOES NOT WORK\nAverage speed: 249%\n')

    def test_lua_failure_is_rejected_even_with_successful_exit_text(self):
        for error in ('[LUA ERROR] in resume: navigation contact missing',
                      'Error in Lua callback: assertion failed'):
            with self.subTest(error=error), self.assertRaises(ValueError):
                check_observer_console(error+'\nAverage speed: 249%\nreturncode=0\n')


if __name__=='__main__':
    unittest.main()

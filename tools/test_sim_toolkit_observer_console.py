import unittest
from tools.sim_toolkit_display_check import check_observer_console


class ObserverConsoleTest(unittest.TestCase):
    def test_ordinary_output_and_declared_machine_warning_are_not_failures(self):
        check_observer_console('Starting native backend\nTHIS MACHINE DOES NOT WORK\nAverage speed: 249%\n')

    def test_lua_failure_is_rejected_even_with_successful_exit_text(self):
        for error in ('[LUA ERROR] in resume: navigation contact missing',
                      'Error in Lua callback: assertion failed'):
            with self.subTest(error=error), self.assertRaises(ValueError):
                check_observer_console(error+'\nAverage speed: 249%\nreturncode=0\n')


if __name__=='__main__':
    unittest.main()

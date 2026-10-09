import unittest

from tools.run_noki8890_code_change import check_change


class CodeChangeTest(unittest.TestCase):
    def traces(self):
        actions = ['menu', 'menu_2', 'menu_3', 'menu_4', 'settings']
        actions += [f'settings_{i}' for i in range(2, 7)] + ['security']
        actions += [f'security_{i}' for i in range(2, 6)] + ['access_codes', 'access_2', 'access_3', 'old_prompt']
        actions += [f'old_{i}' for i in range(1, 6)] + ['new_prompt']
        actions += [f'new_{i}' for i in range(5, 0, -1)] + ['confirm_prompt']
        actions += [f'confirm_{i}' for i in range(5, 0, -1)] + ['result']
        saved = ''.join(f'8890_code_change_physical: action={a}\n' for a in actions)
        saved += '8890_code_change: event=persist encoded=d87d3698 caller=002fa503\n'
        cold = ''.join(f'8890_changed_code_physical: key={a}\n' for a in
                       ['Keypad 5', 'Keypad 4', 'Keypad 3', 'Keypad 2', 'Keypad 1', 'Menu'])
        cold += '8890_changed_code: event=input bytes=353433323100\n'
        cold += '8890_changed_code: event=compare result=00000000 stored=d87d3698 input=d87d3698\n'
        return saved, cold

    def test_complete(self):
        check_change(*self.traces())

    def test_missing_persistence(self):
        saved, cold = self.traces()
        with self.assertRaises(ValueError):
            check_change(saved.replace('event=persist', 'event=other'), cold)

    def test_wrong_saved_code(self):
        saved, cold = self.traces()
        with self.assertRaises(ValueError):
            check_change(saved.replace('d87d3698', 'd33098dc'), cold)

    def test_rejected_cold_code(self):
        saved, cold = self.traces()
        with self.assertRaises(ValueError):
            check_change(saved, cold.replace('result=00000000', 'result=fffffffb'))

    def test_wrong_physical_input(self):
        saved, cold = self.traces()
        with self.assertRaises(ValueError):
            check_change(saved, cold.replace('key=Keypad 5', 'key=Keypad 1'))

    def test_save_before_confirmation(self):
        saved, cold = self.traces()
        event = '8890_code_change: event=persist encoded=d87d3698 caller=002fa503\n'
        saved = saved.replace(event, '').replace('8890_code_change_physical: action=result\n',
                                               event + '8890_code_change_physical: action=result\n')
        with self.assertRaises(ValueError):
            check_change(saved, cold)

    def test_cold_compare_before_keys(self):
        saved, cold = self.traces()
        event = '8890_changed_code: event=compare result=00000000 stored=d87d3698 input=d87d3698\n'
        with self.assertRaises(ValueError):
            check_change(saved, event + cold.replace(event, ''))


if __name__ == '__main__':
    unittest.main()

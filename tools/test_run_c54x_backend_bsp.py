import unittest
from tools.run_c54x_backend_bsp import MARKER, COMPLETION, check_result


class BackendBspResultTest(unittest.TestCase):
    def test_success(self):
        check_result(f'{MARKER}\n{COMPLETION}\n', 3)

    def test_incomplete_or_duplicate(self):
        for text in (COMPLETION, MARKER, f'{MARKER}\n{MARKER}\n{COMPLETION}',
                     f'Fatal error: fixture failed\n{MARKER}\n{COMPLETION}'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                check_result(text, 3)

    def test_normal_exit_does_not_prove_completion(self):
        with self.assertRaises(ValueError):
            check_result(f'{MARKER}\n{COMPLETION}', 0)


if __name__ == '__main__':
    unittest.main()

import unittest
from tools.dct3_toolkit_check import display_text_events, verify_display_text


class AcceptedDisplayTextTest(unittest.TestCase):
    def test_own_product_and_profile(self):
        for product in ('8210', '8850', '6250'):
            text = '\n'.join(display_text_events(product))
            verify_display_text(text, product)
            for wrong in ('8890', '3210'):
                with self.subTest(product=product, wrong=wrong), self.assertRaises(ValueError):
                    verify_display_text(text, wrong)
            with self.assertRaises(ValueError):
                verify_display_text(text, product, 5)

    def test_every_transition_required(self):
        events = display_text_events('8210')
        for index in range(len(events)):
            with self.subTest(index=index), self.assertRaises(ValueError):
                verify_display_text('\n'.join(events[:index] + events[index + 1:]), '8210')

    def test_screen_busy_is_not_success(self):
        text = '\n'.join(display_text_events('8210'))
        with self.assertRaises(ValueError):
            verify_display_text(text.replace('030100', '03022001'), '8210')


if __name__ == '__main__':
    unittest.main()

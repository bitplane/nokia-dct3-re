import unittest

from tools.mu4_context_contract import playback_final_input_words
from tools.mu4_playback_version_check import TARGET,compare_images,compare_sources


class PlaybackVersionTests(unittest.TestCase):
    def images(self):
        old=playback_final_input_words()
        new=dict(old)
        new[0x2958b]=0x9019
        old.update({0x39005+i:word for i,word in enumerate(TARGET)})
        new.update({0x39019+i:word for i,word in enumerate(TARGET)})
        return old,new

    def test_policy_and_relocated_target(self):
        result=compare_images(*self.images())
        self.assertEqual(result['policy_words'],235)
        self.assertEqual(result['identical_policy_words'],234)
        self.assertEqual(result['relocated_call']['R061'],'039019')
        self.assertEqual(result['relocated_call']['identical_target_words'],28)
        self.assertIn('not whole-image',result['scope'])

    def test_every_policy_word_is_checked_in_both_images(self):
        for index in range(2):
            for address in playback_final_input_words():
                images=self.images()
                images[index][address]^=1
                with self.subTest(image=index,address=address),self.assertRaises(ValueError):
                    compare_images(*images)

    def test_every_relocated_target_word_is_checked(self):
        for index,base in enumerate((0x39005,0x39019)):
            for offset in range(len(TARGET)):
                images=self.images()
                images[index][base+offset]^=1
                with self.subTest(image=index,offset=offset),self.assertRaises(ValueError):
                    compare_images(*images)

    def test_other_differences_are_reported_not_hidden(self):
        old,new=self.images()
        baseline=compare_images(old,new)['raw_destination_differences']
        new[0x45678]=0x1234
        self.assertEqual(compare_images(old,new)['raw_destination_differences'],baseline+1)

    def test_unqualified_sources_fail(self):
        with self.assertRaises(ValueError):compare_sources(b'wrong',b'wrong')


if __name__=='__main__':
    unittest.main()

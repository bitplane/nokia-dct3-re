import hashlib
import unittest
from unittest.mock import patch

from tools import nsb3_radio_check as checker


class Nsb3RadioTest(unittest.TestCase):
    def test_consumer_requires_same_native_object_and_exact_cached_vector(self):
        cached = bytes(range(160))
        text = '\n'.join([
            'nsb3_radio_rx: index=15 type=8b length=162 data=' + (b'\x00\x40' + cached).hex() + ' cursor=8b',
            'nsb3_radio_8b_forward: message=00102c40 header=1802a28b observer_writes=0',
            'nsb3_radio_object_read: object=00102c40 data=13b80000 mask=ffff0000 pc=00219c7e',
            'nsb3_radio_result_branch: object=00102c40 state=03 phase=01 cache=00000000 observer_writes=0',
            'nsb3_radio_result_tail: state=03 phase=01 cache=00102e80 observer_writes=0',
            'nsb3_radio_cached_vector: pointer=00102e80 bytes=160 state=03 phase=01 t=45 observer_writes=0',
            'nsb3_radio_controller_state: value=00 pc=0020010e t=0.002 observer_writes=0',
            'nsb3_radio_controller_state: value=03 pc=00219c3e t=1.18 observer_writes=0'])
        checker.check_consumer(text, cached)
        for bad in (text.replace('object=00102c40', 'object=00102c44'),
                    text.replace('state=03', 'state=01'),
                    text.replace('pointer=00102e80', 'pointer=00102e84'),
                    text.replace('pc=00219c7e', 'pc=00219c80'),
                    text + '\nnsb3_radio_controller_state: value=01 pc=00218bf6 t=50 '):
            with self.assertRaises(ValueError):
                checker.check_consumer(bad, cached)
        with self.assertRaises(ValueError):
            checker.check_consumer(text, cached[:-1] + b'\xff')

    def test_ring_bytes_extent_and_cursor_are_checked(self):
        text = ('nsb3_radio_tx: index=1 type=4a length=4 data=02000000 '
                'cursor=00 producer=03 pc=002941fe t=4.8 observer_writes=0')
        digest = hashlib.sha256(b'4a:4:02000000').hexdigest()
        with patch.dict(checker.DIGESTS, tx=digest):
            self.assertEqual(len(checker.packets(text, 'tx')), 1)
            for bad in (text.replace('index=1', 'index=2'),
                        text.replace('cursor=00', 'cursor=01'),
                        text.replace('producer=03', 'producer=02'),
                        text.replace('producer=03', 'producer=52'),
                        text.replace('length=4', 'length=3'),
                        text.replace('data=02000000', 'data=03000000'),
                        text.replace('observer_writes=0', 'observer_writes=1'), ''):
                with self.assertRaises(ValueError):
                    checker.packets(bad, 'tx')

    def test_receive_ring_uses_own_absolute_index_origin(self):
        text = ('nsb3_radio_rx: index=1 type=74 length=2 data=0d00 '
                'cursor=80 producer=82 dsp_pc=3805 t=0.3 observer_writes=0')
        digest = hashlib.sha256(b'74:2:0d00').hexdigest()
        with patch.dict(checker.DIGESTS, rx=digest):
            self.assertEqual(len(checker.packets(text, 'rx')), 1)
            with self.assertRaises(ValueError):
                checker.packets(text.replace('cursor=80', 'cursor=00'), 'rx')

    def test_wrong_image_and_unobserved_native_state_fail(self):
        with self.assertRaises(ValueError):
            checker.contract(b'not own firmware')
        with self.assertRaises(ValueError):
            checker.check('')


if __name__ == '__main__':
    unittest.main()

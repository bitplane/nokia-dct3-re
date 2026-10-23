import unittest

from tools.nse2_boot_check import verify_upload


def transcript():
    payload = b''.join(i.to_bytes(2, 'big') for i in range(186)) + bytes(28)
    text = ''
    for i in range(200):
        value = int.from_bytes(payload[i * 2:i * 2 + 2], 'big')
        text += (f'nse2_upload_word: n={i + 1} address={0x10e00 + i * 2:08x} '
                 f'data={value:04x} mask=ffff pc=0029acc4 t=0.00{i + 1000} observer_writes=0\n')
    controls = ((0x100, 0x2781b0), (0x300, 0x2781b8), (1, 0x2781c2),
                (0, 0x2781c6), (1, 0x2781ca), (1, 0x2781ce), (0x200, 0x2781d6))
    for i, (value, pc) in enumerate(controls):
        text += (f'nse2_upload_word: n={201 + i} address={0x100f6 + i * 2:08x} '
                 f'data={value:04x} mask=ffff pc={pc:08x} t=0.002{i} observer_writes=0\n')
    for i in range(22):
        pc = 0x2781fa if i == 0 else 0x27821a
        text += (f'nse2_startup_poll: n={i + 1} address=00010004 data=ffff mask=ffff '
                 f'pc={pc:08x} t=0.003{i:02d} observer_writes=0\n')
    return text, payload


class UploadTest(unittest.TestCase):
    def test_exact_copy_is_not_native_execution_or_completion(self):
        result = verify_upload(*transcript())
        self.assertEqual(result['rounded_upload_words'], 200)
        self.assertEqual(result['startup_poll_reads'], 22)
        self.assertFalse(result['native_execution'])
        self.assertFalse(result['completed_ownership_exchange'])

    def test_words_addresses_masks_and_count_are_independent_requirements(self):
        text, payload = transcript()
        for old, new in (('data=0001 mask=ffff pc=0029acc4', 'data=0002 mask=ffff pc=0029acc4'),
                         ('address=00010e02', 'address=00010e04'),
                         ('mask=ffff', 'mask=00ff'), ('n=200 ', 'n=201 '),
                         ('pc=0029acc4', 'pc=0029acc0')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                verify_upload(text.replace(old, new), payload)

    def test_control_values_and_order_cannot_be_replaced_by_polling(self):
        text, payload = transcript()
        for old, new in (('data=0300', 'data=0301'),
                         ('pc=002781d6', 'pc=002781d4'),
                         ('t=0.0020', 't=0.0040')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                verify_upload(text.replace(old, new), payload)

    def test_live_sentinel_poll_required_and_capped_capture_rejected(self):
        text, payload = transcript()
        for old, new in (('address=00010004 data=ffff', 'address=00010004 data=0001'),
                         ('nse2_startup_poll:', 'missing_poll:'),
                         ('n=22 address=00010004', 'n=23 address=00010004'),
                         ('t=0.00300', 't=0.00900')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                verify_upload(text.replace(old, new), payload)

    def test_source_extent_and_zero_padding_required(self):
        text, payload = transcript()
        for bad in (payload[:-2], payload + b'\0\0', payload[:-1] + b'\1'):
            with self.subTest(size=len(bad)), self.assertRaises(ValueError):
                verify_upload(text, bad)


if __name__ == '__main__':
    unittest.main()

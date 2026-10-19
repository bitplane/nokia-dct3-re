import unittest

from tools.mu4_recorder_full_transform_check import verify, verify_file
from tools.mu4_context_contract import reference_transform_block


def fixture():
    state = list(range(82))
    output = reference_transform_block(1, 2, state, reverse=False)
    words = lambda values: ','.join(f'{value:04x}' for value in values)
    return '\n'.join([
        'mu4_native_recorder_overlay: PASS marker=aa88 records=4622 words=73766 '
        'checked_ram_words=73762 excluded_register_words=4 before_execution=1',
        'mu4_record_full_block: ordinal=1 caller=03edae sp=6000 pointers=6100,6101 '
        f'input=0001,0002 output={words(output)} state={words(state)} '
        'reads=3 writes=3 state_other_writes=0 source_writes=0 domain=other '
        'start=1.0 end=1.1 identity=Execute_entry_and_bus observer_writes=0',
        'mu4_record_full_caller: caller=03edae calls=1 observer_writes=0',
        'mu4_record_full_terminal: entries=1 returns=1 unadmitted=0 bad=0 dropped=0 '
        'active=0 pending_write=0 stop=20.0 observer_writes=0',
    ])


def aligned_fixture():
    text = fixture().replace('03edae', '03ee95').replace('pointers=6100,6101', 'pointers=6012,6013')
    return text.replace('domain=other', 'domain=aligned32 plain=01000200'+'00'*28+
                        ' arguments=0000,0020,7000,0000,001c,0004 position=00000000 cursor=00000000')


class FullTransformTest(unittest.TestCase):
    def test_complete_call(self):
        self.assertEqual(verify(fixture())['calls'], 1)

    def test_rejects_incomplete_or_unowned_calls(self):
        for old, new in [('bad=0', 'bad=1'), ('reads=3', 'reads=1'),
                         ('returns=1', 'returns=0'), ('ordinal=1', 'ordinal=2'),
                         ('state_other_writes=0', 'state_other_writes=1'),
                         ('observer_writes=0', 'observer_writes=1'),
                         ('end=1.1', 'end=0.9'), ('calls=1', 'calls=2')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                verify(fixture().replace(old, new))

    def test_rejects_changed_output(self):
        text = fixture()
        output = text.split('output=', 1)[1].split()[0]
        with self.assertRaises(ValueError):
            verify(text.replace('output='+output, 'output=0000,0000'))

    def test_requires_original_upload_before_calls(self):
        text = fixture()
        first, rest = text.split('\n', 1)
        for changed in (rest, rest+'\n'+first, text+'\n'+first):
            with self.subTest(text=changed), self.assertRaises(ValueError):
                verify(changed)

    def test_aligned_window_abi(self):
        text = aligned_fixture()
        self.assertEqual(verify(text)['domains'], {'aligned32': 1})
        for old, new in [('6012,6013', '6011,6012'), ('0020,7000', '0010,7000'),
                         ('01000200', '03000200'), ('position=00000000', 'position=00000001')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                verify(text.replace(old, new))

    def test_fixed_origin_native_file_correspondence(self):
        text = aligned_fixture()
        output = reference_transform_block(1, 2, list(range(82)), reverse=False)
        native = b''.join(word.to_bytes(2, 'little') for word in output)+bytes(28)
        report, plain = verify_file(text, b'header'+native+b'tail')
        self.assertEqual(report['file_origin'], 6)
        self.assertEqual(plain, bytes.fromhex('01000200')+bytes(28))
        self.assertFalse(report['whole_file_validated'])
        for saved in (b'header'+native+native, b'wrong', b'header'+native[:-1]):
            with self.subTest(saved=saved), self.assertRaises(ValueError):
                verify_file(text, saved)
        with self.assertRaises(ValueError):
            verify_file(text.replace('cursor=00000000', 'cursor=00000020'), native)

    def test_divergent_tail_is_inventory_not_acceptance(self):
        text = aligned_fixture()
        first = text.splitlines()[1]
        state = list(range(82))
        output = reference_transform_block(3, 4, state, reverse=False)
        word_text = lambda values: ','.join(f'{value:04x}' for value in values)
        second = (first.replace('ordinal=1', 'ordinal=2').replace('input=0001,0002', 'input=0003,0004')
                  .replace('output='+word_text(reference_transform_block(1, 2, state, reverse=False)), 'output='+word_text(output))
                  .replace('plain=01000200', 'plain=03000400').replace('cursor=00000000', 'cursor=00000020')
                  .replace('start=1.0 end=1.1', 'start=1.2 end=1.3'))
        text = text.replace(first, first+'\n'+second).replace('calls=1', 'calls=2').replace('entries=1 returns=1', 'entries=2 returns=2')
        native = b''.join(w.to_bytes(2, 'little') for w in reference_transform_block(1, 2, state, reverse=False))+bytes(28)
        stored = b'header'+native+b'wrong tail'
        with self.assertRaises(ValueError):
            verify_file(text, stored)
        report, plain = verify_file(text, stored, inventory_divergence=True)
        self.assertEqual(report['persisted_native_prefix_bytes'], 32)
        self.assertFalse(report['stored_file_correspondence_validated'])
        self.assertEqual(len(plain), 64)


if __name__ == '__main__':
    unittest.main()

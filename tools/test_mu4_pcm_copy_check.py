import unittest

from tools.mu4_pcm_copy_check import check


def fixture():
    lines = ['mu4_pcm_copy_scope: first=1.0 last=1.25 cap=65536 source_first=7000 source_last=7fff destination_first=1000 destination_last=137f copy_objects=13e0,1460 read_context=02a31f..02a371 write_context=02a367 source_write_tracking=last_actual boundary=data_bus context_pc=not_owner observer_writes=0 audio_acceptance=0',
             'mu4_decoder_call: call=1 phase=return result=0000000000 t=1.005 identity=Execute_entry context_only=1 observer_writes=0']
    for sample in range(1024):
        for channel in range(2):
            address = 0x7000+1024*channel+sample
            value = sample*3+channel
            lines.append(f'mu4_conversion_value: call=1 frame=0 sample={sample} channel={channel} '
                         f'destination={address:04x} result={value:010x} argument=14 t=1.010 '
                         'identity=Execute_entry observer_writes=0 retirement=unclaimed')
    lines.append('mu4_conversion_block: call=1 frame=0 samples=1024 destination=7000 bad=0 t=1.020 identity=Execute_entry observer_writes=0 audio_acceptance=0')
    ordinal = 0
    for chunk in range(16):
        for channel in range(2):
            for offset in range(64):
                sample = chunk*64+offset
                source = 0x7000+1024*channel+sample
                target = 0x1000+(chunk%2)*128+channel*64+offset
                value,object_ = sample*3+channel,(0x13e0 if channel == 0 else 0x1460)
                for kind,address in (('read',source),('write',target)):
                    ordinal += 1
                    origin = (f' last_write_word={value:04x} last_write_mask=ffff last_write_t=1.011 '
                              'last_write_pc=023e1e') if kind == 'read' else ''
                    lines.append(f'mu4_pcm_copy_bus: ordinal={ordinal} kind={kind} address={address:04x} '
                                 f'word={value:04x} mask=ffff t={1.1+ordinal/1000000:.9f} context_pc=02a367 '
                                 f'ar1=0040 ar2=1000 ar3=7000 ar4={object_:04x}{origin} observer_writes=0')
    lines += ['mu4_pcm_destination_census: context_pc=02a367 writes=2048 observer_writes=0',
              'mu4_pcm_copy_terminal: events=4096 reads=2048 writes=2048 source_writes=2048 destination_writes=2048 contexts=1 closed=1 capped=0 illegal=0 t=2.016666667 observer_writes=0 audio_acceptance=0']
    return '\n'.join(lines)


class PcmCopyTest(unittest.TestCase):
    def test_every_word_and_chunk_has_fixed_address_correspondence(self):
        result = check(fixture(),frames=1)
        self.assertEqual(result['covered_words'],2048)
        self.assertEqual(result['chunks'],32)
        self.assertEqual(result['right_censored_reads'],0)
        self.assertFalse(result['native_audio_validated'])

    def test_rejects_stale_or_intervening_writes_wrong_values_and_missing_coverage(self):
        text = fixture()
        for old,new in (('last_write_word=0000','last_write_word=0001'),
                        ('last_write_mask=ffff','last_write_mask=ff00'),
                        ('last_write_pc=023e1e','last_write_pc=025000'),
                        ('last_write_t=1.011','last_write_t=1.001'),
                        ('source_write_tracking=last_actual','source_write_tracking=none'),
                        ('kind=write address=1000 word=0000','kind=write address=1000 word=0001'),
                        ('kind=write address=1000','kind=write address=1040'),
                        ('reads=2048','reads=2047'),('events=4096','events=4095'),
                        ('capped=0','capped=1'),('illegal=0','illegal=1'),('bad=0','bad=1'),
                        ('result=0000000000 t=1.005','result=000000ffff t=1.005'),
                        ('t=2.016666667','t=1.1')):
            self.assertIn(old,text)
            with self.subTest(old=old), self.assertRaises(ValueError):
                check(text.replace(old,new),frames=1)

    def test_duplicate_converter_blocks_and_unobserved_source_are_rejected(self):
        text = fixture()
        block = next(line for line in text.splitlines() if 'mu4_conversion_block:' in line)
        with self.assertRaises(ValueError):
            check(text+'\n'+block,frames=1)
        with self.assertRaises(ValueError):
            check(text.replace('last_write_t=1.011','last_write_t=unobserved'),frames=1)

    def test_program_alias_count_is_explicit_and_bounded(self):
        text = fixture()
        self.assertIsNone(check(text,frames=1)['program_alias_writes'])
        for count in (0,1,2048):
            observed = text.replace('contexts=1 closed=',f'program_alias_writes={count} contexts=1 closed=')
            self.assertEqual(check(observed,frames=1)['program_alias_writes'],count)
        for count in (-1,2049):
            with self.assertRaises(ValueError):
                check(text.replace('contexts=1 closed=',f'program_alias_writes={count} contexts=1 closed='),frames=1)


if __name__ == '__main__':
    unittest.main()

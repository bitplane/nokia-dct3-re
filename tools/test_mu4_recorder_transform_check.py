import contextlib
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools import mu4_recorder_transform_check as checker
from tools.mu4_context_contract import reference_transform_block


def fixture(alternate_state=False):
    plain = bytes((index*13) % 256 for index in range(64))
    lines = ['mu4_native_recorder_overlay: PASS marker=aa88 records=4622 words=73766 '
             'checked_ram_words=73762 excluded_register_words=4 before_execution=1']
    encoded = bytearray()
    for sample in range(2):
        source = plain[sample*32:sample*32+32]
        state = [(word+sample if alternate_state else word) for word in range(82)]
        entry = [int.from_bytes(source[index:index+2],'little') for index in (0,2)]
        output = reference_transform_block(*entry,state,reverse=False)
        encoded.extend(b''.join(word.to_bytes(2,'little') for word in output)+source[4:])
        when = 1.1+sample/1000
        lines.extend([
            f'mu4_record_transform_input: caller=03ee95 target=6e9c left={entry[0]:04x} right={entry[1]:04x} '
            f'state_base=fc6c state={",".join(f"{word:04x}" for word in state)} sp=6e82 t={when:.9f} '
            f'context_pc=03f10d ownership=unvalidated observer_writes=0 sample={sample+1}',
            f'mu4_record_transform_outer: caller=03ee95 sample={sample+1} candidate_sp=6e8c source=fd4e '
            f'position=00000000 words_16_1b=0000,0020,ffa0,0000,001c,0004 t={when:.9f} '
            f'context_only=1 observer_writes=0 cursor_pointer=ffa0 cursor={sample*32:08x} '
            f'source_words={",".join(f"{word:04x}" for word in source)}',
            f'mu4_record_transform_output: caller=03ee95 target=6e9c left={output[0]:04x} right={output[1]:04x} '
            f't={when+0.00001:.9f} ownership=unvalidated observer_writes=0 audio_acceptance=0 sample={sample+1}',
        ])
    return '\n'.join(lines), b'prefix'+encoded+b'tail', plain


class RecorderTransformCheckTest(unittest.TestCase):
    def test_chunks_match_file_without_assumed_origin(self):
        text, saved, expected = fixture()
        report, plain = checker.verify(text,saved,2)
        self.assertTrue(report['consistent'])
        self.assertEqual(report['file_origin'],6)
        self.assertEqual(report['compared_bytes'],64)
        self.assertEqual(report['model_matches'],2)
        self.assertEqual(report['saved_chunk_matches'],2)
        self.assertEqual(report['state_variants'],1)
        self.assertEqual(plain,expected)
        self.assertFalse(report['native_audio_validated'])

    def test_per_call_state_variants_are_not_assumed_constant(self):
        text, saved, expected = fixture(True)
        report, plain = checker.verify(text,saved,2)
        self.assertTrue(report['consistent'])
        self.assertEqual(report['state_variants'],2)
        self.assertEqual(plain,expected)

    def test_disagreements_remain_research_results(self):
        text, saved, expected = fixture()
        mutated = bytearray(saved)
        mutated[6+32+12] ^= 1
        report, plain = checker.verify(text,mutated,2)
        self.assertFalse(report['consistent'])
        self.assertEqual(report['first_mismatched_samples'],[2])
        self.assertEqual(plain,expected)
        rows = text.splitlines()
        fields = rows[-1].split()
        fields = ['left=0000' if field.startswith('left=') else field for field in fields]
        report, _ = checker.verify('\n'.join(rows[:-1]+[' '.join(fields)]),saved,2)
        self.assertFalse(report['consistent'])
        self.assertEqual(report['model_matches'],1)
        report, _ = checker.verify(text.replace('cursor=00000020','cursor=00000021'),saved,2)
        self.assertFalse(report['cursor_step_32'])

    def test_rejects_incomplete_wrong_scope_and_ambiguous_origins(self):
        text, saved, _ = fixture()
        rows = text.splitlines()
        invalid = [
            '\n'.join(rows[1:]), '\n'.join(rows[:-1]), text+'\n'+rows[-1],
            '\n'.join(rows[:1]+rows[4:]+rows[1:4]),
            text.replace('target=6e9c','target=0001',1),
            text.replace('candidate_sp=6e8c','candidate_sp=6e8d',1),
            text.replace('001c,0004','001d,0004',1),
            text.replace('context_only=1','context_only=0',1),
            text.replace('ownership=unvalidated','ownership=claimed',1),
            text.replace('observer_writes=0','observer_writes=1',1),
            text.replace('context_pc=03f10d','context_pc=03f000',1),
            text.replace('t=1.100000000','t=0.100000000',1),
            text.replace('source_words=0000','source_words=0100',1),
            text.replace('caller=03ee95','caller=03ffff',1),
            text.replace('target=6e9c','target=6e9c target=6e9c',1),
        ]
        for index, bad in enumerate(invalid):
            with self.subTest(index=index), self.assertRaises((ValueError,KeyError)):
                checker.verify(bad,saved,2)
        for bad in (b'nothing',saved+saved,saved[:45]):
            with self.assertRaises(ValueError):
                checker.verify(text,bad,2)
        for samples in (0,97):
            with self.assertRaises(ValueError):
                checker.verify(text,saved,samples)

    def test_cli_is_read_only_and_reports_invalid_source(self):
        text, saved, _ = fixture()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            log, media, source = root/'trace.log',root/'TEST',root/'source.a00'
            log.write_text(text)
            media.write_bytes(saved)
            source.write_bytes(b'unit source')
            argv = [str(log),str(media),'--source-a00',str(source),'--samples','2']
            out, err = io.StringIO(), io.StringIO()
            with patch.object(checker,'contract',return_value={}), contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
                self.assertEqual(checker.main(argv),0)
            self.assertTrue(json.loads(out.getvalue())['consistent'])
            self.assertEqual(media.read_bytes(),saved)
            self.assertEqual(err.getvalue(),'')
            with contextlib.redirect_stderr(err):
                self.assertEqual(checker.main(argv),1)
            self.assertIn('MU4 recording transform observation:',err.getvalue())


if __name__ == '__main__':
    unittest.main()

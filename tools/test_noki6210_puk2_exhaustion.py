import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools import run_noki6210_puk2_exhaustion as runner


class RetainedEvidenceTests(unittest.TestCase):
    def exercise(self,drift=None):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            source=root/'source'
            target=root/'target'
            (source/'nvram/npe3hle').mkdir(parents=True)
            (source/'nvram/npe3hle/sim_card').write_bytes(b'card')
            (source/'nvram/npe3hle/flash').write_bytes(b'flash')
            target.mkdir()
            script=root/'input.lua'
            script.write_text('-- physical input\n')
            command=['mame','-autoboot_script',str(script),'-seconds_to_run','88']
            launch={'command':command.copy(),'run_directory':str(target),
                    'initial_storage':runner.storage(source),
                    'source_card_sha256':hashlib.sha256(b'card').hexdigest(),
                    'script_sha256':runner.digest(script),'invocation_id':'separate-process'}
            if drift=='command':
                launch['command'][-1]='1'
            elif drift=='storage':
                launch['initial_storage']['npe3hle/flash']='wrong'
            elif drift=='script':
                launch['script_sha256']='wrong'
            (target/'launch.json').write_text(json.dumps(launch))
            (target/'console.log').write_text('complete')
            (target/'error.log').write_text('events')
            with patch.object(runner,'verify') as verify, \
                    patch.object(runner,'events',return_value='events'), \
                    patch.object(runner,'check_output') as health, \
                    patch.object(runner,'run_retained_security') as execute:
                text,initial,result=runner.retained(source,target,command,script,88,True)
                self.assertEqual(initial,b'card')
                self.assertEqual(result['invocation_id'],'separate-process')
                self.assertIn('events',text)
                verify.assert_called_once()
                health.assert_called_once()
                execute.assert_not_called()

    def test_resume_revalidates_complete_evidence_without_relaunch(self):
        self.exercise()

    def test_resume_rejects_command_storage_and_script_drift(self):
        for drift in ('command','storage','script'):
            with self.subTest(drift=drift),self.assertRaisesRegex(ValueError,'provenance differs'):
                self.exercise(drift)


if __name__=='__main__':
    unittest.main()

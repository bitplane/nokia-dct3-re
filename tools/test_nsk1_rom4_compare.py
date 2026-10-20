import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.nsk1_rom4_compare import prepare,summarize


class Nsk1CompareTest(unittest.TestCase):
    def test_observation_checks_own_payload_and_does_not_claim_boot(self):
        image = bytearray(0x100000)
        payload = bytes(range(186))*2
        image[0x9c2f0:0x9c2f0+372] = payload
        image[0x9728c:0x97296] = bytes.fromhex('fd00ff80024405000078')
        trace = '\n'.join(f'[luaengine] nsk1_boot: t={time} pc=00289406 dsp_pc=408d illegal=0'
                          for time in (0.1,0.5,1,2,5))
        trace += '\n'+'\n'.join(f'nsk1_presentation: t={time} commands=20 data=504 nonzero=504 startup_reason=0a idle_flag=01 observer_writes=0'
                                for time in (0.1,0.5,1,2,5))
        release = ('[luaengine] nsk1_release: index=1 command=01 pc=00273504 '
                   f'args=0100030000008000000100010200 program={payload.hex()}')
        trace += '\n'+release
        report = summarize(trace,image,'standard')
        self.assertEqual(1,report['releases'])
        self.assertTrue(report['own_slot25_payload_observed'])
        self.assertFalse(report['descriptor0_release_observed'])
        self.assertFalse(report['graphical_or_fitted_hardware_proved'])
        self.assertEqual(504,report['lcd_nonzero_bytes'])
        decision = '\nnsk1_record_decision: pointer=00101bc8 state=00000002 valid=00000000 observer_writes=0'
        self.assertEqual([{'pointer':'00101bc8','state':2,'valid':0}],
                         summarize(trace+decision,image,'standard')['record_decisions'])
        second = '\nnsk1_release: index=2 command=01 pc=00273786 args=fd00ff8002440500007800000000 program=00'
        self.assertTrue(summarize(trace+second,image,'standard')['descriptor0_release_observed'])
        for bad in (trace.replace('00273504','00273506'),
                    trace.replace(payload.hex(),'00'),
                    trace.replace('t=5 ','t=4 '),
                    trace+'\n[LUA ERROR]',
                    trace.replace('nsk1_presentation: t=5','nsk1_presentation: t=6'),
                    trace+second.replace('fd00ff80','fd01ff80')):
            with self.assertRaises(ValueError):
                summarize(bad,image,'standard')
        self.assertTrue(summarize(trace.replace('illegal=0','illegal=1'),image,'standard')['native_illegal_observed'])

    def test_pinned_inputs_only_and_fresh_storage(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            sources = root / 'sources'
            sources.mkdir()
            inputs = []
            for index in range(4):
                data = bytes([index])*16
                name = f'input{index}.bin'
                (sources/name).write_bytes(data)
                inputs.append((name,hashlib.sha1(data).hexdigest()))
            run = root/'run'
            with patch('tools.nsk1_rom4_compare.INPUTS',inputs), \
                    patch('tools.nsk1_rom4_compare.inspect') as inspect:
                prepare(sources,run)
                self.assertEqual(['standard','2915'],[call.args[1] for call in inspect.call_args_list])
                self.assertFalse((run/'nvram').exists())
                manifest = json.loads((run/'input_manifest.json').read_text())
                self.assertFalse(manifest['peer_completions'])
                self.assertEqual(4,len(list((run/'inputs/nsk1r4t').iterdir())))
                with self.assertRaisesRegex(ValueError,'fresh'):
                    prepare(sources,run)
                (sources/'input3.bin').write_bytes(b'wrong mask')
                with self.assertRaisesRegex(ValueError,'hash'):
                    prepare(sources,root/'rejected')
                self.assertFalse((root/'rejected').exists())


if __name__ == '__main__':
    unittest.main()

from pathlib import Path
import unittest
from unittest.mock import patch

from tools import nsk3_puk1_exhaust as target
from tools.sim_security_trace_check import AOC_NVRAM_SIZE, ATTEMPTS_OFFSET


class Puk1ChainTest(unittest.TestCase):
    def run_chain(self, bad=False):
        seed=Path('seed')
        output=Path('output')

        def stored(run, name):
            if name=='flash':
                return b'image'
            card=bytearray(AOC_NVRAM_SIZE)
            count=9 if run==seed else int(run.name.split('-')[1])
            card[ATTEMPTS_OFFSET+2]=count+(1 if bad and count==4 else 0)
            return bytes(card)

        with patch.object(target, 'inspect', return_value=(None,b'image')), \
             patch.object(target, 'stored', side_effect=stored), \
             patch.object(target, 'trace_text', return_value='trace'), \
             patch.object(target, 'check_recovery') as recover, \
             patch.object(target, 'check_upload_handoffs'), \
             patch.object(target, 'check_link') as link, \
             patch.object(target, 'frame_check'), \
             patch.object(target, 'check_permanent') as permanent, \
             patch.object(target, 'execute') as execute, \
             patch.object(target, 'prepare') as prepare, \
             patch.object(target, 'compress_trace') as compress:
            result=target.chain(Path('source'),seed,output,check_only=True)
            self.assertEqual(output/'permanent-cold',result)
            permanent.assert_called_once_with(result,output/'remaining-0',b'image')
            self.assertEqual(10,recover.call_count)
            self.assertEqual(9,link.call_count)
            self.assertEqual(seed,link.call_args_list[0].args[1])
            self.assertEqual(output/'remaining-1',link.call_args_list[-1].args[1])
            execute.assert_not_called()
            prepare.assert_not_called()
            compress.assert_not_called()

    def test_all_nine_cold_links_are_checked_without_reexecution(self):
        self.run_chain()

    def test_skipped_counter_transition_is_rejected(self):
        with self.assertRaisesRegex(ValueError,'skipped'):
            self.run_chain(bad=True)

    def test_permanent_rejection_requires_unchanged_card_and_no_apdu(self):
        card=bytearray(AOC_NVRAM_SIZE)
        card[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET+4]=bytes((0,3,0,10))
        keys=(1,2,3,4,5,6,7,9,0x19)+(4,3,2,1,0x19)*2
        trace='\n'.join(f'nsk3_puk_contact: key={key:02x} down={edge} '
                        for key in keys for edge in (1,0))
        with patch.object(target,'check_link'), \
             patch.object(target,'stored',return_value=bytes(card)) as stored, \
             patch.object(target,'trace_text',return_value=trace) as text, \
             patch.object(target,'frame_check') as frames:
            target.check_permanent(Path('cold'),Path('previous'),b'image')
            self.assertEqual(2,frames.call_count)
            for bad in (trace+'\nSIM status ins=2c sw=9840 chv=0/3 puk=0/10 enabled=1',
                        trace.replace('key=19','key=17'),trace+'\nunhandled opcode'):
                text.return_value=bad
                with self.assertRaises(ValueError):
                    target.check_permanent(Path('cold'),Path('previous'),b'image')
            text.return_value=trace
            changed=bytearray(card)
            changed[-1]^=1
            stored.side_effect=[bytes(card),bytes(changed)]
            with self.assertRaises(ValueError):
                target.check_permanent(Path('cold'),Path('previous'),b'image')


if __name__=='__main__':
    unittest.main()

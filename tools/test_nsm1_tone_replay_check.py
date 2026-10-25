import unittest

from tools.nsm1_tone_replay_check import check,canonicalize


def fixture():
    fields=['t=86.200000000','mcu_PC=00000000','mcu_CPSR=00000000']
    fields += [f'mcu_R{i}=00000000' for i in range(16)]
    fields += ['dsp_'+name+'=0000000000' for name in
               ('PC','SP','ST0','ST1','A','B','T',*(f'AR{i}' for i in range(8)),'IFR','IMR')]
    fields += [f'data_{a:04x}=0000' for a in (0x21,0x22,0x23,*range(0xf0,0xff))]
    state=' '.join(fields)
    rows=[]
    for i in range(400):
        ns=86_200_062_500+i*125000
        rows.append(f'native_pcm_fixture_word: value={"0092" if i%2 else "00b8"} t={ns//10**9}.{ns%10**9:09d}\n')
    stream=''.join(rows)
    return (f'nsm1_pcm_restore: phase=saved {state}\n'+stream+
            f'nsm1_pcm_restore: phase=restored {state}\n'+stream+
            'nsm1_pcm_restore: phase=complete t=86.250000000 physical_release_replayed=0\n')


class ReplayTest(unittest.TestCase):
    def test_exact_active_interval(self):
        self.assertEqual(check(fixture())['replayed_words'],400)

    def test_changes_and_observer_failures_are_rejected(self):
        good=fixture()
        for bad in (good.replace('value=0092','value=0055',1),
                    good.replace('mcu_R0=00000000','mcu_R0=00000001',1),
                    good.replace('86.200062500','86.200062501',1),
                    good+'[LUA ERROR] failed\n',
                    good.replace('phase=restored','phase=saved')):
            with self.subTest(prefix=bad[:80]),self.assertRaises(ValueError):
                check(bad)

    def test_accounting_cannot_ignore_missing_summary_or_activity(self):
        with self.assertRaises(ValueError):
            canonicalize(fixture())
        state=fixture().replace('native_pcm_fixture_word:',
            'nsm1_tone_boundary_mcu: address=00010000\nnative_pcm_fixture_word:')
        with self.assertRaises(ValueError):
            canonicalize(state)


if __name__=='__main__':
    unittest.main()

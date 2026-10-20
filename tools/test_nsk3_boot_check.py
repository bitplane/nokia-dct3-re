import unittest
from tools.nsk3_boot_check import check_trace


class Nsk3BootTest(unittest.TestCase):
    def test_bounded_failure_not_boot_acceptance(self):
        for variant,pc in (('boot4','002a02fc'),('boot3','002a02ec')):
            text='\n'.join(f'nsk3_boot_sample: t={t}.000000 pc={pc} sp=0010c6fc ctrl=05 clock=0c fiq=00 irq=00 shared=0000,ffff,ffff,ffff writes=12 observer_writes=0' for t in range(1,17))
            check_trace(text,variant)
            for bad in (text.split('\n',1)[1],text.replace('ctrl=05','ctrl=0a'),text.replace('ffff,ffff,ffff','0000,0000,0000'),text.replace('writes=12','writes=13')):
                with self.assertRaises(ValueError):check_trace(bad,variant)


if __name__=='__main__':unittest.main()

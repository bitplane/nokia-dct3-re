import unittest
from tools.nsk3_boot_check import check_trace,check_native_trace


class Nsk3BootTest(unittest.TestCase):
    def test_native_upload_requires_both_complete_passes(self):
        lines=[]
        for index in range(1,129):
            odd=index%2==1
            pc='0028cef6' if index%64==0 else '0028cea8' if odd else '0028ceb8'
            time=0.01+((index-1)%64+1)*0.001+((index-1)//64)*0.4
            lines.append(f"nsk3_loader_handoff: index={index} offset={'000100fc' if odd else '00010100'} value=00000000 mask={'0000ffff' if odd else 'ffff0000'} pc={pc} t={time:.6f} observer_writes=0")
        for base in (0,0.4):
            for pc,offset,value,mask,time in (
                    ('0028ce70','0010b840','00040000','ffff0000',0.005),
                    ('0028cf0a','0010b844','00000004','0000ffff',0.08),
                    ('0028cf0e','0010b844','00000000','ffff0000',0.081)):
                lines.append(f'nsk3_loader_state: offset={offset} value={value} mask={mask} pc={pc} t={base+time:.6f} observer_writes=0')
        lines.extend(f'nsk3_native_sample: t={i}.000000 pc=408d releases=3 handoffs=128 observer_writes=0' for i in range(1,17))
        lines.append('nsk3_reset_request: command=05 pc=002ac696 t=0.200000 reason=04 observer_writes=0')
        trace='\n'.join(lines)
        check_native_trace(trace)
        for bad in (trace.split('\n',1)[1],trace.replace('index=64 ','index=63 '),
                    trace.replace('pc=0028cf0a','pc=0028cf0b'),trace.replace('pc=408d','pc=ffff'),
                    trace.replace('t=0.200000','t=0.900000'),trace+'\nunsupported opcode',
                    trace+'\nnsk3_loader_handoff: malformed'):
            with self.assertRaises(ValueError):check_native_trace(bad)

    def test_bounded_failure_not_boot_acceptance(self):
        for variant,pc in (('boot4','002a02fc'),('boot3','002a02ec')):
            text='\n'.join(f'nsk3_boot_sample: t={t}.000000 pc={pc} sp=0010c6fc ctrl=05 clock=0c fiq=00 irq=00 shared=0000,ffff,ffff,ffff writes=12 observer_writes=0' for t in range(1,17))
            check_trace(text,variant)
            for bad in (text.split('\n',1)[1],text.replace('ctrl=05','ctrl=0a'),text.replace('ffff,ffff,ffff','0000,0000,0000'),text.replace('writes=12','writes=13')):
                with self.assertRaises(ValueError):check_trace(bad,variant)


if __name__=='__main__':unittest.main()

import unittest
from tools.nsk3_packet_check import check_native_control, check_native_scan, SCAN_FRAGMENTS


class NativeControlTest(unittest.TestCase):
    def test_live_scan_code_counter_and_queue(self):
        program=bytearray(131072)
        for address,raw in SCAN_FRAGMENTS.items():
            data=bytes.fromhex(raw);program[address*2:address*2+len(data)]=data
        rows=[]
        for i,time in enumerate((5,20,40,45,65,90)):
            rows.append(f'nsk3_native_scan_sample: t={time}.0 counter={0xffff0000+i:08x} qualified=0000 flags=0000 producer=1a00 consumer=1a00 threshold=0574 observer_writes=0')
        trace='\n'.join(rows)
        self.assertEqual(check_native_scan(trace,program)['qualifying_results'],0)
        for old,new in (('t=90.0','t=89.0'),('qualified=0000','qualified=0001'),
                        ('consumer=1a00','consumer=1a04'),('counter=ffff0005','counter=ffff0001')):
            with self.subTest(old=old),self.assertRaises(ValueError):check_native_scan(trace.replace(old,new),program)
        for address in SCAN_FRAGMENTS:
            changed=bytearray(program);changed[address*2]^=1
            with self.assertRaises(ValueError):check_native_scan(trace,changed)

    def fixture(self):
        rows=[]
        pending=[0,1,2,8,3]
        completed=[0,2,6,0x106,0x10e]
        for i,(p,c) in enumerate(zip(pending,completed)):
            rows.append(f'nsk3_native_control_write: address=06d0 value={p:04x} pc=3193 t={i}.0 observer_writes=0')
            rows.append(f'nsk3_native_control_write: address=06d1 value={c:04x} pc=31ee t={i}.1 observer_writes=0')
        expected={f'{a:04x}':'1234'*8 for a in (0x3000,0x313f,0x47f6,0x4a49)}
        rows.extend(f'nsk3_native_live_words: address={a} words={v} observer_writes=0' for a,v in expected.items())
        rows.extend(f'nsk3_native_control_sample: t={t}.0 pending=0003 completed=010e control06e3=0000 state1949=0000 writes_pending=5 writes_completed=5 cap=64 observer_writes=0' for t in (5,20,40,45))
        return '\n'.join(rows),expected

    def test_control_lifecycle_and_live_code(self):
        trace,expected=self.fixture()
        self.assertEqual(check_native_control(trace,expected)['request_indexes'],[1,2,8,3])

    def test_changed_control_code_cap_or_missing_sample(self):
        trace,expected=self.fixture()
        for old,new in (('value=0008','value=0004'),('completed=010e','completed=011e'),
                        ('writes_pending=5','writes_pending=64'),('t=45.0','t=44.0'),
                        ('words=1234','words=1235'),('address=06d0','address=06d2')):
            with self.subTest(old=old),self.assertRaises(ValueError):
                check_native_control(trace.replace(old,new),expected)


if __name__=='__main__':unittest.main()

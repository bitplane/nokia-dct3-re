"""External BTS normal-bit decoder against independent control coding."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from tools.gsm_xcch_signal import normal_control_bursts
from tools.gsm_normal_signal import TSC0


class NormalP27CppTest(unittest.TestCase):
    def test_independent_blocks_corruptions_and_contention_reply(self):
        compiler=shutil.which('c++')
        if not compiler:self.skipTest('C++ compiler unavailable')
        root=Path(__file__).resolve().parents[1]
        sabm=bytes.fromhex('013f4905087000f000fffe330809101010325476982b2b')
        def serialize(payload):
            bursts=[]
            for raw in normal_control_bursts(bytes(payload)):
                bits=list(raw);bits[61:87]=TSC0
                lane=[0]*9+bits+[1]*23
                bursts.append([sum(lane[word*12+bit]<<bit for bit in range(12)) for word in range(15)])
            return bursts
        cases=[]
        for payload in (bytes(23),bytes([255])*23,bytes(range(23)),sabm):
            expected=payload.hex()+' '+(sabm[:1]+b'\x73'+sabm[2:]).hex() if payload==sabm else payload.hex()+' none'
            cases.append((0,serialize(payload),expected))
        good=cases[-1][1]
        for bit in (9,12,69,70,95,96,154):
            bad=[row.copy() for row in good]
            bad[0][bit//12]^=1<<(bit%12)
            cases.append((0,bad,'invalid'))
        cases.extend((tsc,good,'invalid') for tsc in (1,8))
        # Re-encode malformed SABM headers: FEC/FIRE remain valid,but no UA.
        for position,value in ((0,3),(1,0x2f),(2,0),(2,0x55),(2,0x4b)):
            payload=bytearray(sabm);payload[position]=value
            cases.append((0,serialize(payload),payload.hex()+' none'))
        disc=bytes.fromhex('015301')+b'\x2b'*20
        cases.append((0,serialize(disc),disc.hex()+' '+(b'\x01\x73\x01'+b'\x2b'*20).hex()))
        for position,value in ((0,3),(1,0x43),(2,5)):
            bad=bytearray(disc);bad[position]=value
            cases.append((0,serialize(bad),bad.hex()+' none'))
        stdin='\n'.join(str(tsc)+' '+' '.join(f'{word:x}' for burst in bursts for word in burst)
                        for tsc,bursts,_ in cases)
        with tempfile.TemporaryDirectory() as directory:
            binary=Path(directory)/'normal-p27'
            subprocess.run([compiler,'-std=c++17','-O2','-Wall','-Wextra','-Werror',
                            str(root/'tests/unit/gsm_normal_p27_probe.cpp'),
                            str(root/'driver/gsm_tch_f_l1.cpp'),'-o',str(binary)],
                           check=True,capture_output=True,text=True)
            output=subprocess.run([str(binary)],input=stdin,check=True,capture_output=True,text=True).stdout.splitlines()
        self.assertEqual(output,[expected for _,_,expected in cases])


if __name__=='__main__':unittest.main()

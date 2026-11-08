"""Native word layout checked against independently constructed GSM FIRE vectors."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path
from tools.gsm_xcch_signal import control_bits


class FireStateTest(unittest.TestCase):
    def test_encoder_readback_matches_independent_parity_and_restores_cursor(self):
        compiler=shutil.which('c++')
        if not compiler:self.skipTest('C++ compiler unavailable')
        root=Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as directory:
            binary=Path(directory)/'fire-encode'
            subprocess.run([compiler,'-std=c++17','-Wall','-Wextra','-Werror',
                '-I',str(root/'driver'),str(root/'tests/unit/test_dsp_fire_encode.cpp'),'-o',str(binary)],
                check=True,capture_output=True)
            for payload in (bytes(23),bytes([255])*23,bytes(range(23)),
                            bytes.fromhex('013f4905087000f000fffe330809101010325476982b2b')):
                bits=control_bits(payload)
                words=[int.from_bytes((payload+b'\0')[at:at+2],'little') for at in range(0,24,2)]
                raw=[bit^1 for bit in bits[184:224]]
                output=[sum(raw[at+bit]<<bit for bit in range(16)) for at in (0,16)]
                output.append(sum(raw[32+bit]<<(8+bit) for bit in range(8)))
                subprocess.run([str(binary)],input=b''.join(word.to_bytes(2,'little') for word in words+output),
                               check=True,capture_output=True)

    def test_polynomial_transaction_and_restore_state(self):
        compiler=shutil.which('c++')
        if not compiler:self.skipTest('C++ compiler unavailable')
        root=Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as directory:
            binary=Path(directory)/'fire'
            subprocess.run([compiler,'-std=c++17','-Wall','-Wextra','-Werror',
                '-I',str(root/'driver'),str(root/'tests/unit/test_dsp_fire_state.cpp'),'-o',str(binary)],
                check=True,capture_output=True)
            for payload in (bytes(23),bytes([255])*23,bytes.fromhex('49061b000100f11000014000000000000000002b2b2b2b')):
                bits=control_bits(payload)
                bits=bits[:184]+tuple(bit^1 for bit in bits[184:224])
                words=[sum(bit<<(index%16) for index,bit in enumerate(bits) if index//16==word) for word in range(14)]
                subprocess.run([str(binary)],input=b''.join(word.to_bytes(2,'little') for word in words),
                    check=True,capture_output=True)


if __name__=='__main__':unittest.main()

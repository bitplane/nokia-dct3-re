import unittest
from unittest.mock import patch

from tools.mu4_resident_int2_restore_check import REGISTERS, check


def fixture():
    lines=[]
    registers={name:0 for name in REGISTERS}
    registers.update(IDLE=1,IMR=4,GENPC=0x28923)
    cpu=','.join(f'{name}:{registers[name]:010x}' for name in REGISTERS)
    for phase,state in [('reference','saved'),('restored','restored')]:
        lines.append(f'mu4_wake_restore_state: phase={state} t=10.950000000 cpu={cpu} ram=12345678 program=abcdef12 data_words=65408 program_bytes=16777216 observer_writes=0')
        for index,(address,value) in enumerate([('0001','0438'),('0000','0ac5')]):
            lines.append(f'mu4_wake_replay_event: phase={phase} index={index} t=11.020000000 address={address} value={value} mask=ffff observer_writes=0')
        for asserted,time in [(1,'11.000000000'),(0,'11.050000000')]:
            lines.append(f'mu4_wake_replay_input: phase={phase} asserted={asserted} t={time} source=bench_pin firmware_state_forcing=0')
            lines.append(f'mu4_mcu_int2_input: asserted={asserted} net=UC_DSP_INT cpu_irq=2 t={time} source=bench_pin firmware_state_forcing=0')
    lines.append('mu4_wake_replay: result=pass reference_events=2 restored_events=2 window=0.250000000 cap=2048 observer_writes=0 normal_mcu_waveform=unproved audio_acceptance=0')
    lines.append('mu4_wake_restore_terminal: complete=1 observer_writes=0 audio_acceptance=0')
    return '\n'.join(lines)


class WakeRestoreTests(unittest.TestCase):
    def run_check(self,text):
        with patch('tools.mu4_resident_int2_restore_check.serial_check') as serial:
            result=check(text,'console')
            serial.assert_called_once_with(text,'console')
            return result

    def test_valid_replay(self):
        result=self.run_check(fixture())
        self.assertEqual(result['registers'],36)
        self.assertEqual(result['events'],2)
        self.assertFalse(result['audio_completion'])

    def test_scope_mutations_fail(self):
        original=fixture()
        for old,new in [('IDLE:0000000001','IDLE:0000000000'),
                        ('IMR:0000000004','IMR:0000000ac5'),
                        ('value=0438','value=0000'),('cap=2048','cap=2'),
                        ('data_words=65408','data_words=1'),
                        ('normal_mcu_waveform=unproved','normal_mcu_waveform=proved'),
                        ('cpu_irq=2','cpu_irq=6'),
                        ('complete=1','complete=0'),('observer_writes=0','observer_writes=1')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                self.run_check(original.replace(old,new))

    def test_replay_timing_and_checksum_drift_fail(self):
        original=fixture()
        for old,new in [('phase=restored index=0 t=11.020000000','phase=restored index=0 t=11.021000000'),
                        ('phase=restored asserted=1 t=11.000000000','phase=restored asserted=1 t=11.010000000'),
                        ('phase=restored t=10.950000000','phase=restored t=10.950000001')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                self.run_check(original.replace(old,new))


if __name__=='__main__':
    unittest.main()

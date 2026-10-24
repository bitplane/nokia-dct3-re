from pathlib import Path
import unittest

from tools import nse5_v500_serial_check as checker
from tools.c54x_rom4_codec_contract import SEQUENCES
from tools.nse5_fullbackup_check import observation


def trace():
    lines = ['nse5_v500_low_checksum: computed=1988 stored=00001988 t=0.8 observer_writes=0',
             'nse5_v500_low_checksum: computed=1988 stored=00001988 t=1.7 observer_writes=0']
    for site,args,caller,time in (
            ('003d6efe',('00000009','00000004','00000001','00000001'),'003d72b9','2.2177'),
            ('003d6e9c',('00000000','00000000','00000000','00000001'),'003d72c1','2.2178'),
            ('003e3b3a',('00000000','00000000','00000000','00000000'),'003d6ec7','2.2179'),
            ('003e35a8',('00000000','00000001','00000000','00101a84'),'003e3661','2.2207')):
        registers = ' '.join(f'r{i}={value}' for i,value in enumerate(args))
        lines.append(f'nse5_v500_entry: site={site} index=1 {registers} caller={caller} t={time} observer_writes=0')
    lines += ['nse5_v500_control_post: bytes=0002044c0000010a caller=003e35d5 t=2.2208 observer_writes=0',
              'nse5_v500_serial_state: flags=0000 pending=0800 timing=0876,0015 rt0=9258 rt1=3bcf t=2.2249 observer_writes=0']
    for name in ('serial_prepare_hook_defaults','serial_prepare_state'):
        address,words = SEQUENCES[name]
        lines.append(f'rom4_serial_live_code: address={address:04x} words=' + ','.join(f'{word:04x}' for word in words))
    for value,pc in [('0080','8d98'),('0080','90bc')]*2 + [('809c','9132')]:
        lines.append(f'rom4_port36: direction=write value={value} pc={pc} t=2.2249')
    lines += ['rom4_port36: direction=read value=809c pc=9140 t=2.2250']*4
    for direction,value,pc in (('read','001d','924c'),('write','000d','9251'),
                               ('read','000d','9135'),('write','001d','913a')):
        lines.append(f'rom4_serial_port: port=00 direction={direction} value={value} pc={pc} t=2.2249')
    for site in ('3bcd','9232','9129','9140','9009','23e4','23e6','9144'):
        lines.append(f'rom4_port36_count: site={site} count={0 if site == "9144" else 1}')
    return '\n'.join(lines)


class Nse5V500SerialTest(unittest.TestCase):
    def test_own_code_fragments_and_bounded_caller_inventory(self):
        data = bytearray(0x400000)
        data[0x310000:0x310000+len(checker.PPM_PREFIX)] = checker.PPM_PREFIX
        for address,encoded in checker.FRAGMENTS.items():
            values = bytes.fromhex(encoded)
            data[address-0x200000:address-0x200000+len(values)] = values
        result = checker.inspect(data)
        self.assertEqual(set(result), {'3e35a8','3e3b3a','3d6e9c','4d7f12'})
        for address in checker.FRAGMENTS:
            bad = bytearray(data)
            bad[address-0x200000] ^= 1
            with self.subTest(address=address), self.assertRaises(ValueError):
                checker.inspect(bad)
        source = Path(__file__).resolve().parents[1] / 'roms/research/nse5-cosconor/My 7110 v5.00.fls'
        if source.exists():
            calls = checker.inspect(source.read_bytes())
            self.assertEqual([len(calls[key]) for key in ('3e35a8','3e3b3a','3d6e9c','4d7f12')], [4,8,4,4])

    def test_live_checksum_own_producer_and_real_pending_port(self):
        result = checker.check_trace(trace())
        self.assertEqual(result['control_payload'], '0000,010a')
        self.assertFalse(result['completion_observed'])
        self.assertFalse(result['physical_port_identity_proved'])
        # r3 is observed but is not an argument of the two-argument wrapper.
        own = trace().replace('r3=00000001 caller=003d72c1', 'r3=ffffffff caller=003d72c1')
        checker.check_trace(own)

    def test_sibling_values_missing_controls_and_completion_cannot_pass(self):
        text = trace()
        for bad in (text.replace('0002044c0000010a','0002044c00800104'),
                    text.replace('pending=0800','pending=0880'),
                    text.replace('timing=0876','timing=0362'),
                    text.replace('computed=1988','computed=1987'),
                    text.replace('caller=003e35d5','caller=003e35d3'),
                    text.replace('value=809c','value=c09c'),
                    text.replace('site=9144 count=0','site=9144 count=1'),
                    text.replace('direction=write value=000d pc=9251','direction=write value=001d pc=9251'),
                    text + '\nrom4_port36_count: site=9140 count=2',
                    text + '\nnse5_v500_control_post: malformed',
                    text + '\n[LUA ERROR] failed'):
            with self.subTest(tail=bad[-100:]), self.assertRaises(ValueError):
                checker.check_trace(bad)

    def test_short_serial_window_does_not_weaken_fifty_second_observation(self):
        text = '\n'.join(f'nse5_full_sample: index={i} t={time}.000000000 arm=0049f970 sp=0016bf78 dsp=00913e stopped=0 idle=0 observer_writes=0'
                         for i,time in enumerate((1,2,4,5),1))
        result = observation(text,0,(1,2,4,5))
        self.assertFalse(result['graphical_boot_verified'])
        with self.assertRaises(ValueError):
            observation(text,0)
        with self.assertRaises(ValueError):
            observation(text.replace('t=5.', 't=4.'),0,(1,2,4,5))


if __name__ == '__main__':
    unittest.main()

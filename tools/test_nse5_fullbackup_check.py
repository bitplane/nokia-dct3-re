import hashlib
from pathlib import Path
import unittest

from tools import nse5_fullbackup_check as checker


def image():
    data = bytearray(b'\xff' * 0x400000)
    sector = bytearray(b'\xff' * 0x2000)
    sector[:12] = bytes.fromhex('f0f0fff80001') + b'EEPROM'
    sector[0x18:0x1a] = b'\x00\x01'
    sector[0x20:0x26] = bytes.fromhex('0400000042ff')
    data[0x3fc000:0x3fe000] = sector
    return bytes(data)


def samples():
    return '\n'.join(f'nse5_full_sample: index={i} t={t}.000000000 arm=0049fff6 sp=0016bf74 dsp=00408d stopped=0 idle=1 observer_writes=0'
                     for i,t in enumerate((1,2,4,8,16,24,36,48), 1))


class Nse5FullBackupTest(unittest.TestCase):
    def test_machine_labels_are_normalized_without_renaming_acquired_sources(self):
        self.assertEqual(checker.IMAGES['500-full'][0], 'My 7110 v5.00.fls')
        self.assertEqual(checker.IMAGES['501d-full'][0], 'FuBu7110-v501-PPM-D.fls')
        for bios, label in checker.ROM_LABELS.items():
            with self.subTest(bios=bios):
                self.assertRegex(label, r'^[a-z0-9_.]+$')
                self.assertEqual(checker.capture_rom_label(bios, {'rom_label': label}), label)
                self.assertEqual(checker.capture_rom_label(bios, {}), checker.IMAGES[bios][0])
                for bad in ('../' + label, checker.IMAGES[bios][0], '', 'unknown.fls'):
                    with self.assertRaises(ValueError):
                        checker.capture_rom_label(bios, {'rom_label': bad})
        with self.assertRaises(ValueError):
            checker.capture_rom_label('500-full', {'rom_label': checker.ROM_LABELS['501d-full']})

    def test_complete_image_and_independent_journal_description(self):
        data = image()
        result = checker.describe(data, data[:0x390000])
        self.assertTrue(result['matches_acquired_v501_mcu'])
        self.assertEqual(result['eeprom_sectors'][0]['address'], '0x5fc000')
        self.assertEqual(result['eeprom_sectors'][0]['writes'], 1)
        self.assertFalse(result['silicon_pairing_proved'])
        changed = bytearray(data)
        changed[0x20] ^= 1
        self.assertFalse(checker.describe(changed, data[:0x390000])['matches_acquired_v501_mcu'])
        for bad in (data[:-1], b'\xff'*len(data)):
            with self.assertRaises(ValueError):
                checker.describe(bad, data[:0x390000])

    def test_unknown_journal_forms_are_not_guessed(self):
        data = bytearray(image())
        for offset, value in ((0x3fc019, 2), (0x3fc020, 0x05), (0x3fc022, 0x10)):
            bad = bytearray(data)
            bad[offset] = value
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                checker.describe(bad, data[:0x390000])

    def test_runtime_window_is_complete_without_promoting_negative_results(self):
        result = checker.observation(samples(), 0)
        self.assertFalse(result['graphical_boot_verified'])
        self.assertFalse(result['native_registration_verified'])
        result = checker.observation(samples().replace('stopped=0', 'stopped=1'), 0)
        self.assertTrue(all(row['stopped'] for row in result['samples']))
        self.assertFalse(result['graphical_boot_verified'])
        for bad in (samples().replace('index=8', 'index=7'),
                    samples().replace('t=48.', 't=47.'),
                    samples() + '\nnse5_full_sample: malformed',
                    samples() + '\n[LUA ERROR] failed'):
            with self.assertRaises(ValueError):
                checker.observation(bad, 0)
        with self.assertRaises(ValueError):
            checker.observation(samples(), 1)

    def test_runtime_cache_and_raw_sector_are_independently_correlated(self):
        data = image()
        cache = b'\x42' + bytes(0x897)
        text = (f'nse5_full_cache: index=1 source=005fc000 bytes={cache.hex()} observer_writes=0\n'
                f'nse5_full_sector: index=1 bytes={data[0x3fc000:0x3fe000].hex()} observer_writes=0')
        result = checker.check_v501_cache(data, text)
        self.assertEqual(result['cache_sha256'], [hashlib.sha256(cache).hexdigest()])
        for bad in (text.replace('source=005fc000', 'source=005fa000'),
                    text.replace('bytes=42', 'bytes=43'),
                    text.replace('index=1', 'index=2'),
                    text + '\nnse5_full_cache: malformed',
                    text.replace('bytes=f0f0', 'bytes=f1f0')):
            with self.assertRaises(ValueError):
                checker.check_v501_cache(data, bad)

    def test_acquired_whole_inputs_when_available(self):
        root = Path(__file__).resolve().parents[1]
        directory = root / 'roms/research/nse5-cosconor'
        if not directory.exists():
            self.skipTest('optional acquired backups not present')
        result = checker.audit(directory, root / 'roms')
        self.assertTrue(result['501d-full']['matches_acquired_v501_mcu'])
        self.assertFalse(result['500-full']['matches_acquired_v501_mcu'])
        self.assertEqual([result[b]['eeprom_sectors'][0]['writes'] for b in checker.IMAGES], [57, 87])

    def test_v501_boundary_requires_positive_consumers_and_actual_flag_writes(self):
        lines = []
        for name, pc, count in (('service_init','003bc2a8',2), ('identity_check','003af540',2),
                                ('context_result','003ae594',1), ('subsystem_skip','0049fe36',1),
                                ('selftest_reply','0030e7a6',1)):
            lines += [f'nse5_full_boundary: name={name} pc={pc} index={i} t=1.0 observer_writes=0'
                      for i in range(1,count+1)]
        lines += ['nse5_full_failed_item: item=12 value=12 t=1.0 observer_writes=0']*2
        lines += ['nse5_full_selftest: index=1 bytes=00000074000401000d000000 observer_writes=0']
        lines += [f'nse5_full_flags: index={i} data={"8c" if i in (7,13) else "00"} pc={"0030e63e" if i in (7,13) else "004f080a"} t=1.0 observer_writes=0'
                  for i in range(1,16)]
        text = '\n'.join(lines)
        result = checker.check_v501_boundary(text)
        self.assertFalse(result['second_task_batch_released'])
        for bad in (text.replace('name=selftest_reply','name=missing'),
                    text.replace('item=12 value=12','item=0c value=0c'),
                    text.replace('0d000000','0d010000'),
                    text.replace('pc=0030e63e','pc=0030e63c'),
                    text+'\nnse5_full_boundary: malformed'):
            with self.assertRaises(ValueError):
                checker.check_v501_boundary(bad)


if __name__ == '__main__':
    unittest.main()

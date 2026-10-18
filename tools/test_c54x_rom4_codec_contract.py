import unittest
from unittest.mock import patch

from tools.c54x_rom4_codec_contract import (SEQUENCES, SCHEDULE_LISTS, check,
                                          UPLOADED_COUNTER_BLOCKS, check_mcu_upload_counters,
                                          check_schedule_data, check_trace, check_restore, check_tone)
from tools.c54x_rom4_codec_contract import MCU_SPEECH_FIELD_BYTES, check_mcu_speech_field
from tools.c54x_rom4_codec_contract import decode_mcu_speech_descriptor
from tools.c54x_rom4_codec_contract import decode_host_dispatch_mask
from tools.c54x_rom4_codec_contract import census_mcu_host_interface
from tools.c54x_rom4_codec_contract import check_live_serial_helpers


class CodecContractTests(unittest.TestCase):
    def test_live_serial_helpers_require_complete_original_words(self):
        rows = []
        for name in ('serial_prepare_hook_defaults', 'serial_prepare_state'):
            address, words = SEQUENCES[name]
            rows.append(f"rom4_serial_live_code: address={address:04x} words=" +
                        ','.join(f'{word:04x}' for word in words))
        text = '\n'.join(rows)
        self.assertEqual(check_live_serial_helpers(text), 2)
        for changed in (rows[0], text + '\n' + rows[0], text + '\nrom4_serial_live_code: broken',
                        text.replace('address=9009', 'address=900a'),
                        text.replace('fc00', 'f495'), text.replace('f7bb', 'f6bb')):
            with self.assertRaises(ValueError):
                check_live_serial_helpers(changed)
        for name in ('serial_prepare_hook_defaults', 'serial_prepare_state'):
            address, words = SEQUENCES[name]
            for index in range(len(words)):
                changed = list(words)
                changed[index] ^= 1
                source = ','.join(f'{word:04x}' for word in words)
                target = ','.join(f'{word:04x}' for word in changed)
                with self.subTest(name=name, index=index), self.assertRaises(ValueError):
                    check_live_serial_helpers(text.replace(source, target))

    def test_host_mmio_census_byte_lanes_and_scope(self):
        with patch('tools.mad2_static_census.analyze_image', return_value=([{'pc': 0x272048}], {'literal_seeds': 1})) as analyze:
            result = census_mcu_host_interface(bytes.fromhex('1234abcd'))
        analyze.assert_called_once_with(bytes.fromhex('3412cdab'), window_base=0x30000,
                                         window_size=4, constant_indexes=True)
        self.assertEqual(result['accesses'], [{'pc': 0x272048}])
        self.assertEqual(result['coverage'], {'literal_seeds': 1})
        self.assertIn('excluded', result['scope'])
        with self.assertRaises(ValueError):
            census_mcu_host_interface(bytes(3))
    def test_host_dispatch_mask_and_every_word_mutation(self):
        image = self.image()
        self.assertEqual(decode_host_dispatch_mask(image), 0x17ff)
        address, words = SEQUENCES['host_command_dispatch']
        for index in range(len(words)):
            changed = bytearray(image)
            changed[2 * (address + index)] ^= 1
            with self.subTest(index=index), self.assertRaises(ValueError):
                decode_host_dispatch_mask(changed)
        with self.assertRaises(ValueError):
            decode_host_dispatch_mask(image[:address * 2])
    def test_speech_descriptor_metadata_and_bounds(self):
        image = bytearray(0xa6460)
        for address, data in MCU_SPEECH_FIELD_BYTES.items():
            offset = address - 0x200000
            image[offset:offset + len(data)] = data
        self.assertEqual(decode_mcu_speech_descriptor(image),
                         {"identifier": 0x301, "offset": 0x210, "length": 4,
                          "record_address": 0x2a6110})
        for address, replacement in ((0x28950c, bytes.fromhex("00100000")),
                                     (0x2a6454, bytes(2)),
                                     (0x2a6110, bytes(4))):
            changed = bytearray(image)
            offset = address - 0x200000
            changed[offset:offset + len(replacement)] = replacement
            with self.subTest(address=address), self.assertRaises(ValueError):
                decode_mcu_speech_descriptor(changed)
        with self.assertRaises(ValueError):
            decode_mcu_speech_descriptor(image[:0xa6454])

    def test_mcu_speech_field_bytes_and_every_byte_mutation(self):
        image = bytearray(max(address - 0x200000 + len(data)
                              for address, data in MCU_SPEECH_FIELD_BYTES.items()))
        for address, data in MCU_SPEECH_FIELD_BYTES.items():
            offset = address - 0x200000
            image[offset:offset + len(data)] = data
        self.assertEqual(check_mcu_speech_field(image), len(MCU_SPEECH_FIELD_BYTES))
        for address, data in MCU_SPEECH_FIELD_BYTES.items():
            for index in range(len(data)):
                with self.subTest(address=address, index=index):
                    changed = bytearray(image)
                    changed[address - 0x200000 + index] ^= 1
                    with self.assertRaises(ValueError):
                        check_mcu_speech_field(changed)

    def test_original_upload_counter_blocks(self):
        image = bytearray(0x2000)
        for address, words in UPLOADED_COUNTER_BLOCKS.values():
            for index, word in enumerate(words):
                start = (address + index) * 2
                image[start:start + 2] = word.to_bytes(2, "big")
        self.assertEqual(len(check_mcu_upload_counters(image)), 3)
        for name, (address, words) in UPLOADED_COUNTER_BLOCKS.items():
            for index in range(len(words)):
                with self.subTest(name=name, index=index):
                    changed = bytearray(image)
                    changed[(address + index) * 2] ^= 1
                    with self.assertRaises(ValueError):
                        check_mcu_upload_counters(changed)
        pattern = b"".join(word.to_bytes(2, "big") for word in next(iter(UPLOADED_COUNTER_BLOCKS.values()))[1])
        with self.assertRaises(ValueError):
            check_mcu_upload_counters(image + pattern)

    def test_schedule_data_words(self):
        image = bytearray(0x20000)
        for address, words in SCHEDULE_LISTS.items():
            for offset, word in enumerate(words):
                start = 2 * (address + offset)
                image[start:start + 2] = word.to_bytes(2, "big")
        self.assertEqual(check_schedule_data(image), len(SCHEDULE_LISTS))
        for address, words in SCHEDULE_LISTS.items():
            for offset in range(len(words)):
                with self.subTest(address=address, offset=offset):
                    mutated = bytearray(image)
                    mutated[2 * (address + offset)] ^= 1
                    with self.assertRaisesRegex(ValueError, f"{address:04x}"):
                        check_schedule_data(mutated)

    def test_schedule_data_requires_complete_image(self):
        for image in (b"", bytes(0x1ffff), bytes(0x20001)):
            with self.assertRaisesRegex(ValueError, "complete"):
                check_schedule_data(image)

    def image(self):
        image = bytearray(0x10000 * 2)
        for address, words in SEQUENCES.values():
            for offset, word in enumerate(words):
                start = 2 * (address + offset)
                image[start:start + 2] = word.to_bytes(2, "big")
        return image

    def test_reviewed_sequences(self):
        self.assertEqual(check(self.image()), list(SEQUENCES))

    def test_every_word_is_checked(self):
        for name, (address, words) in SEQUENCES.items():
            for offset in range(len(words)):
                with self.subTest(sequence=name, offset=offset):
                    image = self.image()
                    image[2 * (address + offset)] ^= 1
                    with self.assertRaisesRegex(ValueError, name):
                        check(image)

    def test_truncation(self):
        with self.assertRaises(ValueError):
            check(self.image()[:0x100])

    def test_partial_word(self):
        with self.assertRaisesRegex(ValueError, "complete"):
            check(b"\x00")

    def test_unreviewed_reader_rejected(self):
        image = self.image()
        image[0x200:0x206] = bytes.fromhex("74f800080021")
        with self.assertRaisesRegex(ValueError, "reader census"):
            check(image)

    def test_unreviewed_writer_rejected(self):
        image = self.image()
        image[0x200:0x206] = bytes.fromhex("75f800080021")
        with self.assertRaisesRegex(ValueError, "writer census"):
            check(image)

    def test_unreviewed_programming_port_rejected(self):
        for port in (0x30, 0x31, 0x32):
            for opcode in (0x74f8, 0x75f8):
                image = self.image()
                image[0x200:0x206] = b"".join(word.to_bytes(2, "big") for word in (opcode, 8, port))
                with self.subTest(port=port, opcode=opcode), self.assertRaisesRegex(ValueError, "programming census"):
                    check(image)


class CodecTraceTests(unittest.TestCase):
    def trace(self):
        events = [("data", "write", "0022", "c008", "0e22"),
                  ("data", "write", "0022", "c0c8", "0e24"),
                  ("data", "write", "0021", "0aaa", "0e31"),
                  ("data", "read", "0020", "0aaa", "0e5d"),
                  ("data", "read", "0020", "0aaa", "3464"),
                  ("data", "write", "0021", "ffd5", "358b"),
                  ("io", "write", "0021", "1482", "4555"),
                  ("io", "write", "0021", "1482", "4559"),
                  ("io", "write", "0021", "0482", "455d")]
        events += [("io", "read", "0021", "0482", "3221"),
                   ("io", "write", "0021", "0c82", "3228"),
                   ("io", "read", "0021", "0c82", "33f6"),
                   ("io", "write", "0021", "0482", "33fd")] * 3
        return "\n".join(
            f"rom4_serial_audit: space={space} direction={direction} address={address} "
            f"value={value} mask=ffff pc={pc} t=0.2"
            for space, direction, address, value, pc in events)

    def test_separate_paths(self):
        check_trace(self.trace())

    def test_new_unresolved_register_access_requires_review(self):
        event = ("\nrom4_serial_audit: space=data direction=write address=0032 "
                 "value=c0c8 mask=ffff pc=3b60 t=3.0")
        with self.assertRaisesRegex(ValueError, "peripheral review"):
            check_trace(self.trace() + event)

    def test_control_readback_does_not_hide_reset_writes(self):
        readback = ("rom4_serial_audit: space=data direction=read address=0022 "
                    "value=0800 mask=ffff pc=408d t=0.01\n")
        check_trace(readback + self.trace())
        with self.assertRaisesRegex(ValueError, "reset/release"):
            check_trace(readback + self.trace().replace("value=c008", "value=c000"))

    def test_stale_echo_read_rejected(self):
        with self.assertRaisesRegex(ValueError, "I/O control"):
            check_trace(self.trace().replace("value=0482 mask=ffff pc=3221",
                                             "value=0aaa mask=ffff pc=3221"))

    def test_missing_echo_rejected(self):
        with self.assertRaisesRegex(ValueError, "boot echo"):
            check_trace(self.trace().replace("address=0021 value=0aaa", "address=0021 value=0000"))

    def test_missing_receive_isr_rejected(self):
        with self.assertRaisesRegex(ValueError, "receive ISR"):
            check_trace(self.trace().replace("pc=3464", "pc=0000"))

    def test_incomplete_cycles_rejected(self):
        with self.assertRaisesRegex(ValueError, "I/O control"):
            check_trace("\n".join(self.trace().splitlines()[:-1]))

    def test_lua_error_rejected(self):
        with self.assertRaisesRegex(ValueError, "Lua observation"):
            check_trace(self.trace() + "\nLua error")

class CodecRestoreTests(unittest.TestCase):
    def trace(self):
        state = "t=7.000000000 pc=30bf st0=0000 st1=2900 sp=1000 io21=0482 bspc22=c008"
        return (f"rom4_codec_state: phase=saved {state}\n"
                f"rom4_codec_state: phase=restored {state}\n"
                "state_roundtrip: result=pass")

    def test_exact_restoration(self):
        check_restore(self.trace())

    def test_every_field_mismatch_rejected(self):
        for field in ("t=7.000000000", "pc=30bf", "st0=0000", "st1=2900",
                      "sp=1000", "io21=0482", "bspc22=c008"):
            with self.subTest(field=field):
                changed = self.trace().splitlines()
                changed[1] = changed[1].replace(field, field[:-1] + "1")
                with self.assertRaisesRegex(ValueError, "exactly"):
                    check_restore("\n".join(changed))

    def test_missing_snapshot_rejected(self):
        with self.assertRaisesRegex(ValueError, "snapshot pair"):
            check_restore("\n".join(self.trace().splitlines()[1:]))

    def test_failed_harness_rejected(self):
        with self.assertRaisesRegex(ValueError, "harness failed"):
            check_restore(self.trace().replace("result=pass", "result=fail"))

    def test_exact_nonempty_io_replay(self):
        row = 'rom4_codec_replay: result=pass reference_events=123 restored_events=123 window=0.160000000'
        check_restore(self.trace() + '\n' + row, require_replay=True)
        for changed in ('', row + '\n' + row, row.replace('result=pass', 'result=fail'),
                        row.replace('reference_events=123', 'reference_events=0'),
                        row.replace('restored_events=123', 'restored_events=122'),
                        row.replace('window=0.160000000', 'window=0'),
                        row.replace('window=0.160000000', 'window=1.6'),
                        row.replace('window=', 'missing=')):
            with self.subTest(changed=changed), self.assertRaisesRegex(ValueError, 'I/O continuation'):
                check_restore(self.trace() + '\n' + changed, require_replay=True)

    def test_reported_replay_failure_is_never_ignored(self):
        with self.assertRaisesRegex(ValueError, 'I/O continuation'):
            check_restore(self.trace() + '\nrom4_codec_replay: result=fail reference_events=5 restored_events=5 window=0.16')


class NativeToneTests(unittest.TestCase):
    def trace(self):
        return CodecTraceTests().trace() + "\n" + "\n".join([
            "rom4_tone_enable: imr=035f ifr=0020 bspc22=c8c8 bspce23=0000 t=1.58",
            "input-press: t=8.0 name=1 port=1f",
            "rom4_tone_access: owner=codec_shared_mcu direction=write address=0100a8 value=84260000 mask=ffff0000 pc=272034 t=1.55854",
            "rom4_tone_access: owner=codec_shared_dsp direction=read address=000854 value=8426 mask=ffff pc=0027f2 t=1.55855",
            "rom4_codec_control_call: hit=21 a=0000000026 b=0000000008 sp=1eab return=a598 t=1.55856",
            "rom4_codec_control_call: hit=24 a=0000000626 b=0000000008 sp=1ea3 return=a4bb t=1.57959",
            "rom4_codec_control_call: hit=37 a=0000000626 b=0000000008 sp=1ea3 return=a4bb t=8.077",
            "rom4_tone_access: owner=mcu direction=write address=0100ac value=e10000 mask=ffff0000 pc=272034 t=8.07",
            "rom4_tone_access: owner=dsp direction=read address=000856 value=00e1 mask=ffff pc=00a59a t=8.08",
            "rom4_tone_access: owner=dsp direction=write address=0000fe value=00e1 mask=ffff pc=00a5de t=8.09",
            "input-release: t=8.22 name=1 port=1d",
            "rom4_speech_configuration_entry: selector=0210 destination=0010a6c4 length=0004 caller=002894fd t=0.156",
            "rom4_tone_access: owner=speech_record direction=write address=10b2b8 value=df29 mask=ffff pc=274cba t=0.157",
            "rom4_speech_field_entry: input=df29 parameter=0000 caller=00275445 pc=00271d70 source=0010b2b8 index=09 t=0.158",
            "rom4_tone_access: owner=mcu direction=write address=0100bc value=df290000 mask=ffff0000 pc=272034 t=0.159",
            "rom4_tone_access: owner=speech_shared direction=read address=00085e value=df29 mask=ffff pc=0027ae t=0.160",
            "rom4_tone_access: owner=speech_flags direction=write address=001f2b value=df29 mask=ffff pc=0027ae t=0.160",
            "rom4_tone_access: owner=speech_shared direction=read address=00085e value=df29 mask=ffff pc=0027ae t=8.084",
            "rom4_tone_access: owner=speech_flags direction=write address=001f2b value=df29 mask=ffff pc=0027ae t=8.084",
            "rom4_tone_summary: tx_words=2 rx_reads=2 tone_reads=29 tone_copies=7 t=11.01",
        ])

    def test_organic_tone_boundary(self):
        check_tone(self.trace())

    def test_packed_control_provenance_requires_lane_payload_caller_and_order(self):
        for before, after in (
                ('value=84260000 mask=ffff0000', 'value=8426 mask=ffff'),
                ('value=8426 mask=ffff pc=0027f2', 'value=8426 mask=ffff pc=00a583'),
                ('a=0000000026 b=0000000008', 'a=0000000426 b=0000000008'),
                ('return=a598 t=1.55856', 'return=a4bb t=1.55856'),
                ('t=1.55855', 't=1.55853'),
                ('t=1.57959', 't=1.55854')):
            with self.subTest(before=before), self.assertRaisesRegex(ValueError, 'packed codec-control'):
                check_tone(self.trace().replace(before, after))

    def test_speech_shared_copy_requires_correct_lane(self):
        with self.assertRaisesRegex(ValueError, "speech-control copy"):
            check_tone(self.trace().replace("value=df290000 mask=ffff0000", "value=df29 mask=ffff"))

    def test_speech_field_original_caller(self):
        with self.assertRaisesRegex(ValueError, "original MCU"):
            check_tone(self.trace().replace("caller=00275445", "caller=00271d83"))

    def test_speech_record_lane_and_order(self):
        for old, new in (("value=df29 mask=ffff pc=274cba", "value=df290000 mask=ffff0000 pc=274cba"),
                         ("pc=274cba t=0.157", "pc=274cba t=0.170"),
                         ("pc=274cba t=0.157", "pc=274cb8 t=0.157")):
            with self.subTest(new=new), self.assertRaisesRegex(ValueError, "speech-record"):
                check_tone(self.trace().replace(old, new))

    def test_speech_configuration_identity_and_order(self):
        for old, new in (("selector=0210", "selector=0211"),
                         ("caller=002894fd t=0.156", "caller=002894fd t=0.180")):
            with self.subTest(new=new), self.assertRaisesRegex(ValueError, "EEPROM configuration"):
                check_tone(self.trace().replace(old, new))

    def test_speech_shared_copy_requires_order(self):
        with self.assertRaisesRegex(ValueError, "speech-control copy"):
            check_tone(self.trace().replace("pc=272034 t=0.159", "pc=272034 t=9.159"))

    def test_speech_copy_requires_post_extension_bus_pc(self):
        for pc in ('0027ad', '0027ac'):
            with self.subTest(pc=pc), self.assertRaisesRegex(ValueError, "speech-control copy"):
                check_tone(self.trace().replace(
                    'owner=speech_shared direction=read address=00085e value=df29 mask=ffff pc=0027ae',
                    'owner=speech_shared direction=read address=00085e value=df29 mask=ffff pc=' + pc))

    def test_speech_shared_copy_requires_post_key_observation(self):
        with self.assertRaisesRegex(ValueError, "speech-control copy"):
            check_tone(self.trace().replace("t=8.084", "t=1.584"))

    def test_missing_transmit_irq_rejected(self):
        with self.assertRaisesRegex(ValueError, "masked transmit-ready"):
            check_tone(self.trace().replace("ifr=0020", "ifr=0000"))

    def test_boot_control_call_does_not_prove_key_activation(self):
        with self.assertRaisesRegex(ValueError, "activation caller"):
            check_tone(self.trace().replace("t=8.077", "t=1.577"))

    def test_wrong_codec_register_rejected(self):
        with self.assertRaisesRegex(ValueError, "activation caller"):
            check_tone(self.trace().replace("b=0000000008", "b=0000000009"))

    def test_missing_command_rejected(self):
        with self.assertRaisesRegex(ValueError, "organic tone"):
            check_tone(self.trace().replace("value=e10000", "value=0000"))

    def test_early_boot_copy_does_not_prove_key_response(self):
        with self.assertRaisesRegex(ValueError, "organic tone"):
            check_tone(self.trace().replace("t=8.09", "t=1.59"))

    def test_new_audio_activity_requires_review(self):
        with self.assertRaisesRegex(ValueError, "boundary changed"):
            check_tone(self.trace().replace("tx_words=2", "tx_words=3"))

    def test_missing_uncapped_counts_rejected(self):
        with self.assertRaisesRegex(ValueError, "uncapped"):
            check_tone("\n".join(self.trace().splitlines()[:-1]))


if __name__ == "__main__":
    unittest.main()

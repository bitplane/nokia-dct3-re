import unittest
import contextlib
import io
from pathlib import Path
import tempfile
from unittest.mock import patch

from tools.c54x_rom4_rf_delivery_check import (check, check_request, check_tuning,
    check_tuning_stages, check_tuning_upload, check_tuning_blob, pack_tuning_nonnegative, decode_search_bitmap)
from tools.c54x_rom4_rf_delivery_check import nse1_gsm900_tuning_consistency, nsk1_dcs1800_tuning_consistency
from tools.c54x_rom4_rf_delivery_check import check_observer_health, main, check_execution_tuning


TRACE = """rom4_rf_native_publication: t=1.0 type=008a bytes=2 words=006d
rom4_rf_native_publish_exit: address=3807 t=1.1 type=008a bytes=2 producer=00ab consumer=00a9
rom4_rf_mcu_ring_write: t=1.2 address=000101c8 value=000000aa mask=0000ffff
rom4_rf_mcu_ring_write: t=1.3 address=000101c8 value=000000ab mask=0000ffff
rom4_rf_status_header_write: t=1.4 pc=0026be4a address=00101438 value=13900000 mask=ffff0000
rom4_rf_post_fetch: t=1.5 address=00254c84 pc=00254c84 r0=0000000a r1=00101438 lr=0026be5d
rom4_rf_task10_lifecycle: t=1.6 pc=00211a9a r0=00101438 status=1390
"""


class DeliveryTests(unittest.TestCase):
    def test_nsk_dcs_frequency_plan_over_all_fixed_channels(self):
        for channel in range(512,886):
            for direction,addend,intermediate in [('rx',0x17bf,487000000),('tx',0x1797,400000000)]:
                with self.subTest(channel=channel,direction=direction):
                    packed = pack_tuning_nonnegative(channel+addend)
                    result = nsk1_dcs1800_tuning_consistency(packed,direction)
                    self.assertEqual(result['channel'],channel)
                    self.assertEqual(result['tx_hz'],1710200000+(channel-512)*200000)
                    self.assertEqual(result['rx_hz']-result['tx_hz'],95000000)
                    self.assertEqual(result['if_hz'],intermediate)
                    self.assertFalse(result['wire_validated'])
        result = nsk1_dcs1800_tuning_consistency(0x303633,'rx')
        self.assertEqual((result['channel'],result['lo_hz'],result['rx_hz']),(884,1392600000,1879600000))

    def test_nsk_frequency_inference_never_guesses_direction_or_band(self):
        ambiguous = pack_tuning_nonnegative(512+0x17bf)
        self.assertEqual(nsk1_dcs1800_tuning_consistency(ambiguous,'rx')['channel'],512)
        self.assertEqual(nsk1_dcs1800_tuning_consistency(ambiguous,'tx')['channel'],552)
        with self.assertRaises(TypeError):
            nsk1_dcs1800_tuning_consistency(ambiguous)
        for packed,direction in [(ambiguous,'auto'),(ambiguous|0x40,'rx'),(0x313633,'rx'),
                                 (pack_tuning_nonnegative(511+0x17bf),'rx'),
                                 (pack_tuning_nonnegative(886+0x1797),'tx'),
                                 (pack_tuning_nonnegative(109+0x13a6),'rx')]:
            with self.subTest(packed=packed,direction=direction), self.assertRaises(ValueError):
                nsk1_dcs1800_tuning_consistency(packed,direction)

    def test_execution_publication_requires_its_own_complete_chain(self):
        marker = 'rom4_rf_publication_probe: points=37ce,3807 phase=instruction_entry\n'
        lines = TRACE.splitlines()
        lines[0] = lines[0].replace('native_publication: t=1.0', 'execution_publication:')
        lines[1] = lines[1].replace('native_publish_exit:', 'execution_publish_exit:').replace(' t=1.1', '')
        trace = marker + '\n'.join(lines)
        self.assertEqual(check(trace, execution=True), ('00101438', '006d'))
        for bad in (TRACE, marker + TRACE, marker + trace, trace.replace('words=006d', 'words=bad'),
                    trace.replace('producer=00ab', 'producer=00ac'),
                    trace.replace('r1=00101438', 'r1=00101480')):
            with self.subTest(text=bad), self.assertRaises(ValueError):
                check(bad, execution=True)
        for index in range(len(lines)):
            with self.subTest(index=index), self.assertRaises(ValueError):
                check(marker + '\n'.join(lines[:index] + lines[index + 1:]), execution=True)

    def test_execution_tuning_requires_numeric_ownership_and_real_port_order(self):
        marker = 'rom4_rf_tuning_probe: points=3ffb,400f,402e,403a,4048 phase=instruction_entry\n'
        fields = 'ar2=0031 flags6be=0010 flags216a=0004 addend216b=13a6 selected1867=006d'
        stages = [('3ffb', '6d', '4', '32'), ('400f', '6d', '13a6', '32'),
                  ('402e', '1413', '13a6', '32'), ('403a', '1413', '13a6', '32'),
                  ('4048', '302813', '302813', '50')]
        rows = [f'rom4_rf_execution_tuning: address={pc} a={a} b={b} field60={word} {fields}\n'
                for pc, a, b, word in stages]
        upload = ''.join(f'rom4_rf_tuning_field_write: space=program t=0.{i + 1} pc=31d7 address={pc} value={word}\n'
                         for i, (pc, word) in enumerate((('216a', '0004'), ('216b', '13a6'), ('216c', '13a6'))))
        ports = ('rom4_rf_tuning_write: t=1.0 pc=ffff port=0031 value=2813 selected1867=006d\n'
                 'rom4_rf_tuning_write: t=1.1 pc=ffff port=0032 value=0030 selected1867=006d\n')
        trace = marker + upload + ''.join(rows) + ports
        self.assertEqual(check_execution_tuning(trace), 0x302813)
        for bad in (trace.replace(marker, ''), marker + trace, marker + ''.join(rows) + upload + ports,
                    trace.replace('t=0.3', 't=2.0'), trace.replace('value=2813', 'value=2814'),
                    trace.replace('a=1413', 'a=1414'), trace.replace('flags6be=0010', 'flags6be=0090'),
                    trace.replace('addend216b=13a6 ', ''), trace.replace('t=1.1', 't=0.9'),
                    trace.replace('execution_tuning:', 'tuning_stage:')):
            with self.subTest(text=bad), self.assertRaises(ValueError):
                check_execution_tuning(bad)
        for index in range(len(rows)):
            with self.subTest(index=index), self.assertRaises(ValueError):
                check_execution_tuning(marker + upload + ''.join(rows[:index] + rows[index + 1:]) + ports)

    def test_header_write_requires_the_store_not_adjacent_literal_load(self):
        self.assertEqual(check(TRACE), ('00101438', '006d'))
        for pc in ('0026be48', '0026be4c'):
            with self.subTest(pc=pc), self.assertRaisesRegex(ValueError, 'status_header_write'):
                check(TRACE.replace('pc=0026be4a', 'pc=' + pc))

    def test_cli_checks_sibling_and_explicit_console(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / 'error.log'
            log.write_text(TRACE)
            console = log.with_name('console.log')
            console.write_text('[LUA ERROR] failed read tap')
            healthy = log.with_name('healthy.log')
            healthy.write_text('known ROM warning')
            for extra, expected in (([], 1), (['--console', str(healthy)], 0),
                                    (['--console', str(log.with_name('missing'))], 1)):
                with self.subTest(extra=extra), patch('sys.argv', ['check', str(log)] + extra), \
                        contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                    self.assertEqual(main(), expected)

    def test_observer_errors_reject_otherwise_valid_partial_trace(self):
        for error in ('[LUA ERROR] error in read tap callback: nil register',
                      'Fatal error: Error running autoboot script fixture.lua: runtime error'):
            with self.subTest(error=error), self.assertRaisesRegex(ValueError, 'observer failed'):
                check(TRACE + error)
            with self.subTest(console=error), self.assertRaises(ValueError):
                check_observer_health(error)

    def test_nonfatal_device_warnings_are_not_observer_failures(self):
        check_observer_health('ALSA [error.sequencer] open failed\n'
                              'nse1_rom4_boot.bin NOT FOUND (NO GOOD DUMP KNOWN)\n')

    def test_status_handler_count_limit_and_flag_return(self):
        effects = ('rom4_rf_task10_consumer: pc=00211eb0 object=00101438 status=1390 control=80000000 t=1.7\n'
                   'rom4_rf_task10_counter: pc=00211ec4 address=0010fee0 value=162f0000 mask=ffff0000 t=1.8\n'
                   'rom4_rf_task10_helper: pc=00210f5c object=00101438 status=1390 caller=00211ed3 t=1.9\n')
        decisions = ''.join(f'rom4_rf_task10_decision: pc={pc} object=00101438 '
            f'status=1390 count={count} flags=00 r0=00000001 t={time}\n'
            for pc, count, time in (('002114fc', '0001', '2.0'),
                                   ('00211508', '0002', '2.1'),
                                   ('002115a2', '0002', '2.2')))
        text = TRACE + effects + decisions
        self.assertEqual(check(text, consumer_decision=True), ('00101438', '006d'))
        for changed in (text.replace('count=0002', 'count=0003'),
                        text.replace('flags=00', 'flags=10'),
                        text.replace('r0=00000001 t=2.1', 'r0=00000002 t=2.1'),
                        text.replace('r0=00000001 t=2.1', 'r0=00010000 t=2.1'),
                        text.replace('pc=002115a2 object=00101438', 'pc=002115a2 object=00101480'),
                        text.replace('t=2.2', 't=1.8'),
                        text.replace('pc=00211508', 'pc=0021150a')):
            with self.subTest(text=changed), self.assertRaises(ValueError):
                check(changed, consumer_decision=True)

    def test_consumer_effects_preserve_identity_and_order(self):
        case = ('rom4_rf_task10_consumer: pc=00211eb0 object=00101438 '
                'status=1390 control=80000000 t=1.7\n')
        counter = ('rom4_rf_task10_counter: pc=00211ec4 address=0010fee0 '
                   'value=162f0000 mask=ffff0000 t=1.8\n')
        helper = ('rom4_rf_task10_helper: pc=00210f5c object=00101438 '
                  'status=1390 caller=00211ed3 t=1.9\n')
        text = TRACE + case + counter + helper
        self.assertEqual(check(text, consumer_effects=True), ('00101438', '006d'))
        for changed in (text.replace(counter, ''), text.replace(helper, ''),
                        text.replace('80000000', '82000000'),
                        text.replace('162f0000', '162f0001'),
                        text.replace('ffff0000', '0000ffff'),
                        text.replace('0010fee0', '0010fee2'),
                        text.replace(helper, helper.replace('00101438', '00101480')),
                        text.replace('00211ed3', '00211f45'),
                        text.replace('t=1.9', 't=1.75')):
            with self.subTest(text=changed), self.assertRaises(ValueError):
                check(changed, consumer_effects=True)

    def test_correlated_consumer_case(self):
        case = ('rom4_rf_task10_consumer: pc=00211eb0 object=00101438 '
                'status=1390 control=80000000 t=1.7\n')
        self.assertEqual(check(TRACE + case, consumer_case=True), ('00101438', '006d'))
        for changed in ('', case.replace('00101438', '00101480'),
                        case.replace('1390', '138f'), case.replace('1.7', '1.5'),
                        case.replace('80000000', 'malformed')):
            with self.subTest(case=changed), self.assertRaises(ValueError):
                check(TRACE + changed, consumer_case=True)

    def test_manual_frequency_consistency(self):
        actual = nse1_gsm900_tuning_consistency(0x302813)
        self.assertEqual((actual["channel"], actual["lo_hz"], actual["rx_hz"], actual["tx_hz"]),
                         (109, 1027800000, 956800000, 911800000))
        self.assertFalse(actual["wire_validated"])
        for channel in range(1, 125):
            result = nse1_gsm900_tuning_consistency(pack_tuning_nonnegative(5030 + channel))
            self.assertEqual(result["channel"], channel)
            self.assertEqual(result["lo_hz"], 1006000000 + 200000 * channel)
        for packed in (-1, 0x100000000, 0x302853, 0x312813,
                       pack_tuning_nonnegative(5030), pack_tuning_nonnegative(5155)):
            with self.subTest(packed=packed), self.assertRaises(ValueError):
                nse1_gsm900_tuning_consistency(packed)

    def test_blob_selection(self):
        rows = [("00272122", "00299894", "216a0000", "ffff0000"),
                ("00272122", "00299894", "00001e80", "0000ffff"),
                ("00272122", "00299898", "011c0000", "ffff0000"),
                ("00272122", "00299898", "00000130", "0000ffff"),
                ("00272122", "0029989c", "00780000", "ffff0000"),
                ("002936f0", "002998a0", "000413a6", "ffffffff"),
                ("002936f0", "002998a4", "13a62a04", "ffffffff")]
        lines = [f"rom4_rf_tuning_blob_read: t=0.{i + 1} blob=00299894 pc={pc} address={address} value={value} mask={mask}\n"
                 for i, (pc, address, value, mask) in enumerate(rows)]
        upload = "rom4_rf_tuning_field_write: space=program t=1.0\n"
        self.assertEqual(check_tuning_blob("".join(lines) + upload), 0x299894)
        for index in range(len(lines)):
            with self.subTest(index=index), self.assertRaises(ValueError):
                check_tuning_blob("".join(lines[:index] + lines[index + 1:]) + upload)
        for old, new in (("13a62a04", "13a62804"), ("blob=00299894", "blob=00295444"),
                         ("t=1.0", "t=0.0"), ("mask=ffffffff", "mask=ffff0000")):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_tuning_blob(("".join(lines) + upload).replace(old, new))

    def test_packing_reference(self):
        self.assertEqual(pack_tuning_nonnegative(0x1413), 0x302813)
        for quotient in (0, 1, 0x50, 0x3ff):
            for remainder in range(64):
                self.assertEqual(pack_tuning_nonnegative(quotient * 64 + remainder),
                                 (0x30 << 16) | (quotient << 7) | remainder)
        for value in (-1, 0x10000):
            with self.assertRaises(ValueError):
                pack_tuning_nonnegative(value)

    def test_packing_stages(self):
        fields = "ar2=0031 flags6be=0010 flags216a=0004 addend216b=13a6 selected1867=006d"
        stages = [("3ffb", "6d", "4", "32"), ("400f", "6d", "13a6", "32"),
                  ("402e", "1413", "13a6", "32"), ("403a", "1413", "13a6", "32"),
                  ("4048", "302813", "302813", "50")]
        lines = [f"rom4_rf_tuning_stage: t={i + 1} address={address} a={a} b={b} field60={field} {fields}\n"
                 for i, (address, a, b, field) in enumerate(stages)]
        ports = """rom4_rf_tuning_write: t=5 pc=4023 port=0031 value=2813 a=00302813 ar2=0031 selected1867=006d
rom4_rf_tuning_write: t=6 pc=4028 port=0032 value=0030 a=00302813 ar2=0031 selected1867=006d
"""
        trace = "".join(lines) + ports
        self.assertEqual(check_tuning_stages(trace), 0x302813)
        upload = "".join(f"rom4_rf_tuning_field_write: space=program t=0.{i + 1} pc=31d7 address={address} value={value}\n"
                         for i, (address, value) in enumerate((("216a", "0004"), ("216b", "13a6"), ("216c", "13a6"))))
        self.assertEqual(check_tuning_upload(upload + trace), 0x302813)
        for old, new in (("space=program", "space=data"), ("pc=31d7", "pc=31d8"),
                         ("value=13a6", "value=13a7"), ("t=0.3", "t=9.3")):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_tuning_upload(upload.replace(old, new) + trace)
        for index in range(len(lines)):
            with self.subTest(index=index), self.assertRaises(ValueError):
                check_tuning_stages("".join(lines[:index] + lines[index + 1:]) + ports)
        for old, new in (("a=1413", "a=1414"), ("flags6be=0010", "flags6be=0090"),
                         ("flags216a=0004", "flags216a=0006"), ("field60=50", "field60=51"),
                         ("b=13a6", "b=13a7")):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_tuning_stages(trace.replace(old, new))

    def test_tuning_accumulator_pair(self):
        trace = """rom4_rf_tuning_write: t=1.0 pc=4023 port=0031 value=2813 a=0000302813 ar2=0031 selected1867=006d
rom4_rf_tuning_write: t=1.1 pc=4028 port=0032 value=0030 a=0000302813 ar2=0031 selected1867=006d
"""
        self.assertEqual(check_tuning(trace), 0x302813)
        for old, new in (("value=2813", "value=2814"),
                         ("value=0030", "value=0031"),
                         ("pc=4028", "pc=4023"), ("t=1.1", "t=0.9"),
                         ("selected1867=006d", "selected1867=0000")):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_tuning(trace.replace(old, new))

    def test_request_selection_contract(self):
        words = [0x10, 0x9800] + [0] * 31
        words[26] = 0x1000
        request = "rom4_rf_search_request: t=1.0 ar2=0816 words=" + ",".join(f"{word:04x}" for word in words) + "\n"
        scan = "rom4_rf_scan_entry: address=3129 t=1.1 selected1867=006d cursor_word=006d\n"
        self.assertEqual(check_request(request + scan), 109)
        for text in (scan, request, scan + request,
                     request + scan.replace("t=1.1", "t=0.9"),
                     request + scan.replace("selected1867=006d", "selected1867=0001"),
                     request + scan.replace("cursor_word=006d", "cursor_word=7fff"),
                     request.replace("words=0010", "words=0011") + scan,
                     request.replace("1000", "1001") + scan):
            with self.subTest(text=text), self.assertRaises(ValueError):
                check_request(text)

    def test_captured_bitmap(self):
        words = [0x10, 0x9800] + [0] * 31
        words[26] = 0x1000
        self.assertEqual(decode_search_bitmap(words), [109])

    def test_bitmap_basis(self):
        for group in range(32):
            for bit in range(16):
                words = [0x10, 0] + [0] * 31
                words[32 - group] = 1 << bit
                expected = (1 + group * 16 if group < 8 else 512 + (group - 8) * 16) + bit
                if group == 31 and bit >= 8:
                    expected_entries = []  # High byte of word 1 is not copied into scratch.
                else:
                    expected_entries = [expected]
                self.assertEqual(decode_search_bitmap(words), expected_entries)

    def test_bitmap_rejects_wrong_contract(self):
        for words in ([0] * 32, [1] + [0] * 32, [0x10000] + [0] * 32):
            with self.assertRaises(ValueError):
                decode_search_bitmap(words)

    def test_complete_chain(self):
        self.assertEqual(check(TRACE), ("00101438", "006d"))

    def test_every_link_required(self):
        lines = TRACE.splitlines()
        for index in range(len(lines)):
            with self.subTest(index=index), self.assertRaises(ValueError):
                check("\n".join(lines[:index] + lines[index + 1:]))

    def test_identity_and_order(self):
        for old, new in (("r1=00101438", "r1=00101480"),
                         ("status=1390", "status=138f"),
                         ("t=1.6", "t=0.5"),
                         ("producer=00ab", "producer=00ac"),
                         ("mask=0000ffff", "mask=ffff0000")):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check(TRACE.replace(old, new))

    def test_wrap(self):
        wrapped = TRACE.replace("consumer=00a9", "consumer=00e3")
        wrapped = wrapped.replace("producer=00ab", "producer=0081")
        wrapped = wrapped.replace("value=000000aa", "value=00000080")
        wrapped = wrapped.replace("value=000000ab", "value=00000081")
        self.assertEqual(check(wrapped)[1], "006d")


if __name__ == "__main__":
    unittest.main()

import struct
import unittest

from mu4_context_contract import (MONO_OUTPUT_BASE, MONO_OUTPUT_WITNESS,
                                  decode_mono_output)
from mu4_iso_aac_check import check_stream, original_assets,compare_host_pcm,check_tail_reserve


def fixture(calls=6):
    encoded = bytearray(2048)
    consumed = 0
    lengths = list(range(13, 13+calls))
    for length in lengths:
        header = bytearray.fromhex('fff85040000000')
        header[4] = length >> 3
        header[5] = (length & 7) << 5
        encoded[consumed:consumed+7] = header
        consumed += length
    lines = ['mu4_aac_abi_setup: original_upload_words=49843 original_initializers=228 input_bytes=2048 output_base=7000 output_words=2048 constructor=023d27 process=023d5d isolated_abi=1 handset_boot=0 conformance=0']
    base, consumed, window, ordinal = 0x162e, 0, 0, 0
    for call, length in enumerate(lengths, 1):
        consumed += length
        remaining = 24 if consumed % 2 else 16
        cursor = 0x162e+(consumed+remaining//8-window)//2
        for index in range(cursor-0x162e+1):
            ordinal += 1
            offset = window+2*index
            lines.append(f'mu4_aac_session_input_read: ordinal={ordinal} index={index} source_byte={offset} address={0x162e+index:04x} value={int.from_bytes(encoded[offset:offset+2],"big"):04x} mask=ffff context_pc=025129 observer_writes=0')
        lines += [
            f'mu4_aac_abi_header: ordinal={call} sync=0fff id=0001 layer=0000 protection=0000 profile=0001 frequency=0004 channels=0001 frame_length={length:04x} blocks=0001 identity=Execute_entry observer_writes=0',
            'mu4_aac_abi_sources: left=e800 right=f000 sp=8fe8 result=0000 identity=Execute_entry observer_writes=0',
            'mu4_aac_abi_capture: frames=1024 channels=2 raw_bytes=8192 converted_bytes=4096 source=decoder_return_pointers memory=post_wrapper_return conformance=0',
            f'mu4_aac_session_return: call={call} descriptor={cursor:04x} base={base:04x} index={cursor-base:04x} remaining={remaining:04x} result=0000 observer_writes=0']
        base = cursor
        if call % 4 == 0:
            window += 2*(cursor-0x162e)
            available = min(2048, len(encoded)-window) & ~1
            lines.append(f'mu4_aac_session_refill: after_call={call} source_byte={window} old_cursor={cursor:04x} cursor=162e end={0x162e+available//2:04x} bytes={available} remaining={remaining:04x} reservoir_high={encoded[window-4:window-2].hex()} reservoir_low={encoded[window-2:window].hex()} fixture_input=1 decoder_state_writes=0')
            base = 0x162e
    available = min(2048, len(encoded)-window) & ~1
    lines.append(f'mu4_aac_wrapper_early_return: completed_calls={calls} headers={calls} sources={calls} result=ffff cursor={cursor:04x} end={0x162e+available//2:04x} base={cursor:04x} index=0000 remaining=0000 input_source={window} window_bytes={available} illegal=0 observer_writes=0')
    return '\n'.join(lines), bytes(calls*8192), bytes(calls*4096), bytes(encoded)


class IsoAacCheckTest(unittest.TestCase):
    def test_original_tail_reserve_is_a_rejection_not_eof_completion(self):
        lines = ['mu4_aac_session_return: call=342 result=0000',
                 'mu4_aac_tail_reserve: opcode=f6aa candidate_index=0000 reserve_limit=0040 extent=0048 remaining=0008 identity=Execute_entry observer_writes=0',
                 'mu4_aac_tail_reserve: opcode=f6aa candidate_index=0048 reserve_limit=0040 extent=0048 remaining=0000 identity=Execute_entry observer_writes=0',
                 'mu4_aac_wrapper_early_return: result=ffff']
        text = '\n'.join(lines)
        result = check_tail_reserve(text)
        self.assertEqual(result['reserve_words'], 8)
        self.assertGreater(result['rejected_candidate'], result['reserve_limit'])
        self.assertFalse(result['complete_stream'])
        self.assertFalse(result['eof_policy_established'])
        for field in ('opcode=f6aa', 'candidate_index=0048', 'reserve_limit=0040',
                      'extent=0048', 'remaining=0008', 'identity=Execute_entry',
                      'observer_writes=0'):
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_tail_reserve(text.replace(field, field.split('=')[0]+'=invalid'))
        for bad in (lines[:1]+lines[2:], lines[:2]+lines[1:],
                    [lines[1], lines[0], *lines[2:]], lines[:-1]):
            with self.assertRaises(ValueError):
                check_tail_reserve('\n'.join(bad))

    def test_host_pcm_diagnostic_uses_first_source_and_no_fit(self):
        raw=struct.pack('<6i',0,999,16384,999,-32768,999)
        host=struct.pack('<4f',0,1/32768,-2/32768,0.25)
        result=compare_host_pcm(raw,host,4)
        self.assertEqual(0,result['rms_s16'])
        self.assertEqual(0,result['maximum_s16'])
        self.assertEqual(3,result['compared_samples'])
        self.assertEqual(1,result['uncompared_host_tail'])
        for field in ('conformance','iso_reference_alignment','handset_playback',
                      'fitted_gain','fitted_offset'):
            self.assertIs(result[field],False)
        shifted=struct.pack('<4f',1/32768,0,-2/32768,0.25)
        self.assertGreater(compare_host_pcm(raw,shifted,4)['rms_s16'],0)

    def test_host_pcm_diagnostic_rejects_incomplete_and_nonfinite_samples(self):
        raw=struct.pack('<4i',0,0,16384,0)
        for native,host,count in ((b'',bytes(8),2),(raw[:-1],bytes(8),2),
                                  (raw,bytes(4),2),(raw,bytes(4),1),
                                  (raw,struct.pack('<2f',float('nan'),0),2),
                                  (raw,struct.pack('<2f',0,float('inf')),2)):
            with self.assertRaises(ValueError):compare_host_pcm(native,host,count)

    def test_packet_boundary_is_not_conformance_or_complete_stream(self):
        result = check_stream(*fixture(), calls=6)
        self.assertEqual(result['calls'], 6)
        for field in ('conformance', 'complete_stream', 'crc_validation',
                      'waveform_alignment', 'handset_playback'):
            self.assertIs(result[field], False)

    def test_return_and_input_mutations_fail(self):
        text, raw, converted, encoded = fixture()
        for old, new in (('completed_calls=6', 'completed_calls=7'),
                         ('result=ffff', 'result=0000'), ('illegal=0', 'illegal=1'),
                         ('channels=0001', 'channels=0002'), ('left=e800', 'left=f000'),
                         ('observer_writes=0', 'observer_writes=1'),
                         ('decoder_state_writes=0', 'decoder_state_writes=1'),
                         ('context_pc=025129', 'context_pc=000000'),
                         ('after_call=4', 'after_call=3'),
                         ('remaining=0018', 'remaining=0010')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_stream(text.replace(old, new, 1), raw, converted, encoded, calls=6)

    def test_missing_and_reordered_records_fail(self):
        text, raw, converted, encoded = fixture()
        lines = text.splitlines()
        for index in (0, 1, len(lines)-1):
            with self.subTest(index=index), self.assertRaises(ValueError):
                check_stream('\n'.join(lines[:index]+lines[index+1:]), raw, converted, encoded, calls=6)
        header = next(i for i, line in enumerate(lines) if 'mu4_aac_abi_header:' in line)
        lines[header], lines[header+1] = lines[header+1], lines[header]
        with self.assertRaises(ValueError):
            check_stream('\n'.join(lines), raw, converted, encoded, calls=6)

    def test_mono_source_and_both_converted_copies_are_checked(self):
        text, raw, converted, encoded = fixture()
        # The pre-alias second raw pointer is not an independently decoded channel.
        other = bytearray(raw)
        struct.pack_into('<i', other, 4, 123456)
        check_stream(text, other, converted, encoded, calls=6)
        for channel in (0, 1):
            bad = bytearray(converted)
            struct.pack_into('<h', bad, channel*2, 1)
            with self.subTest(channel=channel), self.assertRaises(ValueError):
                check_stream(text, raw, bad, encoded, calls=6)
        with self.assertRaises(ValueError):
            check_stream(text, raw[:-4], converted, encoded, calls=6)

    def test_primary_asset_pins_reject_substitutions(self):
        with self.assertRaises(ValueError):
            original_assets(b'substitute', b'reference', b'upload')

    def test_every_mono_selection_word_is_pinned(self):
        words = {MONO_OUTPUT_BASE+i: word for i, word in enumerate(MONO_OUTPUT_WITNESS)}
        self.assertEqual(decode_mono_output(words)['mono_value'], 1)
        for address in words:
            changed = dict(words)
            changed[address] ^= 1
            with self.subTest(address=address), self.assertRaises(ValueError):
                decode_mono_output(changed)


if __name__ == '__main__':
    unittest.main()

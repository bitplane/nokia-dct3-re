import struct
import unittest

from mu4_aac_abi_check import check_capture, check_input, check_session


TEXT = '''mu4_aac_abi_setup: original_upload_words=49843 original_initializers=228 input_bytes=2048 output_base=7000 output_words=2048 constructor=023d27 process=023d5d isolated_abi=1 handset_boot=0 conformance=0
mu4_aac_abi_header: ordinal=1 sync=0fff id=0001 layer=0000 protection=0001 profile=0001 frequency=0004 channels=0002 frame_length=0134 blocks=0001 identity=Execute_entry observer_writes=0
mu4_aac_abi_sources: left=f000 right=e800 sp=8fe8 result=0000 identity=Execute_entry observer_writes=0
mu4_aac_abi_result: returned=1 result=0000 base=162e index=009b remaining=0010 illegal=0 pc=001018 isolated_abi=1 conformance=0
mu4_aac_abi_capture: frames=1024 channels=2 raw_bytes=8192 converted_bytes=4096 source=decoder_return_pointers memory=post_wrapper_return conformance=0
'''


class AbiCheckTest(unittest.TestCase):
    def session_fixture(self, calls=8, boundary=False):
        lengths = ([308,378,362,286,324,348,349,365]*((calls+7)//8))[:calls]
        remaining_counts = ([16,16,16,16,32,16,24,16]*((calls+7)//8))[:calls]
        encoded = bytearray(sum(lengths)+remaining_counts[-1]//8+457 if boundary else max(4096,sum(lengths)+2048))
        offset = 0
        for length in lengths:
            header = bytearray.fromhex('fff95080269ffc')
            header[3] = (header[3] & 0xfc) | (length >> 11)
            header[4] = (length >> 3) & 255
            header[5] = ((length & 7) << 5) | (header[5] & 31)
            encoded[offset:offset+7] = header
            offset += length
        encoded[1332:1334] = bytes.fromhex('dbbc')
        lines = [TEXT.splitlines()[0]]
        ordinal, indices = 0, set()
        def reads(window, count):
            nonlocal ordinal
            for index in range(count):
                ordinal += 1
                indices.add(index)
                source = window+2*index
                lines.append(f'mu4_aac_session_input_read: ordinal={ordinal} index={index} source_byte={source} address={0x162e+index:04x} value={int.from_bytes(encoded[source:source+2],"big"):04x} mask=ffff context_pc=025129 observer_writes=0')
        reads(0,669)
        base, consumed, window = 0x162e, 0, 0
        for call,(length,remaining) in enumerate(zip(lengths,remaining_counts),1):
            consumed += length
            cursor = 0x162e+(consumed+remaining//8-window)//2
            lines += [TEXT.splitlines()[1].replace('ordinal=1',f'ordinal={call}').replace('frame_length=0134',f'frame_length={length:04x}'),
                      TEXT.splitlines()[2],TEXT.splitlines()[4],
                      f'mu4_aac_session_return: call={call} descriptor={cursor:04x} base={base:04x} index={cursor-base:04x} remaining={remaining:04x} result=0000 observer_writes=0']
            last_base, last_index = base, cursor-base
            base = cursor
            if call % 4 == 0 and (call < calls or boundary):
                window += 2*(cursor-0x162e)
                available = min(2048,len(encoded)-window) & ~1
                lines.append(f'mu4_aac_session_refill: after_call={call} source_byte={window} old_cursor={cursor:04x} cursor=162e end={0x162e+available//2:04x} bytes={available} remaining={remaining:04x} reservoir_high={encoded[window-4:window-2].hex()} reservoir_low={encoded[window-2:window].hex()} fixture_input=1 decoder_state_writes=0')
                base = 0x162e
                next_count = 3 if call == calls else (sum(lengths[call:call+4])+remaining_counts[call+3]//8-remaining//8)//2+1
                reads(window,next_count)
        if boundary:
            lines += [f'mu4_aac_tail_reserve: opcode=f6aa candidate_index={index} reserve_limit=00dc extent=00e4 remaining={remaining} identity=Execute_entry observer_writes=0'
                      for index,remaining in [('0000','0008'),('00dd','0000')]]
            lines.append(f'mu4_aac_wrapper_early_return: completed_calls={calls} headers={calls} sources={calls} result=ffff cursor=162e end=1712 base=162e index=0000 remaining=0000 input_source={window} window_bytes=456 illegal=0 observer_writes=0')
            return '\n'.join(lines),bytes(encoded)
        pc = '001023' if calls > 8 else '001076'
        lines += [f'mu4_aac_abi_result: returned=1 result=0000 base={last_base:04x} index={last_index:04x} remaining=0010 illegal=0 pc={pc} isolated_abi=1 conformance=0',
                  f'mu4_aac_abi_input_end: reads={ordinal} unique_words={len(indices)} highest_index={max(indices)} complete=1 observer_writes=0']
        return '\n'.join(lines), bytes(encoded)

    def test_repeated_refills_require_all_offsets_and_final_call(self):
        text,encoded = self.session_fixture(32)
        def check(value):
            return check_session(value,bytes(262144),bytes(131072),bytes(131072),encoded,stream=True)
        self.assertEqual(check(text)['calls'],32)
        for changed in (text.replace('after_call=28','after_call=24'),
                        text.replace('call=32 descriptor','call=31 descriptor'),
                        text.replace('pc=001023','pc=001076'),
                        text.replace('context_pc=025129','context_pc=000000',1)):
            with self.subTest(changed=changed[-100:]), self.assertRaises(ValueError):
                check(changed)

    def test_eof_boundary_is_not_clean_completion_or_padding_permission(self):
        text,encoded = self.session_fixture(172,boundary=True)
        def check(value):
            return check_session(value,bytes(172*8192),bytes(172*4096),bytes(172*4096),encoded,eof_boundary=True)
        self.assertEqual(check(text)['status'],'native_short_input_boundary_not_eof_completion')
        for changed in (text.replace('result=ffff','result=0000'),
                        text.replace('candidate_index=00dd','candidate_index=00dc'),
                        text.replace('window_bytes=456','window_bytes=458'),
                        text.replace('illegal=0','illegal=1')):
            with self.subTest(changed=changed[-120:]), self.assertRaises(ValueError):
                check(changed)

    def test_refill_session_and_mutated_continuity(self):
        text, encoded = self.session_fixture()
        def check(value):
            return check_session(value,bytes(65536),bytes(32768),bytes(32768),encoded,refill=True)
        self.assertEqual(check(text)['consumed_bytes'],2720)
        for changed in (text.replace('source_byte=1336 old_cursor','source_byte=1334 old_cursor'),
                        text.replace('reservoir_low=fff9','reservoir_low=0000'),
                        text.replace('remaining=0020','remaining=0010'),
                        text.replace('decoder_state_writes=0','decoder_state_writes=1'),
                        text.replace('value=fff9','value=fff8',1),
                        text.replace('pc=001076','pc=001042'),
                        text.replace('frame_length=016a','frame_length=016b'),
                        text+ '\n'+TEXT.splitlines()[2]):
            with self.subTest(changed=changed[-100:]), self.assertRaises(ValueError):
                check(changed)

    def test_session_requires_four_returns_and_complete_audio(self):
        with self.assertRaises(ValueError):
            check_session(TEXT,bytes(32768),bytes(16384),bytes(16384),bytes(2048))

    def input_fixture(self):
        encoded = bytes.fromhex('fff95080269ffc') + bytes(2048-7)
        events = '\n'.join(f'mu4_aac_abi_input_read: ordinal={i+1} index={i} address={0x162e+i:04x} value={int.from_bytes(encoded[2*i:2*i+2],"big"):04x} mask=ffff context_pc=025129 observer_writes=0'
                           for i in range(156))
        events += '\nmu4_aac_abi_sources: marker=only\nmu4_aac_abi_input_end: reads=156 unique_words=156 highest_index=155 complete=1 observer_writes=0\n'
        return encoded, events

    def test_input_bytes_and_lookahead_are_separately_accounted(self):
        encoded, events = self.input_fixture()
        result = check_input(events,encoded)
        self.assertEqual(result['consumed_frame_bytes'],308)
        self.assertEqual(result['lookahead_bytes'],4)

    def test_changed_input_value_mask_gap_and_terminal_reject(self):
        encoded, events = self.input_fixture()
        for changed in (events.replace('value=fff9','value=fff8'),
                        events.replace('mask=ffff','mask=00ff',1),
                        events.replace('index=20 ','index=21 ',1),
                        events.replace('reads=156','reads=155')):
            with self.subTest(changed=changed[:100]), self.assertRaises(ValueError):
                check_input(changed,encoded)

    def test_waveform_error_is_reported_not_accepted_as_tolerance(self):
        raw = struct.pack('<2048i', *([16384]*2048))
        converted = struct.pack('<2048h', *([1]*2048))
        result = check_capture(TEXT, raw, converted, bytes(4096))
        self.assertEqual(result['channels'][0]['mismatches'],1024)
        self.assertEqual(result['channels'][0]['rms'],1)
        self.assertNotIn('conformant',result)

    def test_wrong_source_failure_fault_and_duplicate_reject(self):
        for text in (TEXT.replace('left=f000','left=e800'), TEXT.replace('illegal=0','illegal=1'),
                     TEXT.replace('returned=1','returned=0'), TEXT+TEXT,
                     TEXT+'Fatal error: incomplete', TEXT.replace('frames=1024','frames=1023'),
                     TEXT.replace('frequency=0004','frequency=0003'),
                     TEXT.replace('output_base=7000','output_base=8000')):
            with self.subTest(text=text), self.assertRaises(ValueError):
                check_capture(text,bytes(8192),bytes(4096),bytes(4096))

    def test_changed_conversion_and_truncated_buffers_reject(self):
        for raw, converted, reference in ((bytes(8191),bytes(4096),bytes(4096)),
                                           (bytes(8192),bytes(4095),bytes(4096)),
                                           (bytes(8192),bytes(4096),bytes(4095)),
                                           (bytes(8192),b'\x01'+bytes(4095),bytes(4096))):
            with self.assertRaises(ValueError):
                check_capture(TEXT,raw,converted,reference)


if __name__ == '__main__':
    unittest.main()

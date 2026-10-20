import unittest
from tools.nsk3_startup_check import check_startup_trace,check_record_math,RESULTS
from tools.dct3_rom4_record_codec import decode_record


class Nsk3StartupTest(unittest.TestCase):
    def test_record_event_order_and_echo(self):
        # Parser fixture; native arithmetic acceptance comes from the runtime gate.
        lines=[]
        for encoded in (bytes.fromhex('7f80')*12,b'\xff'*24):
            for command,body in ((0x13,bytes.fromhex('82153f2d')),(0x14,b'\xff'*12),
                                 (0x15,b'\xff'*20),(0x16,encoded)):
                packet=b'\0\2'+bytes((len(body)+2,0x70,command,len(body)))+body
                lines.append('nsk3_record_post: data='+packet.ljust(60,b'\0').hex()+' cycles=0')
            decoded=b''.join(decode_record(encoded[i:i+12],bytes.fromhex('00160010'))[:10]+bytes(2) for i in (0,12))
            for payload in (bytes.fromhex('340e00825d62129a52b3023baae25f41'),bytes.fromhex('35320000')+decoded+encoded):
                packet=bytes.fromhex('00000074')+(len(payload)+2).to_bytes(2,'big')+bytes.fromhex('0100')+payload
                lines.append('nsk3_record_reply: data='+packet.ljust(60,b'\0').hex()+' cycles=0')
        trace='\n'.join(lines)
        check_record_math(trace)
        for bad in (trace.split('\n',1)[1],trace.replace('35320000','35330000'),trace+'\n'+lines[-1]):
            with self.assertRaises(ValueError):check_record_math(bad)

    def test_result_sweep_is_complete_and_checksum_specific(self):
        for low in (False,True):
            lines=['nsk3_security_checksum: stored=db24 computed=db24',
                   'nsk3_security_checksum: stored=da54 computed=da54']
            for _ in range(2):
                lines.extend(f"nsk3_startup_result: index={i:02x} code={'12' if i==18 and not low else code} pointer={0x10fc80+i:08x}" for i,code in enumerate(RESULTS))
            lines+=['nsk3_record_format: valid=00000000 pointer=00101c4c']*2
            lines+=['nsk3_restart_entry: reason=00000004 caller=0026f335']
            lines.extend(f'nsk3_startup_marker: offset=0010fdc8 value={value} mask=0000ff00 pc={pc}'
                         for value,pc in (('00005a00','0023ab7e'),('0000a500','0026f308'),('00005500','0026f2fa')))
            trace='\n'.join(lines)
            check_startup_trace(trace,low)
            for bad in (trace.replace('computed=db24','computed=ffff'),trace.replace('index=17','index=16'),
                        trace.replace('valid=00000000','valid=00000001'),trace.replace('caller=0026f335','caller=002ac703')):
                with self.assertRaises(ValueError):check_startup_trace(bad,low)

    def test_record_checker_rejects_missing_or_truncated_conversation(self):
        for trace in ('','nsk3_record_post: malformed',
                      'nsk3_record_reply: consumer=0026f150 data=0000 cycles=0'):
            with self.assertRaises(ValueError):check_record_math(trace)


if __name__=='__main__':unittest.main()

import unittest
import hashlib
from unittest.mock import patch
from tools.nsm1_native_radio_check import samples,packets,metric_source
from tools.nsm1_native_radio_check import program_profile


def sample_trace():
    rows=[]
    for time,scan,producer,consumer,count in ((5,0,551,21,6),(20,0,2021,22,6),
            (40,0,2021,22,6),(45,0,2021,22,6),(65,0xffffcf03,5435,3436,7),
            (90,0xffffe42c,10852,8853,7),(100,0xffffeca2,13018,11019,7)):
        rows.append(f'nsm1_native_radio_sample: t={time:.6f} pending=0003 completed=010e scan={scan:08x} qualified=0000 flags=0000 producer=1a00 consumer=1a00 threshold=0574 packets={count} queue_writes={producer}/{consumer} observer_writes=0')
    return '\n'.join(rows)


class NativeRadioTests(unittest.TestCase):
    def test_retained_profile_requires_its_hash_and_upload_observation(self):
        program=bytes(131072)
        digest=hashlib.sha256(program).hexdigest()
        with patch('tools.nsm1_native_radio_check.RETAINED_PROGRAM_SHA256',digest):
            with self.assertRaises(ValueError):program_profile(program,'',True)
            with patch('tools.nsm1_native_radio_check.check_upload') as check:
                program_profile(program,'own retained upload',True)
                check.assert_called_once_with('own retained upload')
            with self.assertRaises(ValueError):program_profile(program[:-1],'',True)
        with self.assertRaises(ValueError):program_profile(program,'',True)

    def test_historical_profile_is_separate(self):
        program=bytes(131072)
        with patch('tools.nsm1_native_radio_check.PROGRAM_SHA256',hashlib.sha256(program).hexdigest()):
            with patch('tools.nsm1_native_radio_check.check_upload') as check:
                program_profile(program,'',False)
                check.assert_not_called()
            with self.assertRaises(ValueError):program_profile(program,'',True)

    def test_second_scan_packet_contract(self):
        payload=b'\x00\x80'+b''.join((512+i).to_bytes(2,'big')+(-129-i%2).to_bytes(2,'big',signed=True) for i in range(40))
        rows=[]
        for index,kind in enumerate((0x74,0x74,0x74,0x74,0x8a,0x87,0x8b,0x8a,0x87,0x8b),1):
            body=payload if kind==0x8b else bytes.fromhex('0375') if index==8 else b'\x00'
            time={7:49.2,8:122.8,9:122.9,10:161.2}.get(index,2.5)
            packet=bytes((0x18,2,len(body),kind))+body
            rows.append(f'nsm1_native_radio_packet: index={index} type={kind:02x} pointer=00102000 bytes={packet.hex()} t={time} observer_writes=0')
        value='\n'.join(rows)
        packets(value,True)
        for bad in (value.replace('8a0375','8a0202'),value.replace('t=161.2','t=160.2'),
                    value.rsplit('\n',1)[0],value.replace('ff7f','ff6c',1)):
            with self.assertRaises(ValueError):packets(bad,True)
        with self.assertRaises(ValueError):packets(value)

    def test_extended_queue_horizon(self):
        value=sample_trace()
        for time,scan,packets_count,producer,consumer in ((120,0xffffed06,7,13118,11119),(140,0,9,18000,16000),(160,0,9,18000,16000)):
            value+=f'\nnsm1_native_radio_sample: t={time:.6f} pending=0003 completed=010e scan={scan:08x} qualified=0000 flags=0000 producer=1a00 consumer=1a00 threshold=0574 packets={packets_count} queue_writes={producer}/{consumer} observer_writes=0'
        self.assertEqual(len(samples(value,True)),10)
        for bad in (value.replace('t=160.000000','t=159.000000'),value.replace('packets=9','packets=8',1),value.rsplit('\n',1)[0]):
            with self.assertRaises(ValueError):samples(bad,True)

    def test_own_metric_address_and_zero_source_are_both_required(self):
        program=bytearray(131072)
        for address,raw in {0x0b25:'6ff8219b0d20',0x0b47:'77161262e8015086',
                0x4701:'f0742424',0x47e6:'7712125477131255308a218a20823083',0x330a:'74d60027'}.items():
            fragment=bytes.fromhex(raw);program[address*2:address*2+len(fragment)]=fragment
        rows=[]
        for index in range(8):
            pointer=0x1a08+index*4
            rows.append(f'nsm1_scan_metric: index={index+1} pc=0b26 a=0000000000 threshold=0574 producer={pointer:04x} consumer={pointer:04x} t={50.001+index*.005:.9f} observer_writes=0')
            rows.append(f'nsm1_scan_rf_input: index={index+1} value=0000 pc={(0x330c,0x3312,0x3318,0x331e)[index%4]:04x} t={50.0001+index*.000001:.9f} observer_writes=0')
        rows.append('nsm1_scan_rf_queue: start=1a00 words=72 data='+'0000'*72+' observer_writes=0')
        trace='\n'.join(rows);metric_source(trace,program)
        for bad in (trace.replace('pc=0b26','pc=0ae7'),trace.replace('a=0000000000','a=0000000001',1),
                    trace.replace('value=0000','value=0001',1),trace.replace('consumer=1a08','consumer=1a0c',1),
                    trace.replace('words=72','words=71')):
            with self.assertRaises(ValueError):metric_source(bad,program)
        program[0x0b25*2]^=1
        with self.assertRaises(ValueError):metric_source(trace,program)

    def test_quiet_initial_window_is_not_a_permanent_queue_stall(self):
        trace=sample_trace();self.assertEqual(len(samples(trace)),7)
        for bad in (trace.replace('qualified=0000','qualified=0001',1),
                    trace.replace('13018/11019','13018/11018'),
                    trace.replace('t=100.000000','t=99.000000'),trace.split('\n',1)[1]):
            with self.assertRaises(ValueError):samples(bad)

    def test_packet_inventory_and_signed_measurements(self):
        payload=bytearray(b'\x00\x80')
        for index in range(40):payload+=(512+index).to_bytes(2,'big')+(-129-index%2).to_bytes(2,'big',signed=True)
        rows=[]
        for index,kind in enumerate((0x74,0x74,0x74,0x74,0x8a,0x87,0x8b),1):
            body=payload if kind==0x8b else b'\x00'
            packet=bytes((0x18,2,len(body),kind))+body
            rows.append(f'nsm1_native_radio_packet: index={index} type={kind:02x} pointer=00102000 bytes={packet.hex()} t={49.2 if kind==0x8b else 2.5} observer_writes=0')
        trace='\n'.join(rows);packets(trace)
        for bad in (trace.replace('type=8a','type=99'),trace.replace('ff7f','ff6c'),trace.replace('t=49.2','t=38.8'),
                    trace.replace('bytes=1802017400','bytes=00',1)):
            with self.assertRaises(ValueError):packets(bad)


if __name__=='__main__':unittest.main()

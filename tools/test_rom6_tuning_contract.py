import hashlib
import io
import unittest
from unittest.mock import patch
from tools import rom6_tuning_contract as contract


def fixture():
    entries,stages,writes=[],[],[]
    for sequence,key in enumerate(sorted(contract.KEYS),1):
        gsm=not 512<=key<=885
        normalized=key-1024 if key>=975 else key
        # Independent frequency-plan reference, not the packing implementation.
        frequency=(935000000+normalized*200000 if gsm else 1805200000+(key-512)*200000)
        scalar=frequency//(100000 if gsm else 200000)
        q,r=divmod(scalar-38,64)
        packed=0x44000000+(q*128+r)+38
        entries.append(f'rom6_tuning_entry: sequence={sequence} address=388c a={key:010x} ar2=0031 flags3e7e=0004 flags3ec8=09f1 flags051f=0080 addend3e7f=1243 addend3e80=1162 observer_writes=0')
        for port,value,pc in ((0x31,packed&65535,0x38bc),(0x32,packed>>16,0x38c1)):
            writes.append(f'{sequence}.000000000\t{pc:06x}\t{port:04x}\t{value:04x}\tffff\n'.encode())
        if sequence>4 and key not in contract.SELECTED: continue
        addend=4675 if gsm else 8514
        values=[('38aa',normalized&0xffffffffff,addend,normalized&65535),
                ('38c9',normalized+addend,addend,normalized&65535),
                ('38de',scalar,int(gsm),normalized&65535),
                ('38f4',r,packed,q),('38c8',packed,packed,q)]
        for address,a,b,scratch in values:
            ar2=0x33 if address=='38c8' else 0x31
            stages.append(f'rom6_tuning_stage: sequence={sequence} address={address} a={a:010x} b={b:010x} ar2={ar2:04x} ar3={int(gsm):04x} field60={scratch:04x} st0=101f st1=6103 observer_writes=0')
    return '\n'.join(entries+stages+[f'rom6_tuning_complete: entries={len(entries)} cap=65536 observer_writes=0']),b''.join(writes)


class TuningContractTest(unittest.TestCase):
    def test_literals_and_positive_formatter_domain(self):
        for scalar,word in ((9352,0x440048c8),(9358,0x440048ce),(9252,0x440047e4),(18500,0x44009044)):
            self.assertEqual(contract.pack_scalar(scalar),word)
            self.assertEqual(contract.unpack_scalar(word),scalar)
        for value in range(38,32806):
            self.assertEqual(contract.unpack_scalar(contract.pack_scalar(value)),value)

    def test_candidates_preserve_collisions_and_unmatched_words(self):
        candidates=contract.rx_candidates(0x440048c8)
        self.assertEqual([(c['band'],c['key']) for c in candidates],[('GSM900',1),('DCS1800',838)])
        self.assertEqual(contract.rx_candidates(0x44009044),[])
        collisions=0
        for key in contract.KEYS:
            candidates=contract.rx_candidates(contract.pack_scalar(contract.rx_scalar(key)))
            self.assertIn(key,[c['key'] for c in candidates])
            if key<125 or key>=975: collisions+=len(candidates)==2
        self.assertEqual(collisions,74)
        for word in (0x600048c8,0x400081c0,0x44000000,0x44000066,-1):
            with self.assertRaises(ValueError): contract.rx_candidates(word)
        for key in (125,511,886,974,1024):
            with self.assertRaises(ValueError): contract.rx_scalar(key)

    def test_complete_producer_stages_and_actual_ports(self):
        text,writes=fixture()
        program=bytes(0x20000)
        digest=hashlib.sha256(program[0x388c*2:0x3938*2]).hexdigest()
        with patch.object(contract,'FORMATTER_SHA256',digest):
            result=contract.check(text,io.BytesIO(writes),program)
            self.assertEqual(result['distinct_keys'],548)
            self.assertFalse(result['wire_validated'])
            for old,new in (('flags3e7e=0004','flags3e7e=0002'),('address=388c','address=389c'),
                            ('sequence=2 ','sequence=1 '),('field60=0000','field60=0001'),
                            ('address=38de','address=38df'),('observer_writes=0','observer_writes=1')):
                with self.subTest(old=old),self.assertRaises(ValueError):
                    contract.check(text.replace(old,new,1),io.BytesIO(writes),program)
            for broken in (writes[:-1],writes.replace(b'48c8',b'48c9',1),writes+writes):
                with self.assertRaises(ValueError): contract.check(text,io.BytesIO(broken),program)
        with self.assertRaises(ValueError): contract.check(text,io.BytesIO(writes),program)


if __name__=='__main__': unittest.main()

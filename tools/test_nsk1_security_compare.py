import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.nsk1_security_compare import check,CONTACTS


class Nsk1SecurityCompareTest(unittest.TestCase):
    def trace(self, initialized):
        rows = []
        for index,(column,mask) in enumerate(CONTACTS,1):
            when = 11+index if index <= 6 else 21+(index-7)*2
            for down,time in ((1,when),(0,when+0.2)):
                rows.append(f'nsk1_code_contact: index={index} column={column} mask={mask:02x} down={down} t={time} physical_only=1')
        rows.extend(f'nsk1_code_key: value={key:02x} t=12.0 observer_writes=0'
                    for key in (1,2,3,4,5,25,25,25,25))
        retry = '00' if initialized else 'ff'
        rows.append(f'nsk1_code_gate: match=00000001 retry={retry} cycles=0000000000000001 observer_writes=0')
        rows.extend(f'nsk1_code_state: checklist=0f root=03 retry={retry} t={time} observer_writes=0'
                    for time in (18,20,22,24,26,28,30))
        return '\n'.join(rows)

    def test_positive_negative_and_failure_controls(self):
        from PIL import Image
        with tempfile.TemporaryDirectory() as directory:
            frames = Path(directory)
            image = Image.new('RGB',(84,48),'white')
            digest = hashlib.sha256(image.tobytes()).hexdigest()
            for time in (26,28,30):
                image.save(frames/f'nsk1_code_{time}s.png')
            with patch('tools.nsk1_security_compare.PHONEBOOK_LIST_RGB_SHA256',digest), \
                    patch('tools.nsk1_security_compare.SECURITY_PROMPT_RGB_SHA256',digest), \
                    patch('tools.nsk1_security_compare.CODE_ERROR_RGB_SHA256',digest):
                for initialized in (False,True):
                    trace = self.trace(initialized)
                    self.assertEqual(initialized,check(trace,frames,initialized)['phonebook_list_accepted'])
                    extended = trace+'\nnsk1_code_key: value=18 t=30.0 observer_writes=0'
                    with self.assertRaises(ValueError):
                        check(extended,frames,initialized)
                    self.assertEqual(initialized,check(extended,frames,initialized,(0x18,))['phonebook_list_accepted'])
                    for changed in (trace.replace('column=2','column=1',1),
                                    trace.replace('match=00000001','match=00000000'),
                                    trace.replace('t=12 ','t=11 ',1),
                                    trace.replace('checklist=0f','checklist=0e',1),
                                    trace+'\nnsk1_code_key: malformed',trace+'\nnsk1_code_state: malformed',
                                    trace+'\n[LUA ERROR]'):
                        with self.subTest(initialized=initialized),self.assertRaises(ValueError):
                            check(changed,frames,initialized)
                Image.new('RGB',(84,48),'black').save(frames/'nsk1_code_30s.png')
                with self.assertRaisesRegex(ValueError,'pixels'):
                    check(self.trace(True),frames,True)


if __name__ == '__main__':
    unittest.main()

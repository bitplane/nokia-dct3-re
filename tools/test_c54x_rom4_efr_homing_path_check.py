import unittest
from tools.c54x_rom4_efr_homing_path_check import SEQUENCES,check_program,check_trace


def fixture():
    lines=[]
    for frame in range(49):
        changed=frame in (1,24,47)
        for replay in range(2):
            for pc in (0xc8de,0x2697,0x269e,0x2870,0x28b0,0x28dc,0x28f7,0xc8ff):
                ar2,sp=(0x1ec2,0x1ec0) if pc==0x2697 else (0x1344,0x1ec3)
                if pc==0x28dc: ar2=0x1371
                parameter=0 if changed and pc in (0x28f7,0xc8ff) else 11
                lines.append(f'efr_homing_path: pc={pc:04x} bfi=0000 flags=0000 '
                    f'recovery={int(changed):04x} parameter={parameter:04x} ar2={ar2:04x} sp={sp:04x}')
    for pc,value,bfi,count in ((0xc8b4,0,0,92),(0xc8b4,1,1,6),
                               (0xc8c6,0,0,7),(0x28f1,0,0,6)):
        lines.extend([f'efr_history_write: pc={pc:04x} address=0ab9 value={value:04x} '
                      f'bfi={bfi:04x} flags={bfi*4:04x}']*count)
    return '\n'.join(lines)


class HomingPathTest(unittest.TestCase):
    def test_original_words_and_every_word_mutation(self):
        image=bytearray(131070)
        for address,words in SEQUENCES.items():
            data=b''.join(word.to_bytes(2,'big') for word in words)
            image[address*2:address*2+len(data)]=data
        check_program(image)
        for address,words in SEQUENCES.items():
            for index in range(len(words)):
                altered=image.copy()
                altered[(address+index)*2]^=1
                with self.subTest(address=address,index=index),self.assertRaises(ValueError):
                    check_program(altered)
        with self.assertRaises(ValueError): check_program(image[:100])

    def test_paired_original_recovery_paths(self):
        self.assertEqual(check_trace(fixture()),[1,24,47])

    def test_staged_sample_boundary_is_explicit(self):
        rows=fixture().splitlines()
        text='\n'.join(row.replace('sp=1ec3','sp=1ec2')
                       if 'pc=c8de ' in row or 'pc=2870 ' in row else row for row in rows)
        text=text.replace('efr_history_write: pc=c8b4','efr_history_write: pc=c8b3')
        self.assertEqual(check_trace(text,staged=True),[1,24,47])
        with self.assertRaises(ValueError): check_trace(text)

    def test_truncation_redirect_replay_and_mutation_changes_rejected(self):
        text=fixture()
        for bad in (text.split('\n',1)[1],text.replace('pc=269e','pc=269d'),
                    text.replace('parameter=0000','parameter=0001'),
                    text.replace('ar2=1ec2 sp=1ec0','ar2=1ec3 sp=1ec0'),
                    text.replace('recovery=0001','recovery=0000',1),
                    text+'\nefr_homing_path: malformed',
                    text.replace('value=0001 bfi=0001','value=0000 bfi=0001'),
                    text.replace('bfi=0001 flags=0004','bfi=0001 flags=0000'),
                    text+'\nefr_history_write: malformed'):
            with self.subTest(bad=bad),self.assertRaises(ValueError): check_trace(bad)

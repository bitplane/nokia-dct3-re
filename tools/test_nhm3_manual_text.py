import unittest
import io
import tempfile
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch
from tools.nhm3_manual_text import differences,decode,extract,main


NOKIA='/OIJEMH+Rotis.Sans.Serif.for.Nokia083'


class Stream(dict):
    def __init__(self,identity,instructions,**fields):
        super().__init__(fields)
        self.objgen=(identity,0)
        self.instructions=instructions
    @property
    def Resources(self):return self['/Resources']


def instruction(operator,*operands):
    return SimpleNamespace(operator=operator,operands=operands)


MODULE=SimpleNamespace(Dictionary=dict,String=bytes,parse_content_stream=lambda stream:stream.instructions)


class ManualTextTest(unittest.TestCase):
    def test_cli_refuses_an_unreviewed_pdf_before_optional_parser_import(self):
        with tempfile.TemporaryDirectory() as directory:
            source=Path(directory)/'other.pdf'
            source.write_bytes(b'unreviewed source')
            with patch('sys.argv',['tool',str(source)]),patch('sys.stderr',new_callable=io.StringIO) as output, \
                    self.assertRaises(SystemExit) as error:
                main()
            self.assertEqual(error.exception.code,1)
            self.assertIn('does not match the reviewed',output.getvalue())

    def test_declared_differences_recover_numbers_and_keep_real_spaces(self):
        glyphs=differences([1,'/G36','/G3','/G20','/G17',20,'/G16'])
        self.assertEqual(decode(bytes((1,2,3,4,20)),NOKIA,'',glyphs),('A 1.-',[]))
        digits=differences([10,*[f'/G{code}' for code in range(19,29)]])
        self.assertEqual(decode(bytes(range(10,20)),NOKIA,'',digits),('0123456789',[]))

    def test_normal_diagram_fonts_are_not_shifted(self):
        self.assertEqual(decode(b'13MHz 8bit Tx/12bit Rx','/Helvetica','/WinAnsiEncoding',{}),
                         ('13MHz 8bit Tx/12bit Rx',[]))
        text,unknown=decode(b'\x01','/Symbol','',{1:'/G36'})
        self.assertEqual(text,'<unmapped:01>')
        self.assertEqual(unknown,[(1,'/G36')])

    def test_unreviewed_extended_glyphs_and_missing_codes_stay_explicit(self):
        text,unknown=decode(b'\x01\x02\x03',NOKIA,'',{1:'/G167',2:'/G2'})
        self.assertEqual(text,'<unmapped:01><unmapped:02><unmapped:03>')
        self.assertEqual(len(unknown),3)
        text,unknown=decode(b'\x81','/Helvetica','/WinAnsiEncoding',{})
        self.assertEqual(text,'<unmapped:81>')
        self.assertEqual(len(unknown),1)

    def test_invalid_differences_fail_closed(self):
        for values in (['/G20'],[-1,'/G20'],[256,'/G20'],[255,'/G20','/G21'],[1,'not-a-name'],[True,'/G20']):
            with self.subTest(values=values),self.assertRaises(ValueError):differences(values)

    def test_font_switch_restore_and_nested_form_are_structured(self):
        nokia={'/BaseFont':NOKIA,'/Encoding':{'/Differences':[1,'/G36','/G20']}}
        normal={'/BaseFont':'/Helvetica','/Encoding':'/WinAnsiEncoding'}
        child=Stream(2,[instruction('Tf','/Child',1),instruction('Tj',b'8bit Tx/12bit Rx')],
                     **{'/Subtype':'/Form','/Resources':{'/Font':{'/Child':normal}}})
        page=Stream(1,[instruction('Tf','/Nokia',1),instruction('TJ',[b'\x01',-5,b'\x02']),
            instruction('q'),instruction('Tf','/Normal',1),instruction('Tj',b'13MHz'),instruction('Q'),
            instruction('Tj',b'\x02'),instruction('Do','/Form'),instruction('Tj',b'\x01')],
            **{'/Resources':{'/Font':{'/Nokia':nokia,'/Normal':normal},'/XObject':{'/Form':child}}})
        spans,counts=extract(SimpleNamespace(pages=[page]),MODULE)
        self.assertEqual([span['text'] for span in spans],['A1','13MHz','1','8bit Tx/12bit Rx','A'])
        self.assertTrue(all(span['page']==1 for span in spans))
        self.assertEqual(counts['unmapped_glyphs'],0)

    def test_recursive_forms_and_invalid_graphics_state_are_not_silently_skipped(self):
        child=Stream(2,[instruction('Do','/Loop')],**{'/Subtype':'/Form'})
        child['/Resources']={'/XObject':{'/Loop':child}}
        page=Stream(1,[instruction('Do','/Loop')],**{'/Resources':child['/Resources']})
        with self.assertRaisesRegex(ValueError,'recursive'):extract(SimpleNamespace(pages=[page]),MODULE)
        for operator in ('q','Q'):
            page=Stream(1,[instruction(operator)],**{'/Resources':{}})
            with self.assertRaisesRegex(ValueError,'unbalanced'):extract(SimpleNamespace(pages=[page]),MODULE)


if __name__=='__main__':unittest.main()

#!/usr/bin/env python3
"""Font-aware text evidence for the acquired NHM-3 System Module PDF.

The source-pinned Nokia font profile maps /G3../G97 to ASCII n+29. This is not
a general PDF repair or a substitute for visual review of tables/diagrams.
Uses optional pikepdf (available through the system Python on the RE host).
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import sys

SOURCE_SHA256='be8080882c1014d885d5dd504167e59bd919d14fe3c1898081d458d3e9059414'


def differences(values):
    result={}
    current=None
    for value in values:
        if type(value) is int:
            if not 0<=value<=255:raise ValueError('font code outside the simple-font range')
            current=value
        else:
            if current is None or current>255:raise ValueError('invalid PDF font Differences sequence')
            name=str(value)
            if not re.fullmatch(r'/[^\s/]+',name):raise ValueError('PDF font glyph must be a name')
            result[current]=name
            current+=1
    return result


def decode(raw,base_font,encoding,glyphs):
    text=[]
    unknown=[]
    nokia=('Rotis.' in base_font or 'Sabon.' in base_font) and '.for.Nokia' in base_font
    standard=base_font in ('/Helvetica','/Helvetica-Bold','/Times-Roman')
    for code in raw:
        glyph=glyphs.get(code)
        match=re.fullmatch(r'/G(\d+)',glyph or '')
        if nokia and match and 3<=int(match[1])<=97:
            text.append(chr(int(match[1])+29))
        elif standard and glyph is None and (32<=code<127 or encoding=='/WinAnsiEncoding'):
            try:
                value=bytes([code]).decode('cp1252')
                if not value.isprintable():raise UnicodeDecodeError('cp1252',bytes([code]),0,1,'control')
                text.append(value)
                continue
            except UnicodeDecodeError:pass
            text.append(f'<unmapped:{code:02x}>')
            unknown.append((code,glyph))
        else:
            text.append(f'<unmapped:{code:02x}>')
            unknown.append((code,glyph))
    return ''.join(text),unknown


def extract(pdf,module):
    spans=[]
    counts=Counter()
    def walk(stream,resources,page,font=None,stack=()):
        identity=stream.objgen
        if identity in stack:raise ValueError('recursive PDF Form XObject')
        if len(stack)>=16:raise ValueError('PDF Form depth exceeds reviewed bound')
        saved=[]
        for index,instruction in enumerate(module.parse_content_stream(stream)):
            operands,operator=instruction.operands,str(instruction.operator)
            if operator=='q':saved.append(font)
            elif operator=='Q':
                if not saved:raise ValueError('unbalanced PDF graphics-state restore')
                font=saved.pop()
            elif operator=='Tf':font=str(operands[0])
            elif operator=='Do':
                child=resources.get('/XObject',{}).get(str(operands[0]))
                if child is not None and str(child.get('/Subtype'))=='/Form':
                    walk(child,child.get('/Resources',resources),page,font,(*stack,identity))
            elif operator in ('Tj','TJ',"'",'"'):
                spec=resources.get('/Font',{}).get(font)
                if spec is None:raise ValueError('text operation has no font resource')
                encoding=spec.get('/Encoding','/StandardEncoding')
                glyphs=differences(encoding.get('/Differences',[])) if isinstance(encoding,module.Dictionary) else {}
                base=str(spec.get('/BaseFont',''))
                chunks=[item for item in operands[0] if isinstance(item,module.String)] if operator=='TJ' else [operands[-1]]
                raw=b''.join(bytes(item) for item in chunks)
                text,unknown=decode(raw,base,str(encoding),glyphs)
                counts['glyphs']+=len(raw)
                counts['unmapped_glyphs']+=len(unknown)
                spans.append({'page':page,'operation':index,'font':base,'text':text,
                              'unmapped':[{'code':code,'glyph':glyph} for code,glyph in unknown]})
        if saved:raise ValueError('unbalanced PDF graphics-state save')
    for number,page in enumerate(pdf.pages,1):
        walk(page,page.Resources,number)
    return spans,dict(counts)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pdf',type=Path)
    parser.add_argument('--json',action='store_true')
    args=parser.parse_args()
    try:
        digest=hashlib.sha256(args.pdf.read_bytes()).hexdigest()
        if digest!=SOURCE_SHA256:raise ValueError('PDF does not match the reviewed NHM-3 font-profile source')
        import pikepdf
        with pikepdf.open(args.pdf) as pdf:
            spans,counts=extract(pdf,pikepdf)
            pages=len(pdf.pages)
        if args.json:
            print(json.dumps({'source_sha256':digest,'pages':pages,'coverage':counts,'spans':spans},indent=2))
        else:
            for span in spans:print(f"page{span['page']:02d}: {span['text']}")
            print(json.dumps({'source_sha256':digest,'pages':pages,'coverage':counts}),file=sys.stderr)
    except (OSError,ValueError,ImportError) as error:
        parser.exit(1,f'NHM-3 manual text FAIL: {error}\n')


if __name__=='__main__':main()

import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT=Path(__file__).resolve().parents[1]


class RomStagingTests(unittest.TestCase):
    def run_recipe(self,root,env=None):
        source=(ROOT/'Makefile').read_text().splitlines(keepends=True)
        start=next(i for i,line in enumerate(source) if line.startswith('roms:'))
        end=start+1
        while end<len(source) and source[end].startswith('\t'):
            end+=1
        return subprocess.run(['make','--no-print-directory','-f','-','roms','PHONE=none','MAME_DIR=stage'],
                              input='.PHONY: roms\n'+''.join(source[start:end]),text=True,cwd=root,
                              env=env,capture_output=True)

    def seed(self,root):
        for name in ('nokifirst','nokisecond'):
            directory=root/'roms'/name
            directory.mkdir(parents=True)
            (directory/'image').write_bytes(name.encode())

    def test_read_only_generated_copy_can_be_refreshed(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            self.seed(root)
            source=root/'roms/nokifirst/image'
            source.chmod(0o444)
            first=self.run_recipe(root)
            self.assertEqual(first.returncode,0,first.stderr)
            destination=root/'stage/roms/nokifirst/image'
            self.assertEqual(destination.stat().st_mode&0o777,0o444)
            source.chmod(0o644)
            source.write_bytes(b'updated original')
            source.chmod(0o444)
            second=self.run_recipe(root)
            self.assertEqual(second.returncode,0,second.stderr)
            self.assertEqual(destination.read_bytes(),b'updated original')
            self.assertEqual(destination.stat().st_mode&0o777,0o444)

    def test_first_copy_failure_is_not_masked_by_later_success(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            self.seed(root)
            binary=root/'bin'
            binary.mkdir()
            script=binary/'cp'
            script.write_text('#!/bin/sh\nprintf "call\\n" >> "$CALLS"\nif [ "$(wc -l < "$CALLS")" -eq 1 ]; then exit 7; fi\nexit 0\n')
            script.chmod(0o755)
            calls=root/'calls'
            result=self.run_recipe(root,dict(os.environ,PATH=str(binary)+os.pathsep+os.environ['PATH'],CALLS=str(calls)))
            self.assertNotEqual(result.returncode,0)
            self.assertEqual(calls.read_text().splitlines(),['call'])

    def test_destination_creation_failure_stops_before_copy(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            self.seed(root)
            binary=root/'bin'
            binary.mkdir()
            for name,body in [('mkdir','exit 9'),('cp','touch "$COPIED"; exit 0')]:
                script=binary/name
                script.write_text('#!/bin/sh\n'+body+'\n')
                script.chmod(0o755)
            copied=root/'copied'
            result=self.run_recipe(root,dict(os.environ,PATH=str(binary)+os.pathsep+os.environ['PATH'],COPIED=str(copied)))
            self.assertNotEqual(result.returncode,0)
            self.assertFalse(copied.exists())


if __name__=='__main__':
    unittest.main()

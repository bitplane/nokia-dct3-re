"""Build private original SSNRCD and cross-check synthetic, exactly aligned metrics.

Neither the licensed source/dependency nor their binaries are part of MAME.
This does not determine AL09 alignment or establish native codec conformance.
"""
import argparse
import hashlib
from io import BytesIO
import json
from pathlib import Path
import re
import subprocess
import sys
import tarfile
import wave
from zipfile import ZipFile

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.aac_reference_metrics import compare

ISO_SHA='ba372c4069e9c96d0675e1442dbcb5a9c7f46d8ca760866bd51f185641ba759e'
AAC_SHA='c7037595f0152a0f8b3847a862789dcce1f8ac2c0b2b1d84f89e916434243f01'
AFSP_SHA='acf50ece855875be0233fc0848df02b106f93c7199d2410106ba27a47c2855c6'
SUMMARY=re.compile(r'^===  Test on RMS and abs\. max\. difference sample  : (OK|FAIL)$',re.M)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def require_hash(data,expected):
    if digest(data)!=expected:
        raise ValueError('original reference archive identity differs')


def read_result(text,returncode):
    verdicts=SUMMARY.findall(text)
    if (len(verdicts)!=1 or not re.search(r'version 1\.8(?:\s|$)',text) or
            text.count('Test on RMS and abs. max. difference sample')!=1 or
            returncode!=(0 if verdicts==['OK'] else 2)):
        raise ValueError('missing, duplicated or inconsistent original-tool verdict')
    return verdicts[0]=='OK'


def cases():
    """24-bit integer samples: no delay, gain fitting or random inputs."""
    zero=[0]*4096
    spike=lambda value: [value]+[0]*4095
    return [
        ('identical',1,zero,zero,True,True),
        ('small-error',1,zero,[32]*4096,True,True),
        ('rms-rejection',1,zero,[256]*4096,False,False),
        ('peak-rejection',1,zero,spike(1024),False,False),
        # SSNRCD's strict comparison differs only at exact maximum equality.
        ('peak-equality',1,zero,spike(512),True,False),
        ('stereo-second-channel',2,[0]*8192,[0,1024]+[0]*8190,False,False),
    ]


def write_pcm24(path,samples,channels):
    if (not isinstance(channels,int) or channels<1 or not samples or
            len(samples)%channels or any(not -(1<<23)<=v<(1<<23) for v in samples)):
        raise ValueError('invalid synthetic PCM24 domain')
    with wave.open(str(path),'wb') as output:
        output.setnchannels(channels)
        output.setsampwidth(3)
        output.setframerate(44100)
        output.writeframes(b''.join(v.to_bytes(3,'little',signed=True) for v in samples))


def verify_controls(binary,run):
    results=[]
    for name,channels,reference,native,expected_python,expected_original in cases():
        reference_path=run/(name+'-reference.wav')
        native_path=run/(name+'-test.wav')
        write_pcm24(reference_path,reference,channels)
        write_pcm24(native_path,native,channels)
        metrics=compare([v/(1<<23) for v in native],
                        [v/(1<<23) for v in reference],channels)
        python_pass=all(row['criterion_met'] for row in metrics['channels'])
        command=[str(binary),'-t','1','-k','16',str(reference_path),str(native_path)]
        result=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,
                              text=True,timeout=30)
        (run/(name+'.log')).write_text(result.stdout)
        original_pass=read_result(result.stdout,result.returncode)
        if (python_pass,original_pass)!=(expected_python,expected_original):
            raise ValueError(f'{name}: metric/control result differs')
        results.append(dict(name=name,channels=channels,frames=len(native)//channels,
            command=command,reference_sha256=digest(reference_path.read_bytes()),
            test_sha256=digest(native_path.read_bytes()),metrics=metrics,
            python_criterion_met=python_pass,original_criterion_met=original_pass,
            known_strict_peak_difference=name=='peak-equality'))
    return dict(controls=results,accuracy_bits=16,offset_samples=0,fitted_gain=False,
                fitted_delay=False,native_codec_conformance=False,
                al09_alignment_established=False,
                scope='Synthetic equal-interval metric controls only; legacy peak-equality difference retained')


def build(iso,afsp,run):
    outer=iso.read_bytes()
    dependency=afsp.read_bytes()
    require_hash(outer,ISO_SHA)
    require_hash(dependency,AFSP_SHA)
    with ZipFile(BytesIO(outer)) as archive:
        inner=archive.read('mpeg2aac.zip')
    require_hash(inner,AAC_SHA)
    if run.exists():
        raise ValueError('fresh original-tool output directory required')
    run.mkdir(parents=True)
    with ZipFile(BytesIO(inner)) as archive:
        # Exact archive identity is mandatory; still reject path escape.
        if any(Path(n).is_absolute() or '..' in Path(n).parts for n in archive.namelist()):
            raise ValueError('unsafe original archive member')
        archive.extractall(run/'original')
    with tarfile.open(fileobj=BytesIO(dependency),mode='r:gz') as archive:
        archive.extractall(run/'dependency',filter='data')
    library=run/'dependency/AFsp-v6r8'
    source=run/'original/mpeg2aac/conf_ssnrcd/src'
    binary=run/'ssnrcd'
    names=('ssnrcd.c','audiofile_io.c','console_io.c','cepstrum_analysis.c')
    commands=[['make','-C',str(library),'libtsp'],
        ['gcc','-O2','-std=gnu89','-fcommon','-I',str(library/'include'),
         *(str(source/name) for name in names),str(library/'lib/libtsp.a'),'-lm','-o',str(binary)]]
    with (run/'build.log').open('w') as log:
        for command in commands:
            subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180)
    provenance=dict(iso_archive_sha256=ISO_SHA,aac_archive_sha256=AAC_SHA,
        dependency_archive_sha256=AFSP_SHA,commands=commands,
        source_sha256={p.name:digest(p.read_bytes()) for p in sorted(source.iterdir()) if p.is_file()},
        compiler_version=subprocess.run(['gcc','--version'],check=True,stdout=subprocess.PIPE,
            text=True,timeout=30).stdout.splitlines()[0],binary_sha256=digest(binary.read_bytes()),
        original_source_modified=False,mame_dependency=False)
    (run/'provenance.json').write_text(json.dumps(provenance,indent=2)+'\n')
    return binary


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('iso_archive',type=Path)
    parser.add_argument('afsp_archive',type=Path)
    parser.add_argument('run',type=Path)
    args=parser.parse_args()
    try:
        run=args.run.resolve()
        binary=build(args.iso_archive.resolve(),args.afsp_archive.resolve(),run)
        result=verify_controls(binary,run)
        if digest(binary.read_bytes())!=json.loads((run/'provenance.json').read_text())['binary_sha256']:
            raise ValueError('original comparison executable changed during controls')
        (run/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    except (OSError,ValueError,KeyError,subprocess.SubprocessError) as error:
        parser.exit(1,f'Original AAC comparison controls: FAIL: {error}\n')
    print('Original SSNRCD controls PASS; strict peak-equality difference retained; no native conformance claim')


if __name__=='__main__':
    main()

"""Run explicit ADC-pin tone/silence candidates, never an RF/camping gate."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import subprocess

ROOT=Path(__file__).resolve().parents[1]
if __package__ in (None,''): sys.path.insert(0,str(ROOT))
from tools.run_rom6_native import prepare,execute
from tools.rom6_adc_lab_check import check
from tools.rom6_adc_filter_check import check as check_filter,check_mixer
from tools.rom6_adc_measure_check import check as check_measure,check_frequency_correction,check_rx_publications
from tools.rom6_adc_detect_check import check as check_detect


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--machine',choices=('nsm3adc','nsm3ad0'),required=True)
    parser.add_argument('--seconds',type=int,default=6)
    parser.add_argument('--measure',action='store_true',help='Check the bounded native raw log-power/RSSI prefix')
    parser.add_argument('--detect',action='store_true',help='Observe the bounded native search complex products')
    args=parser.parse_args()
    if not 3<=args.seconds<=30: parser.error('ADC-pin trials are bounded to 3..30 seconds')
    if args.measure and args.seconds<4: parser.error('measurement prefix needs at least four seconds')
    if args.measure and args.detect: parser.error('measurement and detection observers use separate bounded runs')
    root=ROOT/'tmp/rom6-adc-lab'
    root.mkdir(exist_ok=True)
    run=Path(tempfile.mkdtemp(prefix=args.machine+'-',dir=root))
    print(f'ADC lab evidence: {run}',flush=True)
    try:
        prepare(run,(ROOT/'roms/noki8210/8210_5.31ppm_c.fls').read_bytes(),
                (ROOT/'roms/noki8210/8210 virgin eeprom 003d0000.fls').read_bytes(),machine=args.machine)
        script='rom6_adc_detect.lua' if args.detect else 'rom6_adc_measure.lua' if args.measure else 'rom6_adc_lab.lua'
        directory,text=execute(run,'primary',script,args.seconds,ROOT/'mame/mame',machine=args.machine)
        report=check(text,args.machine=='nsm3adc')
        report['native_filter']=check_filter(text,
            (directory/'rom6_adc_processing_program.be16').read_bytes(),
            (ROOT/'roms/noki8210/8210_5.31ppm_c.fls').read_bytes(),
            (ROOT/'roms/nsm3r6/nsm3_rom6_program.be16').read_bytes())
        report['native_mixer']=check_mixer(text)
        if args.detect:
            report['native_detector_products']=check_detect(text,
                (directory/'rom6_adc_processing_program.be16').read_bytes(),
                (ROOT/'roms/noki8210/8210_5.31ppm_c.fls').read_bytes(),
                (ROOT/'roms/nsm3r6/nsm3_rom6_program.be16').read_bytes())
        if args.measure:
            report['native_measurement']=check_measure(text,
                (directory/'rom6_adc_processing_program.be16').read_bytes(),
                (ROOT/'roms/noki8210/8210_5.31ppm_c.fls').read_bytes(),
                (ROOT/'roms/nsm3r6/nsm3_rom6_program.be16').read_bytes(),
                (ROOT/'roms/nsm3r6/nsm3_rom6_data.be16').read_bytes(),args.machine=='nsm3adc')
            correction=check_frequency_correction(text)
            if correction['joined_raw_measurements']!=report['native_measurement']['raw_rssi_stores']:
                raise ValueError('frequency correction does not cover the qualified raw measurements')
            report['native_measurement']['frequency_correction']=correction
            report['native_measurement']['post_correction_qualified']=True
            report['native_rx_publications']=check_rx_publications(text)
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        parser.exit(1,f'ADC lab rejected: {error}\n')
    report.update(machine=args.machine,seconds=args.seconds,native_initialization_verified=True,
                  mame_sha256=hashlib.sha256((ROOT/'mame/mame').read_bytes()).hexdigest())
    (run/'sample-prefix.json').write_text(json.dumps(report,indent=2)+'\n')
    if not report['transport_fault_free']:
        parser.exit(1,'ADC lab rejected: raw transport fault; no decoder qualification\n')
    print(f"ADC lab: no raw faults with provisional {report['provisional_staging_words']}-word staging "
          f"(peak {report['peak_queued_words']}); physical receiver remains unqualified")


if __name__=='__main__': main()

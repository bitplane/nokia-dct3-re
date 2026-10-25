"""Linked physical PUK1 exhaustion and cold credential refusal."""
import hashlib
import json
from PIL import Image

from tools.run_nse2_native import prepare_native
from tools.process_runner import run_stage
from tools.nse2_puk1_check import check_wrong, check_permanent, input_remaining


def exhaust(root,run,source,mame):
    if input_remaining(source.read_bytes())!=10:
        raise ValueError('PUK1 exhaustion requires ten remaining attempts')
    run.mkdir(parents=True,exist_ok=False)
    with mame.open('rb') as stream:
        executable_hash=hashlib.file_digest(stream,'sha256').hexdigest()
    stages=[]
    for index in range(1,12):
        cold=index==11
        target=run/('cold' if cold else f'wrong{index:02}')
        before=source.read_bytes()
        report=prepare_native(root,target,record_checksum=True,nominal_vbat=True,insert_sim=True)
        (target/'initial_sim_card.bin').write_bytes(before)
        (target/'nvram/nse2r4v/sim_card').write_bytes(before)
        # Reuse the same physical credential attempt after permanent loss:
        # firmware must refuse it rather than issue another UNBLOCK.
        command=[str(mame),'nse2r4v','-rompath',str(target/'roms'),
                 '-nvram_directory',str(target/'nvram'),'-cfg_directory',str(target/'cfg'),
                 '-video','none','-sound','none','-joystickprovider','none','-midiprovider','none',
                 '-nothrottle','-noreadconfig','-log','-verbose','-seconds_to_run','28',
                 '-autoboot_delay','0','-autoboot_script',str(root/'tools/nse2_puk_wrong_fixture.lua')]
        report.update(command=command,card_source=str(source),card_sha256=hashlib.sha256(before).hexdigest(),
                      mame_sha256=executable_hash)
        (target/'inputs.json').write_text(json.dumps(report,indent=2)+'\n')
        with (target/'console.log').open('w') as stream:
            run_stage(command,target,stream)
        console=(target/'console.log').read_text(errors='replace')
        if 'LUA ERROR' in console or 'Error in' in console:
            raise ValueError('PUK1 exhaustion fixture failed')
        text=(target/'error.log').read_text(errors='replace')
        after=(target/'nvram/nse2r4v/sim_card').read_bytes()
        with Image.open(target/'snap/nse2_puk_probe_1.png') as prompt, Image.open(target/'snap/nse2_puk_probe_5.png') as result:
            if prompt.size!=(84,48) or result.size!=(84,48):
                raise ValueError('PUK1 exhaustion frame geometry differs')
            checker=check_permanent if cold else check_wrong
            checked=checker(text,before,after,prompt.convert('L').tobytes(),result.convert('L').tobytes())
        if not cold and checked['puk1_attempts_remaining']!=10-index:
            raise ValueError('PUK1 exhaustion skipped a predecessor retry')
        (target/'acceptance.json').write_text(json.dumps(checked,indent=2)+'\n')
        stages.append(dict(name=target.name,source=str(source),source_sha256=hashlib.sha256(before).hexdigest(),**checked))
        (run/'stages.json').write_text(json.dumps(stages,indent=2)+'\n')
        print(f'NSE-2 PUK1 {target.name} PASS',flush=True)
        source=target/'nvram/nse2r4v/sim_card'
    result=dict(physical_wrong_puk1_attempts=10,cold_permanent_refusal=True,
                native_registration_verified=False)
    (run/'acceptance.json').write_text(json.dumps(result,indent=2)+'\n')
    return result

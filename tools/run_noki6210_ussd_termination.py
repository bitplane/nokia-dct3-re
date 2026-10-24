#!/usr/bin/env python3
"""Fresh own-PMM 6210 network-USSD termination through the real host boundary."""
import argparse
import asyncio
import hashlib
import json
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.make_sim_card_profile import make_profile
from tools.run_host_call_adapter_gate import connect
from tools.run_host_ussd_gate import pack_gsm7
from tools.run_noki6210_acceptance import build_command, assess, verify_radio_contract
from tools.run_sip_handset_gate import verify_handset_health
from tools.noki6210_ussd_termination_check import verify
from tools.noki6210_host_ussd_check import check_card_authentication, check_admission, FRAMES
from tools.noki6210_puk_check import check_rgb_frame, contacts
from websockets.exceptions import ConnectionClosed


async def run(directory,restore=False):
    root=Path(__file__).resolve().parents[1]
    mcu=(root/'roms/noki6210/6210_556c.fls').read_bytes()
    assess(mcu,(root/'roms/noki6210/6210 virgin eeprom 005fa000.fls').read_bytes())
    verify_radio_contract(mcu)
    directory.mkdir(parents=True,exist_ok=False)
    card=directory/'nvram/npe3hle/sim_card'
    card.parent.mkdir(parents=True)
    card.write_bytes(make_profile(pin_enabled=True))
    (directory/'cfg').mkdir()
    cfg=ET.Element('mameconfig',version='10')
    ports=ET.SubElement(ET.SubElement(cfg,'system',name='npe3hle'),'input')
    for tag,mask in ((':CALLHOST','1'),(':NEIGHBORCFG','1024')):
        ET.SubElement(ports,'port',tag=tag,type='CONFIG',mask=mask,defvalue='0',value=mask)
    ET.ElementTree(cfg).write(directory/'cfg/npe3hle.cfg')
    script='host_ussd_terminate_restore' if restore else 'host_ussd_terminate'
    command=build_command(root,None,'npe3hle',script,62)
    command+=['-http','-http_port','16210']
    (directory/'launch.json').write_text(json.dumps({'command':command,
        'mame_sha256':hashlib.sha256(Path(command[0]).read_bytes()).hexdigest(),
        'restore':restore,
        'script_sha256':hashlib.sha256((root/f'tools/noki6210_{script}.lua').read_bytes()).hexdigest()},indent=2)+'\n')
    process=await asyncio.create_subprocess_exec(*command,cwd=directory,
        stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.STDOUT)
    ready,terminate_ready=asyncio.Event(),asyncio.Event()
    async def drain():
        chunks=[]
        while line:=await process.stdout.readline():
            chunks.append(line)
            if b'6210_host_ussd: ready' in line:ready.set()
            if b'6210_host_ussd: terminate ready' in line:terminate_ready.set()
        return b''.join(chunks).decode(errors='replace')
    console=asyncio.create_task(drain())
    termination=None
    events=[]
    try:
        async with asyncio.timeout(180):
            socket=await connect(16210,process)
            async with socket:
                async def receive():
                    event=json.loads(await socket.recv())
                    events.append(event)
                    return event
                epoch=None
                while True:
                    event=await receive()
                    if event.get('type')=='call_adapter_ready':epoch=event['epoch']
                    if event.get('type')=='network_state' and event.get('registered'):break
                if epoch != 1:raise ValueError('fresh host epoch is not one')
                await ready.wait()
                await socket.send(json.dumps(dict(type='incoming_ussd',epoch=epoch,
                    request_id=1,operation='request',dcs=15,data=pack_gsm7('Choose 2'))))
                async def terminate():
                    await terminate_ready.wait()
                    if epoch != (2 if restore else 1):
                        raise ValueError('termination did not observe required epoch')
                    stale=epoch-1 if restore else epoch+1
                    for e,r in ((stale,1),(epoch,999),(epoch,1),(epoch,1)):
                        await socket.send(json.dumps(dict(type='incoming_ussd_terminate',epoch=e,request_id=r)))
                        await asyncio.sleep(0.1)
                termination=asyncio.create_task(terminate())
                try:
                    while True:
                        event=await receive()
                        if event.get('type')=='call_adapter_ready':
                            epoch=event['epoch']
                except ConnectionClosed:
                    # MAME closes its WebSocket before its process handle settles.
                    if await asyncio.wait_for(process.wait(),5):
                        raise ValueError('MAME failed at endpoint shutdown')
                await termination
    finally:
        if termination and not termination.done():
            termination.cancel()
            await asyncio.gather(termination,return_exceptions=True)
        if process.returncode is None:
            process.terminate()
            await process.wait()
        output=await console
        (directory/'console.log').write_text(output)
        (directory/'host_events.json').write_text(json.dumps(events,indent=2)+'\n')
    verify_handset_health(output)
    text=(directory/'error.log').read_text(errors='replace')
    verify(text,events,restore=restore)
    check_card_authentication(text,card.read_bytes())
    check_admission(text,'cancelled',restored=restore)
    contacts(text,'6210_host_ussd_input',[(8,'Keypad 1'),(9,'Keypad 2'),(10,'Keypad 3'),
        (11,'Keypad 4'),(12,'Left Softkey / Menu'),(26,'Left Softkey / Menu'),
        (30,'End'),(56,'Left Softkey / Menu')])
    if restore and not any(p.stat().st_size for p in directory.rglob('npe3_ussd_termination.sta')):
        raise ValueError('missing termination checkpoint artifact')
    frame=directory/'snap/6210_host_ussd_terminated_menu.png'
    if not frame.is_file():raise ValueError('missing physical post-release Menu capture')
    check_rgb_frame(frame,FRAMES['6210_host_ussd_key_30.png'])
    print('OK - correlated network SS/RR termination; no handset result fabricated')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--restore',action='store_true')
    args=parser.parse_args()
    try:asyncio.run(run(args.directory.resolve(),args.restore))
    except (ValueError,RuntimeError,TimeoutError,ConnectionClosed) as error:
        parser.exit(1,f'FAIL - {error}\n')

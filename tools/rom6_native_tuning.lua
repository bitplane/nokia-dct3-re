-- Read-only resident formatter entries; no serial/RF response is supplied.
local directory=assert(debug.getinfo(1,'S').source:sub(2):match('^(.*[/])'))
dofile(directory..'rom6_native_radio.lua')
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
machine.debugger.visible_cpu=cpu
machine.debugger:command('temp6=0;temp7=0')
for _,address in ipairs({0x388c,0x389c}) do
    cpu.debug:bpset(address,'temp6<10000',string.format(
        'temp6=temp6+1;temp7=a;logerror "rom6_tuning_entry: sequence=%%d address=%04x a=%%010x ar2=%%04x flags3e7e=%%04x flags3ec8=%%04x flags051f=%%04x addend3e7f=%%04x addend3e80=%%04x observer_writes=0\\n",temp6,a,ar2,dw@3e7e,dw@3ec8,dw@051f,dw@3e7f,dw@3e80;g',address))
end
local selected='temp6>0 && temp6<10000 && (temp6<=4 || temp7==1 || temp7==7c || temp7==200 || temp7==346 || temp7==375 || temp7==3cf || temp7==3ff)'
for _,address in ipairs({0x38aa,0x38c9,0x38de,0x38f4,0x38f9,0x38c8,0x3906,0x3969}) do
    cpu.debug:bpset(address,selected,string.format(
        'logerror "rom6_tuning_stage: sequence=%%d address=%04x a=%%010x b=%%010x ar2=%%04x ar3=%%04x field60=%%04x st0=%%04x st1=%%04x observer_writes=0\\n",temp6,a,b,ar2,ar3,dw@60,st0,st1;g',address))
end
_G.rom6_tuning_stop=emu.add_machine_stop_notifier(function()
    machine.debugger.visible_cpu=cpu
    machine.debugger:command('logerror "rom6_tuning_complete: entries=%d cap=65536 observer_writes=0\\n",temp6')
end)

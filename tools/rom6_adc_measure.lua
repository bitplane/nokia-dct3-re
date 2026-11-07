-- Bounded Execute-entry observations of the native raw log-power path.
local directory=assert(debug.getinfo(1,'S').source:sub(2):match('^(.*[/])'))
dofile(directory..'rom6_adc_lab.lua')
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
machine.debugger.visible_cpu=cpu
machine.debugger:command('temp5=0;temp6=0')
cpu.debug:bpset(0x4317,'temp5<20',
    'temp5=temp5+1;temp6=1;logerror "rom6_measure_input: sequence=%d a=%010x ar2=%04x calibration=%04x flags3ec8=%04x flags263b=%04x st1=%04x observer_writes=0\\n",temp5,a,ar2,dw@ar2,dw@3ec8,dw@263b,st1;g')
cpu.debug:bpset(0x4347,'temp6==1',
    'temp6=0;logerror "rom6_measure_output: sequence=%d a=%010x b=%010x ar2=%04x calibration=%04x st1=%04x observer_writes=0\\n",temp5,a,b,ar2,dw@ar2,st1;g')
-- Exclude the final captured call so this condition cannot keep logging its
-- ordinal after the hexadecimal 20-entry counter has stopped incrementing.
cpu.debug:bpset(0x242f,'temp5>0 && temp5<20',
    'logerror "rom6_measure_rssi_return: sequence=%d b=%010x observer_writes=0\\n",temp5,b;g')
_G.rom6_measure_stop=emu.add_machine_stop_notifier(function()
    machine.debugger.visible_cpu=cpu
    machine.debugger:command('logerror "rom6_measure_complete: calls=%d pending=%d cap=32 observer_writes=0\\n",temp5,temp6')
end)

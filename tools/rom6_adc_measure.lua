-- Bounded Execute-entry observations of the native raw log-power path.
local directory=assert(debug.getinfo(1,'S').source:sub(2):match('^(.*[/])'))
dofile(directory..'rom6_adc_lab.lua')
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
machine.debugger.visible_cpu=cpu
machine.debugger:command('temp5=0;temp6=0;temp7=0;temp8=0')
cpu.debug:bpset(0x4317,'temp5<20',
    'temp5=temp5+1;temp6=1;logerror "rom6_measure_input: sequence=%d a=%010x ar2=%04x calibration=%04x flags3ec8=%04x flags263b=%04x st1=%04x observer_writes=0\\n",temp5,a,ar2,dw@ar2,dw@3ec8,dw@263b,st1;g')
cpu.debug:bpset(0x4347,'temp6==1',
    'temp6=0;logerror "rom6_measure_output: sequence=%d a=%010x b=%010x ar2=%04x calibration=%04x st1=%04x observer_writes=0\\n",temp5,a,b,ar2,dw@ar2,st1;g')
-- Exclude the final captured call so this condition cannot keep logging its
-- ordinal after the hexadecimal 20-entry counter has stopped incrementing.
cpu.debug:bpset(0x242f,'temp5>0 && temp5<20',
    'logerror "rom6_measure_rssi_return: sequence=%d b=%010x observer_writes=0\\n",temp5,b;g')
cpu.debug:bpset(0x71a5,'temp7<10',
    'temp7=temp7+1;temp8=1;logerror "rom6_measure_correct_input: sequence=%d key=%04x pointer=%04x raw=%04x flags=%04x main=%04x,%04x,%04x,%04x other=%04x,%04x,%04x,%04x st1=%04x observer_writes=0\\n",temp7,dw@ar2,ar3,dw@ar3,dw@51f,dw@3ea1,dw@3ea2,dw@3ea3,dw@3ea4,dw@3ed6,dw@3ed7,dw@3ed8,dw@3ed9,st1;g')
cpu.debug:bpset(0x71d7,'temp8==1',
    'temp8=0;logerror "rom6_measure_correct_output: sequence=%d pointer=%04x value=%04x st1=%04x observer_writes=0\\n",temp7,ar3,dw@ar3,st1;g')
_G.rom6_measure_stop=emu.add_machine_stop_notifier(function()
    machine.debugger.visible_cpu=cpu
    machine.debugger:command('logerror "rom6_measure_complete: calls=%d pending=%d cap=32 observer_writes=0\\n",temp5,temp6')
    machine.debugger:command('logerror "rom6_measure_correct_complete: calls=%d pending=%d cap=16 observer_writes=0\\n",temp7,temp8')
end)

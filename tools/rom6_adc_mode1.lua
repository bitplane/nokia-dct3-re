-- Nonzero native mode-1 product observations; no firmware writes or calls.
local directory=assert(debug.getinfo(1,'S').source:sub(2):match('^(.*[/])'))
dofile(directory..'rom6_adc_lab.lua')
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
machine.debugger.visible_cpu=cpu
machine.debugger:command('temp5=0')
-- Debugger integer literals are hexadecimal; keep the 100-phase cap explicit.
cpu.debug:bpset(0x422b,'temp5<0x64 && dw@56e==1 && (dw@2f52|dw@2f53|dw@2f54|dw@2f55)!=0',
    'temp5=temp5+1;logerror "rom6_mode1_product_input: phase=%d values=%04x,%04x,%04x,%04x st1=%04x observer_writes=0\\n",temp5,dw@2f52,dw@2f53,dw@2f54,dw@2f55,st1;g')
for _,address in ipairs({0x41a0,0x41fc}) do
    cpu.debug:bpset(address,'(temp5&1)==1',
        'logerror "rom6_mode1_product_output: phase=%d a=%010x b=%010x ar2=%04x ar3=%04x word2f51=%04x words=%04x,%04x,%04x,%04x st1=%04x observer_writes=0\\n",temp5,a,b,ar2,ar3,dw@2f51,dw@2f52,dw@2f53,dw@2f54,dw@2f55,st1;temp5=temp5+1;g')
end
_G.rom6_mode1_stop=emu.add_machine_stop_notifier(function()
    machine.debugger.visible_cpu=cpu
    machine.debugger:command('logerror "rom6_mode1_complete: phases=%d cap=100 observer_writes=0\\n",temp5')
end)

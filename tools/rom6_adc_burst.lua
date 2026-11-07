-- Finite raw ADC tone bursts, never decoded radio messages or success flags.
local directory=assert(debug.getinfo(1,'S').source:sub(2):match('^(.*[/])'))
dofile(directory..'rom6_adc_measure.lua')
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
local writes=0
_G.rom6_burst_state_tap=cpu.spaces.data:install_write_tap(0x3044,0x3044,'rom6_burst_positive_count',function(_,value,mask)
    writes=writes+1
    if writes<=1024 then
        machine:logerror(string.format('rom6_burst_count: sequence=%d t=%.9f value=%04x mask=%04x cpu_pc=%06x observer_writes=0\n',
            writes,machine.time:as_double(),value,mask,cpu.state.GENPC.value))
    end
end)
_G.rom6_burst_stop=emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('rom6_burst_complete: count_writes=%d cap=1024 observer_writes=0\n',writes))
    machine:logerror(string.format('rom6_burst_terminal: pc=%06x ar2=%04x ar3=%04x ar4=%04x ar5=%04x a=%010x b=%010x observer_writes=0\n',
        cpu.state.GENPC.value,cpu.state.AR2.value,cpu.state.AR3.value,cpu.state.AR4.value,cpu.state.AR5.value,cpu.state.A.value,cpu.state.B.value))
    machine.debugger.visible_cpu=cpu
    machine.debugger:command('dasm rom6_burst_search.asm,1c80,c0,1,:dsp_c54x:cpu')
end)

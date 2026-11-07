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
end)

-- Cold NVRAM reopening: inspect and cancel, never confirm a clock/alarm edit.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
_G.nsm1_toolkit_observe_only=true
dofile(directory..'nsm1_toolkit_input.lua')
local machine=manager.machine
local until_time=dofile(directory..'fixture_deadline.lua')
local key=dofile(directory..'nsm1_clock_key.lua')('nsm1_clock_cold_contact')
local actions=coroutine.create(function()
    local steps={{25,'Left Softkey','main'},{27,'Keypad 4','settings'},
        {29,'Left Softkey','alarm_value'},{31,'Right Softkey','alarm_back'},
        {33,'Down','clock'},{35,'Left Softkey','clock_options'},
        {37,'Down','set_time'},{39,'Left Softkey','time_editor'},
        {41,'Right Softkey','time_back'},{43,'End / Mode','idle'}}
    for _,step in ipairs(steps) do
        until_time(step[1]); key(step[2]); until_time(step[1]+1)
        machine.screens[':screen']:snapshot('clock_cold_'..step[3]..'.png')
    end
    until_time(50)
    machine.screens[':screen']:snapshot('clock_cold_minute.png')
    until_time(60)
    machine.screens[':screen']:snapshot('clock_cold_final.png')
    until_time(61.5)
    local dsp=machine.devices[':dsp_c54x:cpu']
    machine:logerror(string.format('nsm1_clock_cold_native: pc=%04x stop=%d t=%.9f observer_writes=0\n',
        dsp.state.PC.value,dsp.state.ILLEGAL.value,machine.time:as_double()))
end)
assert(coroutine.resume(actions))

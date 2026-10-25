-- Physical initialization and alarm control; all other observations are reads.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
_G.nsm1_toolkit_observe_only=true
dofile(directory..'nsm1_toolkit_input.lua')
local machine=manager.machine
local until_time=dofile(directory..'fixture_deadline.lua')
local key=dofile(directory..'nsm1_clock_key.lua')('nsm1_clock_contact')
local actions=coroutine.create(function()
    local steps={{25,'Left Softkey','main'},{27,'Keypad 4','settings'},
        {29,'Left Softkey','clock_required'},{31,'Left Softkey','time_editor'},
        {33,'Keypad 1','time_1'},{34,'Keypad 2','time_12'},
        {35,'Keypad 3','time_123'},{36,'Keypad 4','time_1234'},
        {37,'Left Softkey','time_confirm'},
        {41,'Keypad 0','date_0'},{42,'Keypad 1','date_01'},
        {43,'Keypad 0','date_010'},{44,'Keypad 1','date_0101'},
        {45,'Keypad 2','date_01012'},{46,'Keypad 0','date_010120'},
        {47,'Keypad 0','date_0101200'},{48,'Keypad 0','date_01012000'},
        {49,'Left Softkey','date_confirm'},
        {55,'Keypad 1','alarm_1'},{56,'Keypad 2','alarm_12'},
        {57,'Keypad 3','alarm_123'},{58,'Keypad 5','alarm_1235'},
        {59,'Left Softkey','alarm_confirm'},{63,'End / Mode','idle'}}
    if not _G.nsm1_clock_keep_alarm then
        steps[#steps+1]={105,'Left Softkey','alarm_stop'}
    end
    for _,step in ipairs(steps) do
        until_time(step[1]); key(step[2]); until_time(step[1]+1)
        machine.screens[':screen']:snapshot('clock_'..step[3]..'.png')
    end
    until_time(110)
    machine.screens[':screen']:snapshot('clock_final.png')
    until_time(111.5)
    local dsp=machine.devices[':dsp_c54x:cpu']
    machine:logerror(string.format('nsm1_clock_native: pc=%04x stop=%d t=%.9f observer_writes=0\n',
        dsp.state.PC.value,dsp.state.ILLEGAL.value,machine.time:as_double()))
end)
assert(coroutine.resume(actions))
local watch=coroutine.create(function()
        for _,time in ipairs({65,95,99,100,102,104,108}) do
            until_time(time)
            machine.screens[':screen']:snapshot('clock_watch_'..time..'.png')
        end
end)
assert(coroutine.resume(watch))

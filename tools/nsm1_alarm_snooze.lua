-- Physical Snooze followed by Stop at the firmware-programmed second alarm.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
_G.nsm1_clock_keep_alarm=true
_G.nsm1_toolkit_wire_limit=32768
dofile(directory..'nsm1_clock_alarm.lua')
local machine=manager.machine
local until_time=dofile(directory..'fixture_deadline.lua')
local key=dofile(directory..'nsm1_clock_key.lua')('nsm1_alarm_snooze_contact')
local actions=coroutine.create(function()
    until_time(105); key('Right Softkey')
    until_time(107); machine.screens[':screen']:snapshot('snooze_selected.png')
    for _,time in ipairs({120,300,450,458,460,464}) do
        until_time(time); machine.screens[':screen']:snapshot('snooze_'..time..'.png')
    end
    until_time(465); key('Left Softkey')
    until_time(470); machine.screens[':screen']:snapshot('snooze_final.png')
    until_time(479.5)
    local dsp=machine.devices[':dsp_c54x:cpu']
    machine:logerror(string.format('nsm1_alarm_snooze_native: pc=%04x stop=%d t=%.9f observer_writes=0\n',
        dsp.state.PC.value,dsp.state.ILLEGAL.value,machine.time:as_double()))
end)
assert(coroutine.resume(actions))

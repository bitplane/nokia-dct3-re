-- Physical shutdown, retained-RTC alarm wake, and activation choice.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
_G.nsm1_clock_keep_alarm=true
-- Two authenticated boot epochs exceed the single-boot receive observation cap.
_G.nsm1_toolkit_wire_limit=16384
dofile(directory..'nsm1_clock_alarm.lua')
local machine=manager.machine
local until_time=dofile(directory..'fixture_deadline.lua')
local key=dofile(directory..'nsm1_clock_key.lua')('nsm1_alarm_power_contact')
local power
for _,field in pairs(machine.ioport.ports[':PWR'].fields) do
    if field.mask==1 then power=field end
end
assert(power,'physical power input missing')
local function snapshot(name)
    machine.screens[':screen']:snapshot('alarm_power_'..name..'.png')
end
local actions=coroutine.create(function()
    until_time(70); power:set_value(1)
    machine:logerror('nsm1_alarm_power_contact: name=Power down=1 t=70.000000000 physical_only=1\n')
    until_time(72); power:set_value(0)
    machine:logerror('nsm1_alarm_power_contact: name=Power down=0 t=72.000000000 physical_only=1\n')
    until_time(115); key('Left Softkey')
    until_time(117); snapshot('stop')
    until_time(119); key(_G.nsm1_alarm_activate_yes and 'Left Softkey' or 'Right Softkey')
    until_time(121); snapshot('choice')
    until_time(125); snapshot('final')
    if _G.nsm1_alarm_activate_yes then
        for i,name in ipairs({'Keypad 1','Keypad 2','Keypad 3','Keypad 4','Left Softkey'}) do
            until_time(127+(i-1)*.5); key(name)
        end
        until_time(131); snapshot('after_pin')
        until_time(134); snapshot('after_pin_late')
        for i,name in ipairs({'Keypad 1','Keypad 2','Keypad 3','Keypad 4','Keypad 5','Left Softkey'}) do
            until_time(135+(i-1)*.5); key(name)
        end
        -- Accepted-editor residue is an observation, not a clean-idle oracle.
        until_time(140); snapshot('accepted_editor')
        until_time(143); key('Left Softkey')
        until_time(144); snapshot('menu')
        until_time(145); key('Down')
        until_time(146); snapshot('menu_down')
        until_time(150); snapshot('menu_final')
        until_time(151.5)
        local dsp=machine.devices[':dsp_c54x:cpu']
        machine:logerror(string.format('nsm1_alarm_power_native: pc=%04x stop=%d t=%.9f observer_writes=0\n',
            dsp.state.PC.value,dsp.state.ILLEGAL.value,machine.time:as_double()))
    else
        until_time(139); snapshot('off_final')
    end
end)
assert(coroutine.resume(actions))
local watch=coroutine.create(function()
    for _,time in ipairs({74,80,96,99,102,104,108,112}) do
        until_time(time); snapshot(time)
    end
end)
assert(coroutine.resume(watch))

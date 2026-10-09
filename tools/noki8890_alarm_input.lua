-- Own-ROM physical Settings 4-1 alarm entry; no clock or firmware writes.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_clock_read.lua')
local machine = manager.machine
local input = coroutine.create(function()
    assert(emu.wait(22))
    local function press(column, name, action)
        local key = assert(machine.ioport.ports[':COL.' .. column].fields[name])
        machine:logerror('8890_alarm_physical: action=' .. action .. '\n')
        key:set_value(1)
        assert(emu.wait(0.15))
        key:set_value(0)
        assert(emu.wait(0.85))
        machine.screens[':screen']:snapshot('8890_alarm_' .. action .. '.png')
    end
    press(1, 'Menu', 'menu')
    for index = 2, 4 do press(1, 'Scroll Down', 'menu_' .. index) end
    press(1, 'Menu', 'settings')
    press(1, 'Menu', 'alarm')
    for index, item in ipairs({{2, 'Keypad 1'}, {4, 'Keypad 3'},
                              {2, 'Keypad 4'}, {3, 'Keypad 8'}}) do
        press(item[1], item[2], 'time_' .. index)
    end
    press(1, 'Menu', 'confirm')
    press(0, 'End', 'idle')
    if _G.noki8890_alarm_power_off then
        local power = assert(machine.ioport.ports[':PWR'].fields['Power'])
        machine:logerror('8890_alarm_physical: action=power_off\n')
        power:set_value(1)
        assert(emu.wait(4))
        power:set_value(0)
        machine:logerror('8890_alarm_physical: action=power_release\n')
        assert(emu.wait(7))
        machine.screens[':screen']:snapshot('8890_alarm_powered_off.png')
        -- Re-arm debugger-only caps for the independent alarm activation boot.
        machine.debugger:command('do temp6=0;do temp7=0;do temp8=0;do temp9=0')
        assert(emu.wait(16))
    else
        assert(emu.wait(27))
    end
    for index = 1, 4 do
        machine.screens[':screen']:snapshot('8890_alarm_expiry_' .. index .. '.png')
        local security_delay = 0
        if index == 3 and _G.noki8890_alarm_power_off then
            for _, item in ipairs({{2, 'Keypad 1'}, {3, 'Keypad 2'},
                    {4, 'Keypad 3'}, {2, 'Keypad 4'}, {3, 'Keypad 5'}, {1, 'Menu'}}) do
                local key = assert(machine.ioport.ports[':COL.' .. item[1]].fields[item[2]])
                machine:logerror('8890_alarm_security: key=' .. item[2] .. '\n')
                key:set_value(1)
                assert(emu.wait(0.15))
                key:set_value(0)
                assert(emu.wait(0.35))
            end
            security_delay = 3
            machine.screens[':screen']:snapshot('8890_alarm_security_accepted.png')
        end
        if index < 4 then assert(emu.wait((index == 1 and 4 or 5) - security_delay)) end
    end
    if _G.noki8890_alarm_snooze then
        press(1, 'Names / C', 'snooze')
        -- Own observed deadline is 13:54 after this 13:48:15 Snooze input.
        assert(emu.wait(359))
        machine.screens[':screen']:snapshot('8890_alarm_repeated.png')
    end
    press(1, 'Menu', 'stop')
    assert(emu.wait(3))
    machine.screens[':screen']:snapshot('8890_alarm_stopped.png')
    if _G.noki8890_alarm_power_off then
        press(1, 'Names / C', 'right_softkey_probe')
        assert(emu.wait(5))
        machine.screens[':screen']:snapshot('8890_alarm_power_choice.png')
    end
end)
_G.noki8890_alarm_input = input
assert(coroutine.resume(input))

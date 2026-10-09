-- Alarm wake after the handset's own code-save transaction, not donor PMM.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_changed_code_input.lua')
local machine = manager.machine
local input = coroutine.create(function()
    assert(emu.wait(22))
    local function press(column, name, action, release)
        machine:logerror('8890_alarm_wake_physical: action=' .. action .. '\n')
        local key = assert(machine.ioport.ports[':COL.' .. column].fields[name])
        key:set_value(1)
        assert(emu.wait(0.15))
        key:set_value(0)
        assert(emu.wait(release or 0.85))
        machine.screens[':screen']:snapshot('8890_alarm_wake_' .. action .. '.png')
    end
    press(1, 'Menu', 'menu')
    for index = 2, 4 do press(1, 'Scroll Down', 'menu_' .. index) end
    press(1, 'Menu', 'settings')
    press(1, 'Menu', 'alarm')
    -- The code-save seed retains 13:48, so select the next minute.
    for index, item in ipairs({{2, 'Keypad 1'}, {4, 'Keypad 3'},
                              {2, 'Keypad 4'}, {4, 'Keypad 9'}}) do
        press(item[1], item[2], 'time_' .. index)
    end
    press(1, 'Menu', 'confirm')
    press(0, 'End', 'idle')
    local power = assert(machine.ioport.ports[':PWR'].fields['Power'])
    machine:logerror('8890_alarm_wake_physical: action=power_off\n')
    power:set_value(1)
    assert(emu.wait(4))
    power:set_value(0)
    machine:logerror('8890_alarm_wake_physical: action=power_release\n')
    assert(emu.wait(7))
    machine.screens[':screen']:snapshot('8890_alarm_wake_off.png')
    -- Re-arm only bounded debugger observations for the second native upload.
    machine.debugger:command('do temp6=0;do temp7=0;do temp8=0;do temp9=0')
    assert(emu.wait(25))
    machine.screens[':screen']:snapshot('8890_alarm_wake_security.png')
    for index, item in ipairs({{3, 'Keypad 5'}, {2, 'Keypad 4'}, {4, 'Keypad 3'},
                              {3, 'Keypad 2'}, {2, 'Keypad 1'}, {1, 'Menu'}}) do
        press(item[1], item[2], 'security_' .. index, 0.35)
    end
    assert(emu.wait(12))
    machine.screens[':screen']:snapshot('8890_alarm_wake_ringing.png')
    assert(emu.wait(2))
    press(1, 'Menu', 'stop')
    assert(emu.wait(3))
    machine.screens[':screen']:snapshot('8890_alarm_wake_stopped.png')
end)
_G.noki8890_alarm_wake_input = input
assert(coroutine.resume(input))

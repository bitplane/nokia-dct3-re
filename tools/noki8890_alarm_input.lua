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
    assert(emu.wait(27))
    for index = 1, 4 do
        machine.screens[':screen']:snapshot('8890_alarm_expiry_' .. index .. '.png')
        if index < 4 then assert(emu.wait(index == 1 and 4 or 5)) end
    end
    press(1, 'Menu', 'stop')
    assert(emu.wait(3))
    machine.screens[':screen']:snapshot('8890_alarm_stopped.png')
end)
_G.noki8890_alarm_input = input
assert(coroutine.resume(input))

-- Diagnostic physical clock entry; no firmware or provisioning writes.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8890_security_only = true
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_security_input.lua')
local machine = manager.machine
local function press(column, name, call_action)
    local key = assert(machine.ioport.ports[':COL.' .. column].fields[name])
    machine:logerror(call_action and
        ('8890_call_physical: action=' .. call_action .. '\n') or
        ('8890_clock_physical: key=' .. name .. '\n'))
    key:set_value(1)
    if not emu.wait(0.15) then key:set_value(0); return false end
    key:set_value(0)
    return emu.wait(0.85)
end
local input = coroutine.create(function()
    if not emu.wait(21) then return end
    machine.screens[':screen']:snapshot('8890_clock_before.png')
    if _G.noki8890_clock_invalid_first then
        if not press(1, 'Menu') then return end
        machine.screens[':screen']:snapshot('8890_clock_invalid.png')
        if not press(1, 'Menu') then return end
        machine.screens[':screen']:snapshot('8890_clock_invalid_dismissed.png')
    end
    for index, item in ipairs({{2, 'Keypad 1'}, {3, 'Keypad 2'},
            {3, 'Keypad 0'}, {3, 'Keypad 0'}}) do
        if not press(item[1], item[2]) then return end
        machine.screens[':screen']:snapshot('8890_clock_digit_' .. index .. '.png')
    end
    if not press(1, 'Menu') then return end
    if not emu.wait(3) then return end
    machine.screens[':screen']:snapshot('8890_clock_after.png')
    for _, item in ipairs({{3, 'Keypad 0'}, {2, 'Keypad 7'},
            {2, 'Keypad 1'}, {3, 'Keypad 0'}, {3, 'Keypad 2'},
            {3, 'Keypad 0'}, {3, 'Keypad 2'}, {4, 'Keypad 6'}}) do
        if not press(item[1], item[2]) then return end
    end
    machine.screens[':screen']:snapshot('8890_date_entered.png')
    if not press(1, 'Menu') then return end
    if not emu.wait(3) then return end
    machine.screens[':screen']:snapshot('8890_date_after.png')
    for _, item in ipairs({{2, 'Keypad 1'}, {3, 'Keypad 2'},
            {4, 'Keypad 3'}, {2, 'Keypad 4'}, {3, 'Keypad 5'},
            {4, 'Keypad 6'}, {2, 'Keypad 7'}}) do
        if not press(item[1], item[2]) then return end
    end
    machine.screens[':screen']:snapshot('8890_clock_dialed.png')
    if _G.noki8890_clock_outgoing_call then
        if not press(0, 'Call / Send', 'send') then return end
        if not emu.wait(8) then return end
        machine.screens[':screen']:snapshot('8890_clock_call_connected.png')
        if not press(0, 'End', 'end') then return end
        if not emu.wait(5) then return end
        machine.screens[':screen']:snapshot('8890_clock_call_released.png')
    end
end)
_G.noki8890_clock_input = input
assert(coroutine.resume(input))

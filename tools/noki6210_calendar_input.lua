-- Physical NPE-3 Calendar entry; no firmware writes or internal UI events.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_staged_observe.lua')
local machine = manager.machine
local input = coroutine.create(function()
    if not emu.wait(20) then return end
    local function press(name, action, column)
        local key = assert(machine.ioport.ports[':COL.' .. (column or 1)].fields[name])
        machine:logerror('6210_calendar_probe: action=' .. action .. '\n')
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return false end
        key:set_value(0)
        if not emu.wait(0.85) then return false end
        machine.screens[':screen']:snapshot('6210_calendar_' .. action .. '.png')
        return true
    end
    if not press('Left Softkey / Menu', 'menu') then return end
    for index = 2, 8 do
        if not press('Scroll Down', 'menu_' .. index) then return end
    end
    if not press('Left Softkey / Menu', 'selected') then return end
    for index, step in ipairs({{2, 'Keypad 1'}, {4, 'Keypad 3'},
                              {2, 'Keypad 4'}, {2, 'Keypad 7'}}) do
        if not press(step[2], 'time_' .. index, step[1]) then return end
    end
    if not press('Left Softkey / Menu', 'time_confirm') then return end
    if not emu.wait(3) then return end
    machine.screens[':screen']:snapshot('6210_calendar_after_time.png')
    for index, step in ipairs({{3, 'Keypad 0'}, {2, 'Keypad 7'},
        {2, 'Keypad 1'}, {3, 'Keypad 0'}, {3, 'Keypad 2'},
        {3, 'Keypad 0'}, {3, 'Keypad 2'}, {4, 'Keypad 6'}}) do
        if not press(step[2], 'date_' .. index, step[1]) then return end
    end
    if not press('Left Softkey / Menu', 'date_confirm') then return end
    if not emu.wait(3) then return end
    machine.screens[':screen']:snapshot('6210_calendar_after_date.png')
end)
_G.noki6210_calendar_probe = input
assert(coroutine.resume(input))

-- Own NHM-3 Calendar workflow; only physical keypad fields are driven.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6250_runtime_observe.lua')
local machine = manager.machine
local cold = os.getenv('NOKIA_DCT3_6250_CALENDAR_COLD') == '1'
local function cell(column, row)
    for _, field in pairs(machine.ioport.ports[':COL.' .. column].fields) do
        if field.mask == (1 << row) then return field end
    end
    error('missing NHM-3 physical key cell')
end
local function digit(value)
    for _, port in pairs(machine.ioport.ports) do
        if port.fields['Keypad ' .. value] then return port.fields['Keypad ' .. value] end
    end
    error('missing NHM-3 digit ' .. value)
end
local input = coroutine.create(function()
    if not emu.wait(16) then return end
    local function press(key, action)
        machine:logerror('6250_calendar_physical: action=' .. action .. '\n')
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return false end
        key:set_value(0)
        return emu.wait(1.85)
    end
    if not press(cell(1, 1), 'menu') then return end
    for index = 1, 7 do
        if not press(cell(1, 3), 'down_' .. index) then return end
    end
    if not press(cell(1, 1), 'calendar') then return end
    if cold then
        machine.screens[':screen']:snapshot('6250_calendar_cold.png')
        machine:logerror('6250_calendar_physical: event=cold_presented\n')
        return
    end
    for index, value in ipairs({'1', '3', '4', '7'}) do
        if not press(digit(value), 'time_' .. index) then return end
    end
    if not press(cell(1, 1), 'time_confirm') then return end
    for index, value in ipairs({'0', '7', '1', '0', '2', '0', '2', '6'}) do
        if not press(digit(value), 'date_' .. index) then return end
    end
    if not press(cell(1, 1), 'date_confirm') then return end
    machine.screens[':screen']:snapshot('6250_calendar_entered.png')
    machine:logerror('6250_calendar_physical: event=entered_presented\n')
end)
_G.noki6250_calendar_input = input
assert(coroutine.resume(input))

-- Own NHM-3 Clock 10-2 alarm route; no RTC or firmware writes.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6250_runtime_observe.lua')
local machine = manager.machine
local function cell(column, row)
    for _, field in pairs(machine.ioport.ports[':COL.' .. column].fields) do
        if field.mask == (1 << row) then return field end
    end
    error('missing NHM-3 physical cell')
end
local function named(name)
    for _, port in pairs(machine.ioport.ports) do
        if port.fields[name] then return port.fields[name] end
    end
    error('missing NHM-3 key ' .. name)
end
local input = coroutine.create(function()
    if not emu.wait(16) then return end
    local function press(key, action)
        machine:logerror('6250_alarm_physical: action=' .. action .. '\n')
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return false end
        key:set_value(0)
        return emu.wait(0.85)
    end
    if not press(cell(1, 1), 'menu') then return end
    for index = 1, 9 do
        if not press(cell(1, 3), 'down_' .. index) then return end
    end
    if not press(cell(1, 1), 'clock') then return end
    if not press(cell(1, 3), 'clock_down_1') then return end
    if not press(cell(1, 1), 'alarm') then return end
    for index, value in ipairs({'1', '3', '4', '8'}) do
        if not press(named('Keypad ' .. value), 'time_' .. index) then return end
    end
    if not press(cell(1, 1), 'confirm') then return end
    machine.screens[':screen']:snapshot('6250_alarm_confirm.png')
    if not press(named('End'), 'idle') then return end
    machine.screens[':screen']:snapshot('6250_alarm_idle.png')
    if not emu.wait(30) then return end
    -- This phase shows the firmware's Stop/Snooze controls during title blink.
    machine.screens[':screen']:snapshot('6250_alarm_elapsed.png')
    if not press(cell(1, 1), 'stop') then return end
    if not emu.wait(3) then return end
    machine.screens[':screen']:snapshot('6250_alarm_stopped.png')
    machine:logerror('6250_alarm_physical: event=stopped_presented\n')
end)
_G.noki6250_alarm_input = input
assert(coroutine.resume(input))

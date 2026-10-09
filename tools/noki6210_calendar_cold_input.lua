-- Read Calendar after a cold process; never enter or replace time/date.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_staged_observe.lua')
local machine = manager.machine
local input = coroutine.create(function()
    if not emu.wait(20) then return end
    local function press(name, action)
        local key = assert(machine.ioport.ports[':COL.1'].fields[name])
        machine:logerror('6210_calendar_cold: action=' .. action .. '\n')
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return false end
        key:set_value(0)
        if not emu.wait(0.85) then return false end
        machine.screens[':screen']:snapshot('6210_calendar_cold_' .. action .. '.png')
        return true
    end
    machine.screens[':screen']:snapshot('6210_calendar_cold_idle.png')
    if not press('Left Softkey / Menu', 'menu') then return end
    for index = 2, 8 do
        if not press('Scroll Down', 'menu_' .. index) then return end
    end
    if not press('Left Softkey / Menu', 'selected') then return end
    if not emu.wait(3) then return end
    machine.screens[':screen']:snapshot('6210_calendar_cold_result.png')
end)
_G.noki6210_calendar_cold_input = input
assert(coroutine.resume(input))

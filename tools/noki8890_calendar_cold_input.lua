-- Security-only cold boot, then physical Calendar selection; no date entry.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_clock_nv_read.lua')
local machine = manager.machine
local input = coroutine.create(function()
    if not emu.wait(22) then return end
    local menu = assert(machine.ioport.ports[':COL.1'].fields['Menu'])
    local down = assert(machine.ioport.ports[':COL.1'].fields['Scroll Down'])
    for index = 1, 9 do
        local key = (index == 1 or index == 9) and menu or down
        machine:logerror('8890_calendar_physical: step=' .. index .. '\n')
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return end
        key:set_value(0)
        if not emu.wait(0.85) then return end
    end
    machine.screens[':screen']:snapshot('8890_calendar_cold.png')
end)
_G.noki8890_calendar_cold_input = input
assert(coroutine.resume(input))

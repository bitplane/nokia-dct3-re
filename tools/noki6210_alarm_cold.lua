-- Retained alarm cold boot: no clock/alarm entry and no firmware writes.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_staged_observe.lua')
local machine = manager.machine
local input = coroutine.create(function()
    if not emu.wait(20) then return end
    machine:logerror('6210_alarm_probe: cold_observe=1\n')
    machine.screens[':screen']:snapshot('6210_alarm_cold_idle.png')
    -- Match the existing set/expiry fixture's 77-second capture point.
    if not emu.wait(57) then return end
    machine.screens[':screen']:snapshot('6210_alarm_elapsed.png')
    local key = assert(machine.ioport.ports[':COL.1'].fields['Left Softkey / Menu'])
    machine:logerror('6210_alarm_probe: action=stop\n')
    key:set_value(1)
    if not emu.wait(0.15) then key:set_value(0); return end
    key:set_value(0)
    if not emu.wait(4) then return end
    machine.screens[':screen']:snapshot('6210_alarm_stopped.png')
end)
_G.noki6210_alarm_probe = input
assert(coroutine.resume(input))

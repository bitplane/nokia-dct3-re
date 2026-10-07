-- Capture host rejection, then recover through physical End and Menu.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki8850_outgoing_sms_input.lua')
local machine = manager.machine
local input = coroutine.create(function()
    if not emu.wait(_G.noki8850_sms_failure_observe_at or 38) then return end
    for index = 1, (_G.noki8850_sms_failure_observe_count or 12) do
        machine.screens[':screen']:snapshot('8850_sms_reject_' .. index .. '.png')
        if not emu.wait(_G.noki8850_sms_failure_observe_interval or 0.5) then return end
    end
    if not emu.wait(3) then return end
    for _, name in ipairs({'End', 'Menu'}) do
        local column = name == 'End' and 0 or 1
        local key = assert(machine.ioport.ports[':COL.' .. column].fields[name])
        machine:logerror('8850_sms_recovery_physical: key=' .. name .. '\n')
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return end
        key:set_value(0)
        if not emu.wait(0.85) then return end
    end
    machine.screens[':screen']:snapshot('8850_sms_recovery_menu.png')
end)
_G.noki8850_sms_reject_input = input
assert(coroutine.resume(input))

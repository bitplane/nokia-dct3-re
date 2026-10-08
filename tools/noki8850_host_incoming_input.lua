-- Host-originated call after physical startup; no firmware or network injection.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8850_call_idle_only = true
dofile(assert(source:match('^(.*[/])')) .. 'noki8850_outgoing_call_input.lua')
local machine = manager.machine
local input = coroutine.create(function()
    if not emu.wait(38) then return end
    machine.screens[':screen']:snapshot('8850_incoming_ringing.png')
    if _G.noki8850_incoming_alerting_hold then return end
    for index, name in ipairs({'Call / Send', 'End'}) do
        local key = assert(machine.ioport.ports[':COL.0'].fields[name])
        machine:logerror('8850_incoming_physical: action=' .. name .. '\n')
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return end
        key:set_value(0)
        if not emu.wait(7.85) then return end
        machine.screens[':screen']:snapshot(index == 1 and
            '8850_incoming_connected.png' or '8850_after_incoming_call.png')
    end
end)
_G.noki8850_host_incoming_input = input
assert(coroutine.resume(input))

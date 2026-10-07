-- Settle the clock physically before a host-originated incoming call.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8890_clock_settlement_only = true
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_clock_input.lua')
local machine = manager.machine
local input = coroutine.create(function()
    if not emu.wait(48) then return end
    machine.screens[':screen']:snapshot('8890_incoming_ringing.png')
    for index, name in ipairs({'Call / Send', 'End'}) do
        local key = assert(machine.ioport.ports[':COL.0'].fields[name])
        machine:logerror('8890_incoming_physical: action=' .. name .. '\n')
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return end
        key:set_value(0)
        if not emu.wait(7.85) then return end
        machine.screens[':screen']:snapshot(index == 1 and
            '8890_incoming_connected.png' or '8890_after_incoming_call.png')
    end
end)
_G.noki8890_clock_incoming_input = input
assert(coroutine.resume(input))

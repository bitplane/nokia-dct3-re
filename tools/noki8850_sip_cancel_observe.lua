-- Physical startup and unanswered external call; no Answer or firmware writes.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
_G.noki8850_call_idle_only = true
dofile(directory .. 'noki8850_outgoing_call_input.lua')
local machine = manager.machine
local observation = coroutine.create(function()
    if not emu.wait(32) then return end
    machine.screens[':screen']:snapshot('8850_sip_registered_idle.png')
    machine:logerror('8850_sip_cancel: ready t=32\n')
    if not emu.wait(20) then return end
    machine.screens[':screen']:snapshot('8850_sip_missed_call.png')
    local exit = assert(machine.ioport.ports[':COL.1'].fields['Names / C'])
    machine:logerror('8850_sip_cancel: physical Exit\n')
    exit:set_value(1)
    if not emu.wait(0.15) then exit:set_value(0); return end
    exit:set_value(0)
    if not emu.wait(1.85) then return end
    machine.screens[':screen']:snapshot('8850_sip_after_cancel.png')
end)
_G.noki8850_sip_cancel_observe = observation
assert(coroutine.resume(observation))

-- Observe an unanswered external call; no keys or firmware writes.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_staged_observe.lua')
local machine = manager.machine
local observation = coroutine.create(function()
    if not emu.wait(32) then return end
    machine.screens[':screen']:snapshot('6210_sip_registered_idle.png')
    machine:logerror('6210_sip_cancel: ready t=32\n')
    if not emu.wait(23) then return end
    machine.screens[':screen']:snapshot('6210_sip_missed_call.png')
    local exit = assert(machine.ioport.ports[':COL.1'].fields['Right Softkey / C'])
    machine:logerror('6210_sip_cancel: physical Exit\n')
    exit:set_value(1)
    if not emu.wait(0.15) then exit:set_value(0); return end
    exit:set_value(0)
    if not emu.wait(1.85) then return end
    machine.screens[':screen']:snapshot('6210_sip_after_cancel.png')
    machine:logerror('6210_sip_cancel: final t=57\n')
end)
_G.noki6210_sip_cancel_observe = observation
assert(coroutine.resume(observation))

-- Observe an unanswered external call and dismiss its notification physically.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
if not _G.noki6210_sip_restore_idle then
    dofile(directory .. 'noki6210_staged_observe.lua')
end
local machine = manager.machine
local observation = coroutine.create(function()
    if not emu.wait(32 - machine.time:as_double()) then return end
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
if _G.noki6210_sip_restore_idle then
    _G.noki6210_sip_cancel_post_load = emu.add_machine_post_load_notifier(function()
        assert(coroutine.resume(observation))
    end)
else
    assert(coroutine.resume(observation))
end

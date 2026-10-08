-- Physical security entry and missed-call dismissal; never Answer.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
if not _G.noki8210_sip_restore_idle then
    _G.noki8210_security_only = true
    dofile(directory .. 'noki8210_security_input.lua')
end
local machine = manager.machine
local observation = coroutine.create(function()
    local ready = _G.noki8210_sip_restore_idle and 35 or 32
    assert(emu.wait(ready - machine.time:as_double()))
    machine.screens[':screen']:snapshot('8210_sip_registered_idle.png')
    machine:logerror(string.format('8210_sip_cancel: ready t=%d\n', ready))
    assert(emu.wait(20))
    machine.screens[':screen']:snapshot('8210_sip_missed_call.png')
    local exit = assert(machine.ioport.ports[':COL.1'].fields['Names / C'])
    machine:logerror('8210_sip_cancel: physical Exit\n')
    exit:set_value(1)
    assert(emu.wait(0.15))
    exit:set_value(0)
    assert(emu.wait(1.85))
    machine.screens[':screen']:snapshot('8210_sip_after_cancel.png')
end)
_G.noki8210_sip_cancel_observe = observation
if _G.noki8210_sip_restore_idle then
    _G.noki8210_sip_cancel_post_load = emu.add_machine_post_load_notifier(function()
        assert(coroutine.resume(observation))
    end)
else
    assert(coroutine.resume(observation))
end

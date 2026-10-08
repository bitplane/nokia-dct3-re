-- Physical security entry and missed-call dismissal; never Answer.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8210_security_only = true
dofile(assert(source:match('^(.*[/])')) .. 'noki8210_security_input.lua')
local machine = manager.machine
local observation = coroutine.create(function()
    assert(emu.wait(32))
    machine.screens[':screen']:snapshot('8210_sip_registered_idle.png')
    machine:logerror('8210_sip_cancel: ready t=32\n')
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
assert(coroutine.resume(observation))

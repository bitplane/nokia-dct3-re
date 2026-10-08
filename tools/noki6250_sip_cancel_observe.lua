-- Passive own-ROM observer plus physical missed-call Exit, never Answer.
local source = debug.getinfo(1, 'S').source:sub(2)
if not _G.noki6250_runtime_observer_loaded then
    dofile(assert(source:match('^(.*[/])')) .. 'noki6250_runtime_observe.lua')
    _G.noki6250_runtime_observer_loaded = true
end
local machine = manager.machine
local input = coroutine.create(function()
    assert(emu.wait(32 - machine.time:as_double()))
    machine.screens[':screen']:snapshot('6250_sip_registered_idle.png')
    machine:logerror('6250_sip_cancel: ready\n')
    assert(emu.wait(20))
    machine.screens[':screen']:snapshot('6250_sip_missed_call.png')
    local exit
    for _, field in pairs(machine.ioport.ports[':COL.1'].fields) do
        if field.mask == 0x10 then exit = field end
    end
    assert(exit, 'missing own Names/C physical matrix cell')
    machine:logerror('6250_sip_cancel: physical Exit\n')
    exit:set_value(1)
    assert(emu.wait(0.15))
    exit:set_value(0)
    assert(emu.wait(1.85))
    machine.screens[':screen']:snapshot('6250_sip_after_cancel.png')
end)
_G.noki6250_sip_cancel_observe = input
assert(coroutine.resume(input))

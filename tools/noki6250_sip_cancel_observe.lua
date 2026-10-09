-- Passive own-ROM observer plus physical missed-call Exit, never Answer.
local source = debug.getinfo(1, 'S').source:sub(2)
if not _G.noki6250_runtime_observer_loaded then
    dofile(assert(source:match('^(.*[/])')) .. 'noki6250_runtime_observe.lua')
    _G.noki6250_runtime_observer_loaded = true
end
local machine = manager.machine
local input = coroutine.create(function()
    if not emu.wait(32 - machine.time:as_double()) then return end
    machine.screens[':screen']:snapshot('6250_sip_registered_idle.png')
    machine:logerror('6250_sip_cancel: ready\n')
    -- Loading a state cancels this schedule; the restore fixture resumes cleanup.
    if not emu.wait(20) then return end
    machine.screens[':screen']:snapshot('6250_sip_missed_call.png')
    local exit
    for _, field in pairs(machine.ioport.ports[':COL.1'].fields) do
        if field.mask == 0x10 then exit = field end
    end
    assert(exit, 'missing own Names/C physical matrix cell')
    machine:logerror('6250_sip_cancel: physical Exit\n')
    exit:set_value(1)
    if not emu.wait(0.15) then exit:set_value(0); return end
    exit:set_value(0)
    if not emu.wait(1.85) then return end
    machine.screens[':screen']:snapshot('6250_sip_after_cancel.png')
end)
_G.noki6250_sip_cancel_observe = input
assert(coroutine.resume(input))

-- Own physical clock/date setup; observe an unanswered call and dismiss it.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
if not _G.noki8890_sip_restore_idle then
    _G.noki8890_clock_settlement_only = true
    dofile(directory .. 'noki8890_clock_input.lua')
end
local machine = manager.machine
local observation = coroutine.create(function()
    if not emu.wait(45 - machine.time:as_double()) then return end
    machine.screens[':screen']:snapshot('8890_sip_registered_idle.png')
    machine:logerror('8890_sip_cancel: ready t=45\n')
    if not emu.wait(20) then return end
    machine.screens[':screen']:snapshot('8890_sip_missed_call.png')
    local exit = assert(machine.ioport.ports[':COL.1'].fields['Names / C'])
    machine:logerror('8890_sip_cancel: physical Exit\n')
    exit:set_value(1)
    if not emu.wait(0.15) then exit:set_value(0); return end
    exit:set_value(0)
    if not emu.wait(1.85) then return end
    machine.screens[':screen']:snapshot('8890_sip_after_cancel.png')
end)
_G.noki8890_sip_cancel_observe = observation
if _G.noki8890_sip_restore_idle then
    _G.noki8890_sip_cancel_post_load = emu.add_machine_post_load_notifier(function()
        assert(coroutine.resume(observation))
    end)
else
    assert(coroutine.resume(observation))
end

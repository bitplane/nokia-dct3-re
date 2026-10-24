-- Reuse the qualified physical *123# fixture; the host owns only its response.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/])') .. 'noki8850_ussd_input.lua')
local machine = manager.machine
local runner = coroutine.create(function()
    assert(emu.wait(45))
    local key = assert(machine.ioport.ports[':COL.1'].fields['Menu'])
    machine:logerror('8850_host_ussd_recovery: Menu press\n')
    key:set_value(1)
    assert(emu.wait(0.2))
    key:set_value(0)
    machine:logerror('8850_host_ussd_recovery: Menu release\n')
    assert(emu.wait(0.8))
    machine.screens[':screen']:snapshot('8850_host_ussd_menu.png')
end)
_G.nsm2_host_ussd_request = runner
assert(coroutine.resume(runner))

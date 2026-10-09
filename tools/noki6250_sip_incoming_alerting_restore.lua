-- Exact ringing-state restore; external INVITE is rejected, not replayed.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
dofile(directory .. 'noki6250_sip_cancel_observe.lua')
local machine = manager.machine
local snapshot = dofile(directory .. 'dct3_state_snapshot.lua')(
    machine, assert(machine.devices[':maincpu']), '6250_state')
local saved, completed
local pre_save = emu.add_machine_pre_save_notifier(function()
    saved = snapshot('saved')
    machine:logerror('sip_state: saved\n')
end)
local post_load = emu.add_machine_post_load_notifier(function()
    local restored = snapshot('restored')
    assert(saved, 'missing saved architectural observation')
    for index = 1, 4 do assert(saved[index] == restored[index], 'architectural state mismatch') end
    machine:logerror('sip_state: restored\n')
    machine:logerror('state_roundtrip: result=pass scenario=6250_incoming_alerting\n')
    local cleanup = coroutine.create(function()
        assert(emu.wait(52 - machine.time:as_double()))
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
        completed = true
    end)
    _G.noki6250_sip_alerting_cleanup = cleanup
    assert(coroutine.resume(cleanup))
end)
local runner = coroutine.create(function()
    assert(emu.wait(40))
    machine:save('6250_sip_alerting')
    assert(emu.wait(0.5))
    assert(saved, 'save did not execute')
    machine:load('6250_sip_alerting')
end)
_G.noki6250_sip_alerting_restore = {runner, pre_save, post_load,
    emu.add_machine_stop_notifier(function()
        if not completed then machine:logerror('6250_state: FAIL incomplete\n') end
    end)}
assert(coroutine.resume(runner))

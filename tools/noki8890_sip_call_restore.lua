-- Exact own-product call restoration; external SIP state is cleared, not replayed.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
local outgoing = _G.noki8890_sip_restore_outgoing == true
dofile(directory .. (outgoing and 'noki8890_outgoing_call_input.lua' or 'noki8890_sip_cancel_observe.lua'))
local machine = manager.machine
local cpu = assert(machine.devices[':maincpu'])
local saved, completed
local snapshot = dofile(directory .. 'dct3_state_snapshot.lua')(machine, cpu, '8890_state')
local pre_save = emu.add_machine_pre_save_notifier(function()
    saved = snapshot('saved')
    machine:logerror('sip_state: saved\n')
end)
local post_load = emu.add_machine_post_load_notifier(function()
    local restored = snapshot('restored')
    assert(saved, 'missing saved architectural observation')
    for index = 1, 4 do assert(saved[index] == restored[index], 'architectural state mismatch') end
    machine:logerror('sip_state: restored\n')
    machine:logerror('state_roundtrip: result=pass scenario=8890_' ..
        (outgoing and 'outgoing_pending' or 'incoming_alerting') .. '\n')
    -- Loading cancels prior Lua waits; resume physical dismissal explicitly.
    local cleanup = coroutine.create(function()
        if outgoing then
            assert(emu.wait(45 - machine.time:as_double()))
            machine.screens[':screen']:snapshot('8890_after_outgoing_call.png')
            completed = true
            return
        end
        assert(emu.wait(65 - machine.time:as_double()))
        machine.screens[':screen']:snapshot('8890_sip_missed_call.png')
        local exit = assert(machine.ioport.ports[':COL.1'].fields['Names / C'])
        machine:logerror('8890_sip_cancel: physical Exit\n')
        exit:set_value(1)
        assert(emu.wait(0.15))
        exit:set_value(0)
        assert(emu.wait(1.85))
        machine.screens[':screen']:snapshot('8890_sip_after_cancel.png')
        completed = true
    end)
    _G.noki8890_sip_alerting_cleanup = cleanup
    assert(coroutine.resume(cleanup))
end)
local runner = coroutine.create(function()
    assert(emu.wait(outgoing and 32 or 52))
    machine:save('8890_sip_alerting')
    assert(emu.wait(0.5))
    assert(saved, 'save did not execute')
    machine:load('8890_sip_alerting')
end)
_G.noki8890_sip_alerting_restore = {runner, pre_save, post_load,
    emu.add_machine_stop_notifier(function()
        if not completed then machine:logerror('8890_state: FAIL incomplete\n') end
    end)}
assert(coroutine.resume(runner))

-- Save while the external decision is pending; replay only physical input.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = source:match('^(.*[/])')
dofile(directory .. 'noki8850_host_ussd_request.lua')
local machine = manager.machine
local capture = dofile(directory .. 'arm_architecture_snapshot.lua')
local saved, complete
local retained = {}
local function snapshot(event)
    local state = capture(machine)
    machine:logerror(string.format('8850_ussd_state: event=%s pc=%08x sp=%08x ram=%08x cpu=%s t=%.9f\n',
        event, state.pc, state.sp, state.ram, state.cpu, state.time))
    return state
end
retained[#retained + 1] = emu.add_machine_pre_save_notifier(function()
    saved = snapshot('saved')
end)
retained[#retained + 1] = emu.add_machine_post_load_notifier(function()
    local restored = snapshot('restored')
    assert(saved, 'missing pending USSD save')
    for _, name in ipairs({'time', 'pc', 'sp', 'ram', 'cpu'}) do
        assert(saved[name] == restored[name], 'USSD restoration mismatch: ' .. name)
    end
    machine:logerror(string.format('state_roundtrip: result=pass scenario=nsm2_ussd requested_at=%.9f t=%.9f\n', saved.time, restored.time))
    local continuation = coroutine.create(function()
        assert(emu.wait(39 - machine.time:as_double()))
        machine.screens[':screen']:snapshot('8850_ussd_result.png')
        for _, action in ipairs({{39, 1, 'Names / C', 'Back', '8850_ussd_after_back.png'},
                                {45, 1, 'Menu', 'Menu', '8850_host_ussd_menu.png'}}) do
            local delay = action[1] - machine.time:as_double()
            if delay > 0 then assert(emu.wait(delay)) end
            local key = assert(machine.ioport.ports[':COL.' .. action[2]].fields[action[3]])
            if action[4] == 'Back' then
                machine:logerror('8850_ussd_physical: key=Back\n')
            else
                machine:logerror('8850_host_ussd_recovery: Menu press\n')
            end
            key:set_value(1)
            assert(emu.wait(action[4] == 'Back' and 0.15 or 0.2))
            key:set_value(0)
            if action[4] == 'Menu' then machine:logerror('8850_host_ussd_recovery: Menu release\n') end
            assert(emu.wait(action[4] == 'Back' and 1.85 or 0.8))
            machine.screens[':screen']:snapshot(action[5])
        end
        complete = true
    end)
    retained[#retained + 1] = continuation
    assert(coroutine.resume(continuation))
end)
local runner = coroutine.create(function()
    assert(emu.wait(31.8))
    machine:save('8850_pending_ussd')
    assert(emu.wait(1))
    assert(saved, 'pending USSD save did not execute')
    machine:load('8850_pending_ussd')
end)
retained[#retained + 1] = runner
retained[#retained + 1] = emu.add_machine_stop_notifier(function()
    if not complete then machine:logerror('8850_ussd_state: FAIL incomplete\n') end
end)
_G.nsm2_host_ussd_restore = retained
assert(coroutine.resume(runner))

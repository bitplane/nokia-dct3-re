-- Restore the handset-owned pending request, then replay physical input only.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
dofile(directory .. 'noki8850_host_ussd_network_request.lua')
local machine = manager.machine
local capture = dofile(directory .. 'arm_architecture_snapshot.lua')
local saved, complete
local retained = {}
local function snapshot(event)
    local state = capture(machine)
    machine:logerror(string.format('8850_network_ussd_state: event=%s pc=%08x sp=%08x ram=%08x cpu=%s t=%.9f\n',
        event, state.pc, state.sp, state.ram, state.cpu, state.time))
    return state
end
retained[#retained + 1] = emu.add_machine_pre_save_notifier(function()
    saved = snapshot('saved')
end)
retained[#retained + 1] = emu.add_machine_post_load_notifier(function()
    local restored = snapshot('restored')
    assert(saved, 'missing pending network request save')
    for _, name in ipairs({'time', 'pc', 'sp', 'ram', 'cpu'}) do
        assert(saved[name] == restored[name], 'network request restore mismatch: ' .. name)
    end
    machine:logerror(string.format('state_roundtrip: result=pass scenario=nsm2_network_ussd requested_at=%.9f t=%.9f\n', saved.time, restored.time))
    local continuation = coroutine.create(function()
        _G.nsm2_host_ussd_replay()
        complete = true
    end)
    retained[#retained + 1] = continuation
    assert(coroutine.resume(continuation))
end)
local runner = coroutine.create(function()
    assert(emu.wait(33))
    machine:save('8850_network_ussd')
    assert(emu.wait(0.5))
    assert(saved, 'pending network request save did not execute')
    machine:load('8850_network_ussd')
end)
retained[#retained + 1] = runner
retained[#retained + 1] = emu.add_machine_stop_notifier(function()
    if not complete then machine:logerror('8850_network_ussd_state: FAIL incomplete\n') end
end)
_G.nsm2_network_ussd_restore = retained
assert(coroutine.resume(runner))

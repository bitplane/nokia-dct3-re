-- Restore a sounding own-product alarm before physical Stop; no state pokes.
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
local machine = manager.machine
local capture = dofile(directory .. 'arm_architecture_snapshot.lua')
local saved, reference
local function observe(event)
    local state = capture(machine)
    machine:logerror(string.format(
        '6250_alarm_awake_state: event=%s pc=%08x sp=%08x ram=%08x cpu=%s t=%.9f\n',
        event, state.pc, state.sp, state.ram, state.cpu, state.time))
    return state
end
local function equal(first, second)
    assert(first, 'missing reference architecture')
    for key, value in pairs(first) do
        assert(value == second[key], 'awake alarm architecture mismatch: ' .. key)
    end
end
_G.noki6250_alarm_awake_notifiers = {
    emu.add_machine_pre_save_notifier(function() saved = observe('saved') end),
    emu.add_machine_post_load_notifier(function()
        equal(saved, observe('restored'))
        local replay = coroutine.create(function()
            assert(emu.wait(1.25))
            equal(reference, observe('replayed'))
            machine.screens[':screen']:snapshot('6250_alarm_awake_replayed.png')
            machine:logerror('6250_alarm_awake_restore: PASS\n')
            assert(coroutine.resume(_G.noki6250_alarm_input))
        end)
        _G.noki6250_alarm_awake_replay = replay
        assert(coroutine.resume(replay))
    end),
}
_G.noki6250_alarm_awake_checkpoint = function()
    machine:save('6250_alarm_awake')
    assert(emu.wait(1.25))
    assert(saved, 'awake alarm save did not execute')
    reference = observe('reference')
    machine.screens[':screen']:snapshot('6250_alarm_awake_reference.png')
    machine:load('6250_alarm_awake')
    coroutine.yield()
    return true
end
dofile(directory .. 'noki6250_alarm_input.lua')

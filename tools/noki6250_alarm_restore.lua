-- Restore an own NHM-3 powered-off alarm; no firmware/controller writes.
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
local machine = manager.machine
local capture = dofile(directory .. 'arm_architecture_snapshot.lua')
local saved
local function observe(event)
    local state = capture(machine)
    machine:logerror(string.format(
        '6250_alarm_state: event=%s pc=%08x sp=%08x ram=%08x cpu=%s t=%.9f\n',
        event, state.pc, state.sp, state.ram, state.cpu, state.time))
    return state
end
_G.noki6250_alarm_restore_notifiers = {
    emu.add_machine_pre_save_notifier(function()
        saved = observe('saved')
        machine:logerror(string.format('6250_alarm_replay: phase=reference event=begin t=%.9f\n', saved.time))
    end),
    emu.add_machine_post_load_notifier(function()
        local restored = observe('restored')
        assert(saved, 'missing alarm save observation')
        for key, value in pairs(saved) do
            assert(value == restored[key], 'alarm architecture mismatch: ' .. key)
        end
        machine:logerror(string.format('6250_alarm_replay: phase=restored event=begin t=%.9f\n', restored.time))
        local replay = coroutine.create(function()
            assert(emu.wait(1.25))
            machine:logerror(string.format('6250_alarm_replay: phase=restored event=end t=%.9f\n', machine.time:as_double()))
            machine.screens[':screen']:snapshot('6250_alarm_off_restored.png')
            assert(coroutine.resume(_G.noki6250_alarm_input))
        end)
        _G.noki6250_alarm_restore_replay = replay
        assert(coroutine.resume(replay))
    end),
}
_G.noki6250_alarm_restore_checkpoint = function()
    machine:save('6250_alarm_off')
    -- Keep replay edges away from RTC/coroutine same-timestamp ordering.
    assert(emu.wait(1.25))
    assert(saved, 'alarm save did not execute')
    machine:logerror(string.format('6250_alarm_replay: phase=reference event=end t=%.9f\n', machine.time:as_double()))
    machine.screens[':screen']:snapshot('6250_alarm_off_reference.png')
    machine:load('6250_alarm_off')
    coroutine.yield()
    return 1.25
end
dofile(directory .. 'noki6250_alarm_input.lua')

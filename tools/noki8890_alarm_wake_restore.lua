-- Save/load the powered-off countdown; all inputs remain physical.
local source = debug.getinfo(1, 'S').source:sub(2)
local machine = manager.machine
local saved
local capture = dofile(assert(source:match('^(.*[/])')) .. 'arm_architecture_snapshot.lua')
local function architecture(event)
    local observed = capture(machine)
    local state = {observed.time, observed.pc, observed.sp, observed.ram, observed.cpu}
    machine:logerror(string.format(
        '8890_alarm_state: event=%s pc=%08x sp=%08x ram=%08x cpu=%s t=%.9f\n',
        event, state[2], state[3], state[4], state[5], state[1]))
    return state
end
_G.noki8890_alarm_restore_notifiers = {
    emu.add_machine_pre_save_notifier(function()
        saved = architecture('saved')
        machine:logerror(string.format(
            '8890_alarm_replay: phase=reference event=begin t=%.9f\n', saved[1]))
    end),
    emu.add_machine_post_load_notifier(function()
        local restored = architecture('restored')
        assert(saved, 'alarm save observation absent')
        for index = 1, #saved do assert(saved[index] == restored[index], 'alarm architecture mismatch') end
        machine:logerror(string.format(
            '8890_alarm_replay: phase=restored event=begin t=%.9f\n', restored[1]))
        local replay = coroutine.create(function()
            assert(emu.wait(1.25))
            machine:logerror(string.format(
                '8890_alarm_replay: phase=restored event=end t=%.9f\n', machine.time:as_double()))
            machine.screens[':screen']:snapshot('8890_alarm_wake_off_restored.png')
            assert(coroutine.resume(_G.noki8890_alarm_wake_input))
        end)
        _G.noki8890_alarm_restored = replay
        assert(coroutine.resume(replay))
    end),
}
_G.noki8890_alarm_restore_checkpoint = function()
    machine:save('8890_alarm_off')
    -- Avoid same-timestamp RTC/coroutine ordering at a replay-window edge.
    assert(emu.wait(1.25))
    assert(saved, 'alarm save did not execute')
    machine:logerror(string.format(
        '8890_alarm_replay: phase=reference event=end t=%.9f\n', machine.time:as_double()))
    machine.screens[':screen']:snapshot('8890_alarm_wake_off_reference.png')
    machine:load('8890_alarm_off')
    coroutine.yield()
    return 1.25
end
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_alarm_wake_input.lua')

-- Physical clock/date entry, then exact save/load across natural midnight.
local directory = assert(debug.getinfo(1, 'S').source:match('^@(.*/)'))
dofile(directory .. 'noki8890_calendar_rollover_input.lua')
local machine = manager.machine
local capture = dofile(directory .. 'arm_architecture_snapshot.lua')
local saved, reference, complete
local function observe(event)
    local state = capture(machine)
    local date = machine.devices[':maincpu'].spaces['program']:read_u32(0x137420)
    machine:logerror(string.format(
        '8890_calendar_restore: event=%s t=%.9f pc=%08x sp=%08x ram=%08x cpu=%s date=%08x\n',
        event, state.time, state.pc, state.sp, state.ram, state.cpu, date))
    state.date = date
    return state
end
local function same(left, right)
    for _, field in ipairs({'time', 'pc', 'sp', 'ram', 'cpu', 'date'}) do
        assert(left[field] == right[field], 'calendar restoration differs: ' .. field)
    end
end
local pre = emu.add_machine_pre_save_notifier(function()
    saved = observe('saved')
    assert(saved.date == 0xd44b1580, 'checkpoint must precede midnight')
end)
local post = emu.add_machine_post_load_notifier(function()
    same(saved, observe('restored'))
    local replay = coroutine.create(function()
        assert(emu.wait(20))
        same(reference, observe('replayed'))
        assert(reference.date == 0xd44c6700, 'natural midnight date absent')
        machine.screens[':screen']:snapshot('8890_calendar_restore_replayed.png')
        complete = true
        machine:logerror('8890_calendar_restore: result=pass elapsed=20 native_speech=0\n')
    end)
    _G.noki8890_calendar_restore_replay = replay
    assert(coroutine.resume(replay))
end)
local runner = coroutine.create(function()
    assert(emu.wait(80))
    machine:save('calendar_midnight')
    assert(emu.wait(20))
    assert(saved, 'save did not execute')
    reference = observe('reference')
    machine.screens[':screen']:snapshot('8890_calendar_restore_reference.png')
    machine:load('calendar_midnight')
end)
_G.noki8890_calendar_restore = {runner, pre, post,
    emu.add_machine_stop_notifier(function()
        if not complete then machine:logerror('8890_calendar_restore: FAIL incomplete\n') end
    end)}
assert(coroutine.resume(runner))

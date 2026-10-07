-- Exact architectural observation and emulator save/load; no firmware writes.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8890_clock_settlement_only = true
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_clock_input.lua')
local machine = manager.machine
local cpu = assert(machine.devices[':maincpu'])
local memory = cpu.spaces['program']
local saved, completed
local function snapshot(event)
    local sum = 0
    for address = 0x100000, 0x17fffc, 4 do
        sum = ((sum << 5) - sum + memory:read_u32(address)) & 0xffffffff
    end
    local state = {machine.time:as_double(), cpu.state['R15'].value,
        cpu.state['R13'].value, sum}
    machine:logerror(string.format('8890_state: event=%s pc=%08x sp=%08x ram=%08x t=%.9f\n',
        event, state[2], state[3], state[4], state[1]))
    return state
end
local pre_save = emu.add_machine_pre_save_notifier(function()
    saved = snapshot('saved')
    machine:logerror(string.format('state_replay: phase=reference event=begin t=%.9f\n', saved[1]))
end)
local post_load = emu.add_machine_post_load_notifier(function()
    local restored = snapshot('restored')
    assert(saved, 'save observation absent')
    for index = 1, 4 do assert(restored[index] == saved[index], 'architectural state mismatch') end
    machine:logerror(string.format('state_roundtrip: result=pass scenario=8890_idle requested_at=%.9f t=%.9f\n', saved[1], restored[1]))
    machine:logerror(string.format('state_replay: phase=restored event=begin t=%.9f\n', restored[1]))
    local replay = coroutine.create(function()
        assert(emu.wait(1))
        machine:logerror(string.format('state_replay: phase=restored event=end t=%.9f\n', machine.time:as_double()))
        machine.screens[':screen']:snapshot('8890_state_idle_restored.png')
        local key = assert(machine.ioport.ports[':COL.1'].fields['Menu'])
        machine:logerror('8890_state_physical: key=Menu\n')
        key:set_value(1)
        assert(emu.wait(0.15))
        key:set_value(0)
        assert(emu.wait(2))
        machine.screens[':screen']:snapshot('8890_state_idle_menu.png')
        completed = true
    end)
    _G.noki8890_state_replay = replay
    assert(coroutine.resume(replay))
end)
local runner = coroutine.create(function()
    assert(emu.wait(42))
    machine:save('8890_idle')
    assert(emu.wait(1))
    assert(saved, 'save did not execute')
    machine:logerror(string.format('state_replay: phase=reference event=end t=%.9f\n', machine.time:as_double()))
    machine.screens[':screen']:snapshot('8890_state_idle_reference.png')
    machine:load('8890_idle')
end)
_G.noki8890_state_idle = {runner, pre_save, post_load,
    emu.add_machine_stop_notifier(function()
        if not completed then machine:logerror('8890_state: FAIL incomplete\n') end
    end)}
assert(coroutine.resume(runner))

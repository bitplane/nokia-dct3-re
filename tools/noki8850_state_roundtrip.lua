-- Emulator restoration; physical inputs and read-only observations.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
local sms = _G.noki8850_state_sms == true
local idle = _G.noki8850_state_idle == true
local scenario = sms and 'sms' or idle and 'idle' or 'call'
_G.noki8850_call_idle_only = idle
dofile(directory .. (sms and 'noki8850_incoming_sms_input.lua' or 'noki8850_outgoing_call_input.lua'))
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
    machine:logerror(string.format('8850_state: event=%s pc=%08x sp=%08x ram=%08x t=%.9f\n',
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
    machine:logerror(string.format('state_roundtrip: result=pass scenario=8850_%s requested_at=%.9f t=%.9f\n', scenario, saved[1], restored[1]))
    machine:logerror(string.format('state_replay: phase=restored event=begin t=%.9f\n', restored[1]))
    local replay = coroutine.create(function()
        assert(emu.wait(1))
        machine:logerror(string.format('state_replay: phase=restored event=end t=%.9f\n', machine.time:as_double()))
        machine.screens[':screen']:snapshot('8850_state_call_restored.png')
        if sms then
            dofile(directory .. 'noki8850_sms_read.lua')
            assert(emu.wait(10))
            completed = true
            return
        end
        if idle then
            local key = assert(machine.ioport.ports[':COL.1'].fields['Menu'])
            machine:logerror('8850_state_physical: key=Menu\n')
            key:set_value(1)
            assert(emu.wait(0.15))
            key:set_value(0)
            assert(emu.wait(2))
            machine.screens[':screen']:snapshot('8850_state_idle_menu.png')
            completed = true
            return
        end
        assert(emu.wait(2))
        local key = assert(machine.ioport.ports[':COL.0'].fields['End'])
        machine:logerror('8850_call_physical: action=end\n')
        key:set_value(1)
        assert(emu.wait(0.15))
        key:set_value(0)
        assert(emu.wait(8))
        machine.screens[':screen']:snapshot('8850_after_outgoing_call.png')
        completed = true
    end)
    _G.noki8850_state_replay = replay
    assert(coroutine.resume(replay))
end)
local runner = coroutine.create(function()
    assert(emu.wait(sms and 19 or idle and 32 or 40))
    machine:save('8850_call')
    assert(emu.wait(1))
    assert(saved, 'save did not execute')
    machine:logerror(string.format('state_replay: phase=reference event=end t=%.9f\n', machine.time:as_double()))
    machine.screens[':screen']:snapshot('8850_state_call_reference.png')
    machine:load('8850_call')
end)
_G.noki8850_state_call = {runner, pre_save, post_load,
    emu.add_machine_stop_notifier(function()
        if not completed then machine:logerror('8850_state: FAIL incomplete\n') end
    end)}
assert(coroutine.resume(runner))

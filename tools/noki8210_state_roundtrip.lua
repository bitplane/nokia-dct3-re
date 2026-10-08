-- Exact emulator restoration; physical inputs and read-only observations.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
_G.noki8210_security_only = true
local call = _G.noki8210_state_call == true
local incoming = _G.noki8210_state_incoming == true
local incoming_alerting = incoming and _G.noki8210_incoming_alerting_hold == true
local sms = _G.noki8210_state_sms == true
local scenario = incoming_alerting and 'incoming_alerting_call' or incoming and 'incoming_call' or sms and 'sms' or call and 'call' or 'idle'
_G.noki8210_call_hold = call
_G.noki8210_incoming_hold = incoming
dofile(directory ..
    (incoming and 'noki8210_incoming_call_input.lua' or
     call and 'noki8210_outgoing_call_input.lua' or 'noki8210_security_input.lua'))
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
    machine:logerror(string.format('8210_state: event=%s pc=%08x sp=%08x ram=%08x t=%.9f\n',
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
    for index = 1, 4 do assert(saved[index] == restored[index], 'architectural state mismatch') end
    machine:logerror(string.format('state_roundtrip: result=pass scenario=8210_%s requested_at=%.9f t=%.9f\n', scenario, saved[1], restored[1]))
    machine:logerror(string.format('state_replay: phase=restored event=begin t=%.9f\n', restored[1]))
    local replay = coroutine.create(function()
        assert(emu.wait(1))
        machine:logerror(string.format('state_replay: phase=restored event=end t=%.9f\n', machine.time:as_double()))
        machine.screens[':screen']:snapshot('8210_state_' .. scenario .. '_restored.png')
        if _G.noki8210_sip_restore_idle then
            completed = true
            return
        end
        if sms then
            dofile(directory .. 'noki8210_sms_read.lua')
            assert(emu.wait(6))
            completed = true
            return
        end
        if call or incoming then
            assert(emu.wait(2))
            local key = assert(machine.ioport.ports[':COL.0'].fields['End'])
            machine:logerror('8210_call_physical: action=end\n')
            key:set_value(1)
            assert(emu.wait(0.15))
            key:set_value(0)
            assert(emu.wait(8))
            machine.screens[':screen']:snapshot('8210_state_call_released.png')
            completed = true
            return
        end
        local key = assert(machine.ioport.ports[':COL.1'].fields['Menu'])
        machine:logerror('8210_state_physical: key=Menu\n')
        key:set_value(1)
        assert(emu.wait(0.15))
        key:set_value(0)
        assert(emu.wait(2))
        machine.screens[':screen']:snapshot('8210_state_idle_menu.png')
        completed = true
    end)
    _G.noki8210_state_replay = replay
    assert(coroutine.resume(replay))
end)
local runner = coroutine.create(function()
    assert(emu.wait(incoming_alerting and 36 or incoming and 42 or sms and 21 or call and 34 or 32))
    machine:save('8210_' .. scenario)
    assert(emu.wait(1))
    assert(saved, 'save did not execute')
    machine:logerror(string.format('state_replay: phase=reference event=end t=%.9f\n', machine.time:as_double()))
    machine.screens[':screen']:snapshot('8210_state_' .. scenario .. '_reference.png')
    machine:load('8210_' .. scenario)
end)
_G.noki8210_state_roundtrip = {runner, pre_save, post_load,
    emu.add_machine_stop_notifier(function()
        if not completed then machine:logerror('8210_state: FAIL incomplete\n') end
    end)}
assert(coroutine.resume(runner))

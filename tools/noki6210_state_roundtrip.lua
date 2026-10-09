-- Read-only observation and emulator save/load; no firmware-state writes.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
local scenario = assert(_G.noki6210_state_scenario)
local inputs = {idle='staged_observe', incoming_alerting='staged_observe', call='outgoing_call_input', sms='incoming_sms_input',
                divert='divert_lifecycle_input'}
dofile(directory .. 'noki6210_' .. assert(inputs[scenario]) .. '.lua')
if scenario == 'idle' and os.getenv('NOKIA_DCT3_6210_PIN_ENTRY') == '1' then
    dofile(directory .. 'noki6210_security_input.lua')
end
local machine = manager.machine
local save_time = ({idle=19, incoming_alerting=40, call=31, sms=15.02, divert=36})[scenario]
local saved, completed
local capture = dofile(directory .. 'arm_architecture_snapshot.lua')
local function snapshot() return capture(machine) end
local function log_snapshot(event, state)
    machine:logerror(string.format('6210_state: scenario=%s event=%s pc=%08x sp=%08x ram=%08x cpu=%s t=%.9f\n',
        scenario, event, state.pc, state.sp, state.ram, state.cpu, state.time))
end
local pre_save = emu.add_machine_pre_save_notifier(function()
    saved = snapshot()
    log_snapshot('saved', saved)
    machine:logerror(string.format('state_replay: phase=reference event=begin t=%.9f\n', saved.time))
end)
local post_load = emu.add_machine_post_load_notifier(function()
    assert(saved, 'missing save observation')
    local restored = snapshot()
    log_snapshot('restored', restored)
    assert(restored.time == saved.time, 'saved timeline was not restored exactly')
    assert(restored.pc == saved.pc and restored.sp == saved.sp and restored.ram == saved.ram and
        restored.cpu == saved.cpu,
        'CPU/RAM snapshot not restored exactly')
    machine:logerror(string.format('state_roundtrip: result=pass scenario=%s requested_at=%.9f t=%.9f\n',
        scenario, saved.time, restored.time))
    machine:logerror(string.format('state_replay: phase=restored event=begin t=%.9f\n', restored.time))
    local replay = coroutine.create(function()
        assert(emu.wait(1))
        machine:logerror(string.format('state_replay: phase=restored event=end t=%.9f\n', machine.time:as_double()))
        -- Lua waits are host-side and are cancelled on load. Resume only
        -- physical fixture input here; no handset task/state is restored by Lua.
        if scenario == 'incoming_alerting' then
            assert(emu.wait(12))
            machine.screens[':screen']:snapshot('6210_sip_missed_call.png')
            local key = assert(machine.ioport.ports[':COL.1'].fields['Right Softkey / C'])
            machine:logerror('6210_sip_cancel: physical Exit\n')
            key:set_value(1)
            assert(emu.wait(0.15))
            key:set_value(0)
            assert(emu.wait(1.85))
            machine.screens[':screen']:snapshot('6210_sip_after_cancel.png')
        elseif scenario == 'divert' then
            assert(_G.noki6210_resume_divert)()
        elseif scenario == 'call' then
            assert(emu.wait(3))
            local key = assert(machine.ioport.ports[':COL.0'].fields['End'])
            machine:logerror('6210_call_physical: action=end\n')
            key:set_value(1)
            assert(emu.wait(0.15))
            key:set_value(0)
            assert(emu.wait(8))
            machine.screens[':screen']:snapshot('6210_state_call_after_release.png')
        elseif scenario == 'sms' then
            assert(emu.wait(22 - machine.time:as_double()))
            for index = 1, 2 do
                local key = assert(machine.ioport.ports[':COL.1'].fields['Left Softkey / Menu'])
                machine:logerror('6210_sms_physical: action=read_' .. index .. '\n')
                key:set_value(1)
                assert(emu.wait(0.15))
                key:set_value(0)
                assert(emu.wait(1.85))
                machine.screens[':screen']:snapshot('6210_state_sms_read_' .. index .. '.png')
            end
        end
        completed = true
    end)
    _G.noki6210_state_replay = replay
    assert(coroutine.resume(replay))
end)
local runner = coroutine.create(function()
    assert(emu.wait(save_time))
    machine:save('6210_' .. scenario)
    assert(emu.wait(1))
    assert(saved, 'save did not execute')
    machine:logerror(string.format('state_replay: phase=reference event=end t=%.9f\n', machine.time:as_double()))
    machine:load('6210_' .. scenario)
end)
_G.noki6210_state_roundtrip = {runner, pre_save, post_load,
    emu.add_machine_stop_notifier(function()
        if not completed then machine:logerror('6210_state: FAIL incomplete\n') end
    end)}
assert(coroutine.resume(runner))

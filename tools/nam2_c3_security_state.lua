-- Save/load and physical contacts only; no firmware/MMIO writes.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
local machine = manager.machine
assert(machine.system.name == 'nam2c3')
local capture = dofile(directory .. 'arm_architecture_snapshot.lua')
local actions = {
    {2, 'Keypad 1'}, {3, 'Keypad 2'}, {4, 'Keypad 3'},
    {2, 'Keypad 4'}, {3, 'Keypad 5'}, {1, 'Left Softkey / Menu'},
}
local saved, reference, completed
local retained = {}
local function observe(event)
    local state = capture(machine)
    machine:logerror(string.format('nam2_security_state: event=%s pc=%08x sp=%08x ram=%08x cpu=%s t=%.9f\n',
        event, state.pc, state.sp, state.ram, state.cpu, state.time))
    return state
end
local function equal(expected, actual)
    for key, value in pairs(expected) do
        assert(value == actual[key], 'architectural state differs: ' .. key)
    end
end
local function input(phase)
    local runner = coroutine.create(function()
        assert(emu.wait(0.25))
        for index, action in ipairs(actions) do
            local key = assert(machine.ioport.ports[':COL.' .. action[1]].fields[action[2]])
            machine:logerror(string.format('nam2_security_state_key: phase=%s index=%d key=%s t=%.9f\n',
                phase, index, action[2], machine.time:as_double()))
            key:set_value(1)
            assert(emu.wait(0.2))
            key:set_value(0)
            assert(emu.wait(0.3))
        end
        local state = observe(phase .. '_end')
        machine.screens[':screen']:snapshot('nam2_security_state_' .. phase .. '.png')
        if phase == 'reference' then
            reference = state
            assert(emu.wait(0.1))
            machine:load('nam2_security')
        else
            equal(assert(reference), state)
            completed = true
            machine:logerror('nam2_security_state: result=pass physical_replay=1 idle_claim=0\n')
        end
    end)
    retained[#retained + 1] = runner
    assert(coroutine.resume(runner))
end
retained[#retained + 1] = emu.add_machine_pre_save_notifier(function()
    saved = observe('saved')
    input('reference')
end)
retained[#retained + 1] = emu.add_machine_post_load_notifier(function()
    equal(assert(saved), observe('restored'))
    input('replay')
end)
retained[#retained + 1] = emu.add_machine_stop_notifier(function()
    if not completed then machine:logerror('nam2_security_state: FAIL incomplete\n') end
end)
local runner = coroutine.create(function()
    assert(emu.wait(7.75))
    machine:save('nam2_security')
end)
retained[#retained + 1] = runner
_G.nam2_security_state_retained = retained
assert(coroutine.resume(runner))

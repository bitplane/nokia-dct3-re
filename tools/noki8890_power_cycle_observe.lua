-- Physical power inputs and screenshots; no firmware writes or charger wake.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
_G.noki8890_clock_settlement_only = true
dofile(directory .. 'noki8890_clock_input.lua')
local machine = manager.machine
local power = assert(machine.ioport.ports[':PWR'].fields['Power'])
local function at(when)
    return emu.wait(when - machine.time:as_double())
end
local function restart()
    if not at(58) then return end
    -- Re-arm only debugger log caps, so both boots have full passive evidence.
    machine.debugger:command('do temp6=0;do temp7=0;do temp8=0;do temp9=0')
    machine:logerror('8890_power_physical: action=restart_press\n')
    power:set_value(1)
    if not emu.wait(2) then power:set_value(0); return end
    power:set_value(0)
    machine:logerror('8890_power_physical: action=restart_release\n')
    if not at(65) then return end
    machine.screens[':screen']:snapshot('8890_power_after_key.png')
    if not at(71) then return end
    machine.screens[':screen']:snapshot('8890_power_restart_security.png')
    if not at(76) then return end
    for _, item in ipairs({{2, 'Keypad 1'}, {3, 'Keypad 2'}, {4, 'Keypad 3'},
            {2, 'Keypad 4'}, {3, 'Keypad 5'}, {1, 'Menu'}}) do
        local key = assert(machine.ioport.ports[':COL.' .. item[1]].fields[item[2]])
        machine:logerror('8890_power_security: key=' .. item[2] .. '\n')
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return end
        key:set_value(0)
        if not emu.wait(0.35) then return end
    end
    for _, when in ipairs({81, 86}) do
        if not at(when) then return end
        machine.screens[':screen']:snapshot('8890_power_restart_' .. when .. '.png')
    end
end
local saved
local function architecture(event)
    local cpu = assert(machine.devices[':maincpu'])
    local memory, sum = cpu.spaces['program'], 0
    for address = 0x100000, 0x17fffc, 4 do
        sum = ((sum << 5) - sum + memory:read_u32(address)) & 0xffffffff
    end
    local state = {machine.time:as_double(), cpu.state['R15'].value,
        cpu.state['R13'].value, sum}
    machine:logerror(string.format('8890_power_state: event=%s pc=%08x sp=%08x ram=%08x t=%.9f\n',
        event, state[2], state[3], state[4], state[1]))
    return state
end
if _G.noki8890_power_restore_off then
    _G.noki8890_power_state_notifiers = {
        emu.add_machine_pre_save_notifier(function()
            saved = architecture('saved')
            machine:logerror(string.format('8890_power_replay: phase=reference event=begin t=%.9f\n', saved[1]))
        end),
        emu.add_machine_post_load_notifier(function()
            local restored = architecture('restored')
            assert(saved, 'off-state save observation absent')
            for index = 1, 4 do assert(saved[index] == restored[index], 'off-state architecture mismatch') end
            machine:logerror(string.format('8890_power_replay: phase=restored event=begin t=%.9f\n', restored[1]))
            local replay = coroutine.create(function()
                assert(emu.wait(1))
                machine:logerror(string.format('8890_power_replay: phase=restored event=end t=%.9f\n', machine.time:as_double()))
                machine.screens[':screen']:snapshot('8890_power_off_restored.png')
                restart()
            end)
            _G.noki8890_power_restored = replay
            assert(coroutine.resume(replay))
        end),
    }
end
local input = coroutine.create(function()
    if not at(45) then return end
    machine.screens[':screen']:snapshot('8890_power_idle.png')
    machine:logerror('8890_power_physical: action=shutdown_press\n')
    power:set_value(1)
    if not emu.wait(4) then power:set_value(0); return end
    power:set_value(0)
    machine:logerror('8890_power_physical: action=shutdown_release\n')
    if not at(51.8) then return end
    machine.screens[':screen']:snapshot('8890_power_off.png')
    if _G.noki8890_power_restore_off then
        assert(at(53))
        machine:save('8890_power_off')
        assert(emu.wait(1))
        assert(saved, 'off-state save did not execute')
        machine:logerror(string.format('8890_power_replay: phase=reference event=end t=%.9f\n', machine.time:as_double()))
        machine.screens[':screen']:snapshot('8890_power_off_reference.png')
        machine:load('8890_power_off')
        return
    end
    restart()
end)
_G.noki8890_power_cycle_observe = input
assert(coroutine.resume(input))

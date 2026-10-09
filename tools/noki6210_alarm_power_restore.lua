-- Save/load only: the shared fixture still supplies every physical input.
local source = debug.getinfo(1, 'S').source:sub(2)
local machine = manager.machine
local saved
local function architecture(event)
    local cpu = machine.devices[':maincpu']
    local memory, sum = cpu.spaces['program'], 0
    for address = 0x100000, 0x17fffc, 4 do
        sum = ((sum << 5) - sum + memory:read_u32(address)) & 0xffffffff
    end
    local registers = {}
    for index = 0, 15 do
        registers[#registers + 1] = string.format('%08x', cpu.state['R' .. index].value)
    end
    for _, name in ipairs({'CPSR', 'FR8', 'FR9', 'FR10', 'FR11', 'FR12', 'FR13', 'FR14', 'FR16',
                           'IR13', 'IR14', 'IR16', 'SR13', 'SR14', 'SR16',
                           'AR13', 'AR14', 'AR16', 'UR13', 'UR14', 'UR16'}) do
        registers[#registers + 1] = string.format('%08x', cpu.state[name].value)
    end
    local state = {machine.time:as_double(), cpu.state['R15'].value,
        cpu.state['R13'].value, sum, table.concat(registers, ',')}
    machine:logerror(string.format('6210_alarm_state: event=%s pc=%08x sp=%08x ram=%08x cpu=%s t=%.9f\n',
        event, state[2], state[3], state[4], state[5], state[1]))
    return state
end
_G.noki6210_alarm_restore_notifiers = {
    emu.add_machine_pre_save_notifier(function()
        saved = architecture('saved')
        machine:logerror(string.format('6210_alarm_replay: phase=reference event=begin t=%.9f\n', saved[1]))
    end),
    emu.add_machine_post_load_notifier(function()
        local restored = architecture('restored')
        assert(saved, 'alarm save observation absent')
        for index = 1, #saved do assert(saved[index] == restored[index], 'alarm architecture mismatch') end
        machine:logerror(string.format('6210_alarm_replay: phase=restored event=begin t=%.9f\n', restored[1]))
        local replay = coroutine.create(function()
            assert(emu.wait(1.25))
            machine:logerror(string.format('6210_alarm_replay: phase=restored event=end t=%.9f\n', machine.time:as_double()))
            machine.screens[':screen']:snapshot('6210_alarm_off_restored.png')
            assert(coroutine.resume(_G.noki6210_alarm_probe))
        end)
        _G.noki6210_alarm_restored = replay
        assert(coroutine.resume(replay))
    end),
}
_G.noki6210_alarm_restore_checkpoint = function()
    machine:save('6210_alarm_off')
    -- End between RTC ticks so callback order at a shared timestamp cannot
    -- put the reference tick outside its window but the restored tick inside.
    assert(emu.wait(1.25))
    assert(saved, 'alarm save did not execute')
    machine:logerror(string.format('6210_alarm_replay: phase=reference event=end t=%.9f\n', machine.time:as_double()))
    machine.screens[':screen']:snapshot('6210_alarm_off_reference.png')
    machine:load('6210_alarm_off')
    -- No emu.wait timer from the abandoned timeline may resume physical input.
    coroutine.yield()
    return 1.25
end
_G.noki6210_alarm_power_choice = _G.noki6210_alarm_power_choice or 'no'
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_alarm_power_off.lua')

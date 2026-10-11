-- Save the open card menu, then replay a physical selection from that state.
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
local product = assert(_G.dct3_toolkit_menu_state_product)
assert(product == '8210' or product == '8850' or product == '8890', 'unvalidated state fixture product')
local prefix = 'noki' .. product .. '_toolkit_menu'
local actions = product == '8210' and 'toolkit_physical' or 'toolkit_interactive'
_G[prefix .. '_hold'] = true
dofile(directory .. prefix .. '_input.lua')
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
    machine:logerror(string.format('%s_menu_state: event=%s pc=%08x sp=%08x ram=%08x t=%.9f\n',
        product, event, state[2], state[3], state[4], state[1]))
    return state
end
local function select(phase)
    local key = assert(machine.ioport.ports[':COL.1'].fields['Menu'])
    machine:logerror(product .. '_' .. actions .. ': action=menu_select phase=' .. phase .. '\n')
    key:set_value(1)
    assert(emu.wait(0.15))
    key:set_value(0)
    assert(emu.wait(2.85))
    machine.screens[':screen']:snapshot(product .. '_toolkit_menu_' .. phase .. '.png')
    machine:logerror(string.format('state_replay: phase=%s event=end t=%.9f\n',
        phase, machine.time:as_double()))
end
local pre_save = emu.add_machine_pre_save_notifier(function()
    saved = snapshot('saved')
    machine:logerror(string.format('state_replay: phase=reference event=begin t=%.9f\n', saved[1]))
end)
local post_load = emu.add_machine_post_load_notifier(function()
    local restored = snapshot('restored')
    assert(saved, 'save observation absent')
    for index = 1, 4 do assert(saved[index] == restored[index], 'architectural state mismatch') end
    machine:logerror(string.format('state_roundtrip: result=pass scenario=%s_toolkit_menu requested_at=%.9f t=%.9f\n',
        product, saved[1], restored[1]))
    machine:logerror(string.format('state_replay: phase=restored event=begin t=%.9f\n', restored[1]))
    local replay = coroutine.create(function()
        assert(emu.wait(1))
        select('restored')
        completed = true
    end)
    _G[prefix .. '_replay'] = replay
    assert(coroutine.resume(replay))
end)
local runner = coroutine.create(function()
    assert(emu.wait(product == '8890' and 98 or 57))
    machine:save(product .. '_toolkit_menu')
    assert(emu.wait(1))
    assert(saved, 'save did not execute')
    select('reference')
    machine:load(product .. '_toolkit_menu')
end)
_G[prefix .. '_state'] = {runner, pre_save, post_load,
    emu.add_machine_stop_notifier(function()
        if not completed then machine:logerror(product .. '_menu_state: FAIL incomplete\n') end
    end)}
assert(coroutine.resume(runner))

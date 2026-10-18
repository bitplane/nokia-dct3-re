-- Active native serial replay; only save/load and physical fixture input.
local directory = assert(debug.getinfo(1, 'S').source:match('^@(.*/)'))
dofile(directory .. 'c54x_rom4_tone_observe.lua')
local machine = manager.machine
local wire = machine.system.name == 'nse1bit' or machine.system.name == 'nse1pin' or machine.system.name == 'nse1aclk' or machine.system.name == 'nse1pclk'
assert(machine.system.name == 'nse1pcm' or wire)
local dsp = assert(machine.devices[':dsp_c54x:cpu'])
local mcu = assert(machine.devices[':maincpu'])
local key = assert(machine.ioport.ports[':COL.3'].fields['Keypad 2'])
local saved, restored, stage = nil, false, 0
local checkpoint = coroutine.create(function()
    if not wire then return end
    -- Eight microseconds into a frame: preserve a partial native shift, not
    -- only a word boundary. Save scheduling remains owned by MAME.
    local deadline = 8.150008
    while machine.time:as_double() + 1e-12 < deadline do
        emu.wait(deadline - machine.time:as_double())
    end
    stage = 1
    machine:save('native_pcm_active')
end)
assert(coroutine.resume(checkpoint))
-- A load can resume emu.wait early. Recheck absolute time rather than letting
-- that wakeup turn into an unintended physical release during the replay.
local keyboard = coroutine.create(function()
    local function until_time(deadline)
        while machine.time:as_double() + 1e-9 < deadline do
            emu.wait(deadline - machine.time:as_double())
        end
    end
    until_time(8.0)
    key:set_value(1)
    machine:logerror(string.format('input-press: t=%.6f name=2 source=native_pcm_restore\n', machine.time:as_double()))
    until_time(8.22)
    key:clear_value()
    machine:logerror(string.format('input-release: t=%.6f name=2 source=native_pcm_restore\n', machine.time:as_double()))
end)
assert(coroutine.resume(keyboard))
local function snapshot()
    local fields = {string.format('t=%.9f', machine.time:as_double())}
    for _, name in ipairs({'PC', 'SP', 'ST0', 'ST1', 'A', 'B', 'T',
        'AR0', 'AR1', 'AR2', 'AR3', 'AR4', 'AR5', 'AR6', 'AR7', 'IFR', 'IMR'}) do
        fields[#fields + 1] = string.format('dsp_%s=%010x', name, dsp.state[name].value & 0xffffffffff)
    end
    local pc = mcu.state['PC'].value
    local canonical = mcu.state['GENPC'].value
    assert(pc == canonical, 'ARM7 PC export differs from architectural GENPC')
    fields[#fields + 1] = string.format('mcu_PC=%08x', pc)
    for i = 0, 15 do fields[#fields + 1] = string.format('mcu_R%d=%08x', i, mcu.state['R' .. i].value) end
    fields[#fields + 1] = string.format('mcu_CPSR=%08x', mcu.state['CPSR'].value)
    -- Do not read BDRR: observing it would acknowledge a pending sample.
    for _, address in ipairs({0x21, 0x22, 0x23, 0xf0, 0xf1, 0xf2, 0xf3,
        0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe}) do
        fields[#fields + 1] = string.format('data_%04x=%04x', address, dsp.spaces['data']:read_u16(address))
    end
    return table.concat(fields, ' ')
end
local before = emu.add_machine_pre_save_notifier(function()
    assert(stage == 1 and not saved)
    saved = snapshot()
    machine:logerror('native_pcm_restore: phase=saved ' .. saved .. '\n')
end)
local after = emu.add_machine_post_load_notifier(function()
    local current = snapshot()
    machine:logerror('native_pcm_restore: phase=restored ' .. current .. '\n')
    assert(stage == 2 and saved == current, 'native serial checkpoint differs after load')
    restored = true
end)
local frames = emu.register_frame_done(function()
    local now = machine.time:as_double()
    if not wire and stage == 0 and now >= 8.15 then
        stage = 1
        machine:save('native_pcm_active')
    elseif stage == 1 and saved and now >= 8.20 then
        stage = 2
        machine:load('native_pcm_active')
    elseif stage == 2 and restored and now >= 8.20 then
        stage = 3
        machine:logerror(string.format('native_pcm_restore: phase=complete t=%.9f physical_release_replayed=0\n', now))
    end
end)
local stop = emu.add_machine_stop_notifier(function()
    assert(stage == 3, 'native serial replay did not complete')
end)
_G.native_pcm_restore = {before, after, frames, stop, keyboard, checkpoint}

-- Observe exact save boundaries; only I/O 21's non-destructive word is read.
local machine = manager.machine
local cpu = assert(machine.devices[':dsp_c54x:cpu'])
local io = assert(cpu.spaces['io'])
local data = assert(cpu.spaces['data'])
local saved
local function snapshot()
    return string.format('t=%.9f pc=%04x st0=%04x st1=%04x sp=%04x io21=%04x bspc22=%04x',
        machine.time:as_double(), cpu.state['PC'].value,
        cpu.state['ST0'].value, cpu.state['ST1'].value,
        cpu.state['SP'].value, io:read_u16(0x21), data:read_u16(0x22))
end
local before = emu.add_machine_pre_save_notifier(function()
    saved = snapshot()
    machine:logerror('rom4_codec_state: phase=saved ' .. saved .. '\n')
end)
local after = emu.add_machine_post_load_notifier(function()
    local restored = snapshot()
    machine:logerror('rom4_codec_state: phase=restored ' .. restored .. '\n')
    assert(saved and saved == restored, 'native codec state differs after load')
end)
_G.rom4_codec_restore = {before, after}
local directory = assert(debug.getinfo(1, 'S').source:match('^@(.*/)'))
dofile(directory .. '../mame_nokia_dct3_input_exerciser.lua')

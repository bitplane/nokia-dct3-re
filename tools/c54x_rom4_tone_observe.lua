-- Organic key fixture plus passive shared-tone and serial-word observations.
local machine = manager.machine
local dsp = assert(machine.devices[':dsp_c54x:cpu'])
local mcu = assert(machine.devices[':maincpu'])
local taps, counts, emitted = {}, {}, {}
local function watch(cpu, kind, address, owner, last)
    local space = assert(cpu.spaces[kind])
    for _, direction in ipairs({'read', 'write'}) do
        local key = owner .. '_' .. address .. '_' .. direction
        counts[key] = 0
        local install = direction == 'read' and space.install_read_tap or space.install_write_tap
        taps[#taps + 1] = install(space, address, last or address, key, function(_, value, mask)
            counts[key] = counts[key] + 1
            local phase = machine.time:as_double() >= 7 and 'interactive' or 'boot'
            local output_key = key .. '_' .. phase
            emitted[output_key] = (emitted[output_key] or 0) + 1
            if emitted[output_key] <= 16 then
                machine:logerror(string.format(
                    'rom4_tone_access: owner=%s direction=%s address=%06x value=%04x mask=%04x pc=%06x t=%.9f\n',
                    owner, direction, address, value, mask,
                    cpu.state['PC'].value, machine.time:as_double()))
            end
        end)
    end
end
-- ARM program-space taps cover a complete 32-bit bus word; keep raw masks so
-- the two shared halfwords are not conflated by an assumed byte lane.
watch(mcu, 'program', 0x100ac, 'mcu', 0x100af)
watch(dsp, 'data', 0x0856, 'dsp')
watch(dsp, 'data', 0x00fe, 'dsp')
watch(dsp, 'data', 0x06be, 'dsp')
watch(dsp, 'io', 0x002c, 'cobba_select')
watch(dsp, 'io', 0x002d, 'cobba_data')
local directory = assert(debug.getinfo(1, 'S').source:match('^@(.*/)'))
dofile(directory .. 'c54x_rom4_codec_observe.lua')
local stop = emu.add_machine_stop_notifier(function()
    local serial = assert(_G.rom4_codec_observe[2])
    machine:logerror(string.format(
        'rom4_tone_summary: tx_words=%d rx_reads=%d tone_reads=%d tone_copies=%d t=%.9f\n',
        serial['data_write_33'], serial['data_read_32'],
        counts['dsp_2134_read'], counts['dsp_254_write'], machine.time:as_double()))
end)
-- IMR/IFR bypass address-space taps inside the CPU. These debugger state
-- reads and BSPC readback are non-destructive; never poll BDRR here.
local previous
local state_observer = emu.register_frame_done(function()
    local state = string.format('imr=%04x ifr=%04x bspc22=%04x',
        dsp.state['IMR'].value, dsp.state['IFR'].value,
        dsp.spaces['data']:read_u16(0x22))
    if state ~= previous then
        machine:logerror(string.format('rom4_tone_enable: %s t=%.9f\n',
            state, machine.time:as_double()))
        previous = state
    end
end)
_G.rom4_tone_observe = {taps, counts, stop, state_observer}
dofile(directory .. '../mame_nokia_dct3_input_exerciser.lua')

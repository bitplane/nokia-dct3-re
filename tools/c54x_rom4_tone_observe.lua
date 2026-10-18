-- Organic key fixture plus passive shared-tone and serial-word observations.
local machine = manager.machine
local dsp = assert(machine.devices[':dsp_c54x:cpu'])
local mcu = assert(machine.devices[':maincpu'])
local taps, counts, emitted = {}, {}, {}
local function watch(cpu, kind, address, owner, last, lane)
    local space = assert(cpu.spaces[kind])
    for _, direction in ipairs({'read', 'write'}) do
        local key = owner .. '_' .. address .. '_' .. direction
        counts[key] = 0
        local install = direction == 'read' and space.install_read_tap or space.install_write_tap
        taps[#taps + 1] = install(space, address, last or address, key, function(_, value, mask)
            if lane and (mask & lane) == 0 then return end
            counts[key] = counts[key] + 1
            local phase = machine.time:as_double() >= 7 and 'interactive' or 'boot'
            local output_key = key .. '_' .. phase
            emitted[output_key] = (emitted[output_key] or 0) + 1
            if emitted[output_key] <= 64 then
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
watch(mcu, 'program', 0x100b0, 'mcu_frequency', 0x100b3)
watch(dsp, 'data', 0x0856, 'dsp')
watch(dsp, 'data', 0x00fe, 'dsp')
watch(dsp, 'data', 0x06be, 'dsp')
watch(dsp, 'data', 0x00ef, 'codec_demand')
watch(dsp, 'program', 0x00ef, 'codec_demand_overlay')
watch(mcu, 'program', 0x100a8, 'codec_shared_mcu', 0x100ab)
-- These private shadows occupy different ARM bus halfword lanes. Keep raw
-- masks: retained HPI data is not the source of selector-8/9 subfield updates.
watch(mcu, 'program', 0x10a9e4, 'codec_selector8_shadow', 0x10a9e7, 0x0000ffff)
watch(mcu, 'program', 0x10a9e8, 'codec_selector9_shadow', 0x10a9eb, 0xffff0000)
watch(dsp, 'data', 0x0854, 'codec_shared_dsp')
watch(dsp, 'program', 0x0854, 'codec_shared_overlay')
watch(dsp, 'data', 0x0870, 'codec_pending_dsp')
watch(mcu, 'program', 0x100e0, 'codec_pending_mcu', 0x100e3)
watch(mcu, 'program', 0x30000, 'codec_dspif_mcu', 0x30003)
watch(mcu, 'program', 0x40000, 'codec_mcuif_mcu', 0x40003)
local parameter_publications = {boot = 0, interactive = 0}
taps[#taps + 1] = mcu.spaces['program']:install_write_tap(
    0x100a8, 0x100ab, 'codec_parameter_publisher', function(_, value, mask)
        if mcu.state['PC'].value ~= 0x272034 or mask ~= 0xffff0000 then return end
        local phase = machine.time:as_double() >= 7 and 'interactive' or 'boot'
        parameter_publications[phase] = parameter_publications[phase] + 1
        if parameter_publications[phase] <= 64 then
            machine:logerror(string.format(
                'rom4_codec_parameter_publish: phase=%s field=%04x wire=%04x r5=%08x r6=%08x r7=%08x r8=%08x lr=%08x t=%.9f\n',
                phase, mcu.state['R4'].value, (value >> 16) & 0xffff,
                mcu.state['R5'].value, mcu.state['R6'].value, mcu.state['R7'].value,
                mcu.state['R8'].value, mcu.state['R14'].value, machine.time:as_double()))
        end
    end)
for _, address in ipairs({0x19a2, 0x19a4, 0x1f92}) do
    watch(dsp, 'data', address, 'codec_level_local')
    watch(dsp, 'program', address, 'codec_level_overlay')
end
watch(mcu, 'program', 0x100bc, 'mcu', 0x100bf)
-- The selector-0x28 source is the lower halfword of this ARM bus word.
-- Preserve wide writes and masks to distinguish record initialization from
-- writes to the adjacent field; observing it must not read or modify RAM.
watch(mcu, 'program', 0x10b2b8, 'speech_record', 0x10b2bb)
watch(mcu, 'program', 0x10a6c4, 'speech_configuration', 0x10a6c7)
watch(dsp, 'data', 0x085e, 'speech_shared')
watch(dsp, 'data', 0x1f2b, 'speech_flags')
-- Program-overlay stores can bypass data-space taps; retain both owners.
watch(dsp, 'program', 0x1f2b, 'speech_overlay')
local speech_publications = 0
taps[#taps + 1] = mcu.spaces['program']:install_write_tap(
    0x100bc, 0x100bf, 'speech_control_selector', function(_, value, mask)
        if mask ~= 0xffff0000 or mcu.state['PC'].value ~= 0x272034 then return end
        speech_publications = speech_publications + 1
        if speech_publications <= 16 then
            machine:logerror(string.format(
                'rom4_speech_control_publish: selector=%04x input=%04x word=%04x destination=%08x merge=%08x lr=%08x t=%.9f\n',
                mcu.state['R4'].value, mcu.state['R6'].value,
                (value >> 16) & 0xffff, mcu.state['R2'].value,
                mcu.state['R12'].value, mcu.state['R14'].value, machine.time:as_double()))
        end
    end)
local speech_entries = 0
local configuration_entries = 0
local descriptor_entries = 0
taps[#taps + 1] = mcu.spaces['program']:install_read_tap(
    0x2894c8, 0x2894cb, 'speech_descriptor_entry', function()
        if mcu.state['PC'].value ~= 0x2894c8 or mcu.state['R0'].value ~= 0x10a6c4 then return end
        descriptor_entries = descriptor_entries + 1
        if descriptor_entries <= 16 then
            machine:logerror(string.format(
                'rom4_speech_descriptor_entry: destination=%08x r1=%08x r2=%08x r3=%08x caller=%08x t=%.9f\n',
                mcu.state['R0'].value, mcu.state['R1'].value,
                mcu.state['R2'].value, mcu.state['R3'].value,
                mcu.state['R14'].value, machine.time:as_double()))
        end
    end)
taps[#taps + 1] = mcu.spaces['program']:install_read_tap(
    0x28c494, 0x28c49b, 'speech_configuration_entry', function()
        local pc = mcu.state['PC'].value
        local destination = mcu.state['R1'].value
        local length = mcu.state['R2'].value
        if pc ~= 0x28c496 and pc ~= 0x28c498 then return end
        if destination > 0x10a6c6 or destination + length <= 0x10a6c6 then return end
        configuration_entries = configuration_entries + 1
        if configuration_entries <= 16 then
            machine:logerror(string.format(
                'rom4_speech_configuration_entry: selector=%04x destination=%08x length=%04x caller=%08x t=%.9f\n',
                mcu.state['R0'].value, destination, length,
                mcu.state['R14'].value, machine.time:as_double()))
        end
    end)
taps[#taps + 1] = mcu.spaces['program']:install_read_tap(
    0x271d70, 0x271d73, 'speech_field_entry', function()
        local pc = mcu.state['PC'].value
        if (pc ~= 0x271d70 and pc ~= 0x271d72) or mcu.state['R0'].value ~= 0x28 then return end
        speech_entries = speech_entries + 1
        if speech_entries <= 16 then
            machine:logerror(string.format(
                'rom4_speech_field_entry: input=%04x parameter=%04x caller=%08x pc=%08x source=%08x index=%02x t=%.9f\n',
                mcu.state['R1'].value, mcu.state['R2'].value,
                mcu.state['R14'].value, pc, mcu.state['R5'].value,
                mcu.state['R7'].value, machine.time:as_double()))
        end
    end)
watch(dsp, 'io', 0x002c, 'cobba_select')
watch(dsp, 'io', 0x002d, 'cobba_data')
-- Fetch increments PC before the program tap. Restrict this to actual helper
-- entry, not a data read of its opcode; the stack read is ordinary RAM.
local control_calls = 0
local demand_entries = 0
local execution_demand = machine.system.name == 'nse1aclk' or machine.system.name == 'nse1pclk'
for _, address in ipairs({0xa484, 0xa4cb}) do
    taps[#taps + 1] = dsp.spaces['program']:install_read_tap(
        address, address, 'codec_demand_entry_' .. address, function()
            if execution_demand then return end
            if dsp.state['PC'].value ~= address + 1 then return end
            demand_entries = demand_entries + 1
            if demand_entries <= 128 then
                machine:logerror(string.format(
                    'rom4_codec_demand_entry: hit=%d operation=%s mask=%04x previous=%04x t=%.9f\n',
                    demand_entries, address == 0xa484 and 'acquire' or 'release',
                    dsp.state['B'].value & 0xffff,
                    dsp.spaces['data']:read_u16(0x00ef), machine.time:as_double()))
            end
        end)
end
local request_reads, request_accepts, pending_request = 0, 0, nil
taps[#taps + 1] = dsp.spaces['io']:install_read_tap(1, 1, 'codec_host_request', function(_, value)
    if value & 0x8000 == 0 then return end
    pending_request = value
    request_reads = request_reads + 1
    if request_reads <= 64 then
        machine:logerror(string.format('rom4_codec_host_request: value=%04x pc=%04x t=%.9f\n',
            value, dsp.state['PC'].value, machine.time:as_double()))
    end
end)
taps[#taps + 1] = dsp.spaces['io']:install_write_tap(2, 2, 'codec_host_accept', function(_, value)
    if not pending_request then return end
    request_accepts = request_accepts + 1
    if request_accepts <= 64 then
        machine:logerror(string.format('rom4_codec_host_accept: request=%04x mask=%04x pc=%04x t=%.9f\n',
            pending_request, value, dsp.state['PC'].value, machine.time:as_double()))
    end
    pending_request = nil
end)
taps[#taps + 1] = dsp.spaces['program']:install_read_tap(
    0x45c2, 0x45c2, 'codec_control_call', function()
        if dsp.state['PC'].value ~= 0x45c3 then return end
        control_calls = control_calls + 1
        if control_calls <= 256 then
            local sp = dsp.state['SP'].value
            machine:logerror(string.format(
                'rom4_codec_control_call: hit=%d a=%010x b=%010x sp=%04x return=%04x t=%.9f\n',
                control_calls, dsp.state['A'].value, dsp.state['B'].value, sp,
                dsp.spaces['data']:read_u16(sp), machine.time:as_double()))
        end
    end)
local directory = assert(debug.getinfo(1, 'S').source:match('^@(.*/)'))
dofile(directory .. 'c54x_rom4_codec_observe.lua')
local stop = emu.add_machine_stop_notifier(function()
    local serial = assert(_G.rom4_codec_observe[2])
    machine:logerror(string.format('rom4_codec_control_summary: calls=%d t=%.9f\n',
        control_calls, machine.time:as_double()))
    if not execution_demand then
        machine:logerror(string.format('rom4_codec_demand_summary: entries=%d t=%.9f\n',
            demand_entries, machine.time:as_double()))
    end
    machine:logerror(string.format('rom4_codec_host_summary: requests=%d accepts=%d t=%.9f\n',
        request_reads, request_accepts, machine.time:as_double()))
    machine:logerror(string.format('rom4_codec_parameter_summary: boot=%d interactive=%d t=%.9f\n',
        parameter_publications.boot, parameter_publications.interactive, machine.time:as_double()))
    machine:logerror(string.format(
        'rom4_tone_summary: tx_words=%d rx_reads=%d tone_reads=%d tone_copies=%d t=%.9f\n',
        serial['data_write_33'], serial['data_read_32'],
        counts['dsp_2134_read'], counts['dsp_254_write'], machine.time:as_double()))
end)
-- IMR/IFR bypass address-space taps inside the CPU. These debugger state
-- reads and BSPC readback are non-destructive; never poll BDRR here.
local previous
local state_observer = emu.register_frame_done(function()
    local state = string.format('imr=%04x ifr=%04x bspc22=%04x bspce23=%04x',
        dsp.state['IMR'].value, dsp.state['IFR'].value,
        dsp.spaces['data']:read_u16(0x22), dsp.spaces['data']:read_u16(0x23))
    if state ~= previous then
        machine:logerror(string.format('rom4_tone_enable: %s t=%.9f\n',
            state, machine.time:as_double()))
        previous = state
    end
end)
_G.rom4_tone_observe = {taps, counts, stop, state_observer}
dofile(directory .. '../mame_nokia_dct3_input_exerciser.lua')

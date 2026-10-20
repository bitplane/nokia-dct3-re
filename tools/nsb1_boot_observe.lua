-- Passive own-image MCU/storage/DSP boundary observations; no guest writes.
local machine = manager.machine
assert(machine.system.name == 'noki5190' or machine.system.name == 'nsb1r4t')
local cpu = machine.devices[':maincpu']
local space = cpu.spaces.program
local shared_writes, handoffs, releases = 0, 0, 0
local serial_selection, lcd_commands, lcd_bytes, lcd_nonzero = 0, 0, 0, 0
local selftest_writes = 0
local mode_writes = 0
_G.nsb1_startup_mode_tap = space:install_write_tap(0x10c4c4, 0x10c4c7, 'nsb1_startup_mode', function(offset, data, mask)
    if (mask & 0xff000000) == 0 then return end
    mode_writes = mode_writes + 1
    if mode_writes <= 16 then
        machine:logerror(string.format('nsb1_startup_mode: value=%02x pc=%08x t=%.9f observer_writes=0\n',
            (data >> 24) & 255, cpu.state['R15'].value, machine.time:as_double()))
    end
end)
local simi_reads, simi_writes = 0, 0
local function simi_access(offset, data, mask, write)
    for lane = 0, 3 do
        local address, shift = offset + lane, (3 - lane) * 8
        if address >= 0x20036 and address <= 0x2003f and (mask & (255 << shift)) ~= 0 then
            if write then simi_writes = simi_writes + 1 else simi_reads = simi_reads + 1 end
            local count = write and simi_writes or simi_reads
            if count <= 8 then
                machine:logerror(string.format('nsb1_simi: direction=%s address=%08x data=%02x pc=%08x t=%.9f observer_writes=0\n',
                    write and 'W' or 'R', address, (data >> shift) & 255,
                    cpu.state['R15'].value, machine.time:as_double()))
            end
        end
    end
end
_G.nsb1_simi_read_tap = space:install_read_tap(0x20034, 0x2003f, 'nsb1_simi_read', function(offset, data, mask)
    simi_access(offset, data, mask, false)
end)
_G.nsb1_simi_write_tap = space:install_write_tap(0x20034, 0x2003f, 'nsb1_simi_write', function(offset, data, mask)
    simi_access(offset, data, mask, true)
end)
_G.nsb1_selftest_tap = space:install_write_tap(0x10fde4, 0x10fde7, 'nsb1_selftest_flags', function(offset, data, mask)
    if (mask & 0x00ff0000) == 0 then return end
    selftest_writes = selftest_writes + 1
    if selftest_writes <= 32 then
        machine:logerror(string.format('nsb1_selftest_flags: value=%02x pc=%08x t=%.9f observer_writes=0\n',
            (data >> 16) & 255, cpu.state['R15'].value, machine.time:as_double()))
    end
end)
_G.nsb1_lcd_tap = space:install_write_tap(0x20028, 0x2002f, 'nsb1_lcd_bus', function(offset, data, mask)
    if offset == 0x20028 and (mask & 0xff000000) ~= 0 then
        serial_selection = (data >> 24) & 255
    end
    if serial_selection ~= 0x21 then return end
    if offset == 0x2002c and (mask & 0xff000000) ~= 0 then
        lcd_commands = lcd_commands + 1
        if lcd_commands <= 24 then
            machine:logerror(string.format('nsb1_lcd_command: byte=%02x pc=%08x t=%.9f observer_writes=0\n',
                (data >> 24) & 255, cpu.state['R15'].value, machine.time:as_double()))
        end
    elseif offset == 0x20028 and (mask & 0xff) ~= 0 then
        lcd_bytes = lcd_bytes + 1
        if (data & 255) ~= 0 then
            lcd_nonzero = lcd_nonzero + 1
            if lcd_nonzero <= 8 then
                machine:logerror(string.format('nsb1_lcd_nonzero: byte=%02x pc=%08x t=%.9f observer_writes=0\n',
                    data & 255, cpu.state['R15'].value, machine.time:as_double()))
            end
        end
    end
end)
local released, result_reads, observing = false, 0, false
local last_results = {}
_G.nsb1_release_tap = space:install_write_tap(0x20000, 0x20003, 'nsb1_dsp_release', function(offset, data, mask)
    if (mask & 0xff00) == 0 then return end
    local command = (data >> 8) & 255
    if (command & 1) == 0 then released = false; return end
    if released then return end
    released = true
    result_reads, last_results = 0, {}
    releases = releases + 1
    if releases > 4 then return end
    local arguments = {}
    for index = 0, 6 do arguments[#arguments + 1] = string.format('%04x', space:read_u16(0x100f6 + index * 2)) end
    machine:logerror(string.format('nsb1_dsp_release: command=%02x arguments=%s pc=%08x t=%.9f observer_writes=0\n',
        command, table.concat(arguments), cpu.state['R15'].value, machine.time:as_double()))
    if cpu.state['R15'].value == 0x28f9c8 then
        local program = {}
        for index = 0, 185 do program[#program + 1] = string.format('%04x', space:read_u16(0x11e00 + index * 2)) end
        machine:logerror(string.format('nsb1_release_program: release=%d words=186 data=%s observer_writes=0\n', releases, table.concat(program)))
    end
end)
_G.nsb1_result_read_tap = space:install_read_tap(0x10000, 0x10007, 'nsb1_dsp_result', function(offset, data, mask)
    if observing or not released or result_reads >= 32 then return end
    local key = string.format('%x/%x/%x', offset, mask, cpu.state['R15'].value)
    if last_results[key] == data then return end
    last_results[key] = data
    result_reads = result_reads + 1
    machine:logerror(string.format('nsb1_dsp_result_read: address=%08x data=%08x mask=%08x pc=%08x t=%.9f observer_writes=0\n',
        offset, data, mask, cpu.state['R15'].value, machine.time:as_double()))
end)
local flag_writes = 0
_G.nsb1_flag_tap = space:install_write_tap(0x10c540, 0x10c543, 'nsb1_supervisor_flag', function(offset, data, mask)
    if (mask & 0xff000000) == 0 then return end
    flag_writes = flag_writes + 1
    if flag_writes <= 32 then
        machine:logerror(string.format('nsb1_supervisor_flag: value=%02x pc=%08x t=%.9f observer_writes=0\n',
            (data >> 24) & 255, cpu.state['R15'].value, machine.time:as_double()))
    end
end)
_G.nsb1_shared_tap = space:install_write_tap(0x10000, 0x10fff, 'nsb1_shared', function(offset, data, mask)
    shared_writes = shared_writes + 1
    local pc = cpu.state['R15'].value
    if pc == 0x28fa40 or pc == 0x28fa50 then handoffs = handoffs + 1 end
    if shared_writes <= 12 then
        machine:logerror(string.format('nsb1_shared: address=%08x data=%08x mask=%08x pc=%08x t=%.9f observer_writes=0\n',
            offset, data, mask, cpu.state['R15'].value, machine.time:as_double()))
    end
end)
local times, next_sample = {0.1, 0.5, 1, 2, 5}, 1
emu.register_frame_done(function()
    local now = machine.time:as_double()
    if next_sample > #times or now < times[next_sample] then return end
    observing = true
    if next_sample == 3 then
        local program = {}
        for index = 0, 185 do
            program[#program + 1] = string.format('%04x', space:read_u16(0x11e00 + index * 2))
        end
        machine:logerror('nsb1_program: words=186 data=' .. table.concat(program) .. ' observer_writes=0\n')
    end
    machine:logerror(string.format('nsb1_boot: t=%.9f pc=%08x sp=%08x irqmask=%02x eeprom_mode=%02x page=%02x capacity=%08x selector=%02x dsp_family=%04x catalogue0=%08x/%08x shared_writes=%d observer_writes=0\n',
        now, cpu.state['R15'].value, cpu.state['R13'].value, space:read_u8(0x2000b),
        space:read_u8(0x10c4a4), space:read_u8(0x10c4a5), space:read_u32(0x10c4a8),
        space:read_u8(0x10c466), space:read_u16(0x10ad2e),
        space:read_u32(0x10adc0), space:read_u32(0x10ad50), shared_writes))
    machine:logerror(string.format('nsb1_interrupts: t=%.9f cpsr=%08x ctrl=%02x fiqmask=%02x fiq=%02x irq=%02x ext=%02x supervisor=%02x flag_writes=%d observer_writes=0\n',
        now, cpu.state['CPSR'].value, space:read_u8(0x2000c), space:read_u8(0x2000a),
        space:read_u8(0x20008), space:read_u8(0x20009), space:read_u8(0x20016),
        space:read_u8(0x10c540), flag_writes))
    local results = {}
    for index = 0, 3 do results[#results + 1] = string.format('%04x', space:read_u16(0x10000 + index * 2)) end
    machine:logerror(string.format('nsb1_dsp_state: t=%.9f words=%s flags=%04x/%04x released=%d init_success=%02x saved_version=%04x handoffs=%d releases=%d observer_writes=0\n',
        now, table.concat(results), space:read_u16(0x100fe), space:read_u16(0x10100), released and 1 or 0,
        space:read_u8(0x10ad2d), space:read_u16(0x10ad36), handoffs, releases))
    if machine.system.name == 'nsb1r4t' and (next_sample == 3 or next_sample == 5) then
        machine.screens[':screen']:snapshot(string.format('nsb1_native_%ds.png', math.floor(now)))
    end
    machine:logerror(string.format('nsb1_lcd_summary: t=%.9f commands=%d data_bytes=%d nonzero=%d observer_writes=0\n',
        now, lcd_commands, lcd_bytes, lcd_nonzero))
    machine:logerror(string.format('nsb1_presentation: t=%.9f selftest_flags=%02x observer_writes=0\n',
        now, space:read_u8(0x10fde5)))
    machine:logerror(string.format('nsb1_input_readiness: t=%.9f startup_reason=%02x phase=%02x reports=%02x column_mask=%02x observer_writes=0\n',
        now, space:read_u8(0x10ff6d), space:read_u8(0x10ffb0),
        space:read_u8(0x10c3f5), space:read_u8(0x20033)))
    machine:logerror(string.format('nsb1_simi_summary: t=%.9f reads=%d writes=%d card_inserted=0 observer_writes=0\n',
        now, simi_reads, simi_writes))
    next_sample = next_sample + 1
    observing = false
end, 'NSB-1 passive MCU boundary')

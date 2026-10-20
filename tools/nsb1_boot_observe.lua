-- Passive own-image MCU/storage/DSP boundary observations; no guest writes.
local machine = manager.machine
assert(machine.system.name == 'noki5190')
local cpu = machine.devices[':maincpu']
local space = cpu.spaces.program
local shared_writes = 0
local released, result_reads, observing = false, 0, false
_G.nsb1_release_tap = space:install_write_tap(0x20000, 0x20003, 'nsb1_dsp_release', function(offset, data, mask)
    if (mask & 0xff00) == 0 then return end
    local command = (data >> 8) & 255
    if (command & 1) == 0 or released then return end
    released = true
    local arguments = {}
    for index = 0, 6 do arguments[#arguments + 1] = string.format('%04x', space:read_u16(0x100f6 + index * 2)) end
    machine:logerror(string.format('nsb1_dsp_release: command=%02x arguments=%s pc=%08x t=%.9f observer_writes=0\n',
        command, table.concat(arguments), cpu.state['R15'].value, machine.time:as_double()))
end)
_G.nsb1_result_read_tap = space:install_read_tap(0x10000, 0x10007, 'nsb1_dsp_result', function(offset, data, mask)
    if observing or not released or result_reads >= 8 then return end
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
    machine:logerror(string.format('nsb1_dsp_state: t=%.9f words=%s flags=%04x/%04x released=%d init_success=%02x saved_version=%04x observer_writes=0\n',
        now, table.concat(results), space:read_u16(0x100fe), space:read_u16(0x10100), released and 1 or 0,
        space:read_u8(0x10ad2d), space:read_u16(0x10ad36)))
    next_sample = next_sample + 1
    observing = false
end, 'NSB-1 passive MCU boundary')

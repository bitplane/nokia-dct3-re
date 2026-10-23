-- Observe own firmware bus accesses; never write guest memory or registers.
local machine = manager.machine
assert(machine.system.name == 'noki5110i')
local cpu = machine.devices[':maincpu']
local space = cpu.spaces.program
local shared_writes, tx, rx, commands, pixels, signal_writes = 0, 0, 0, 0, 0, 0
local selected = 0
local upload_writes = 0
local startup_polls = 0
_G.nse2_startup_poll_tap = space:install_read_tap(0x10004, 0x10007, 'nse2_startup_polls', function(_, data, mask)
    local pc = cpu.state['R15'].value
    if (pc ~= 0x2781fa and pc ~= 0x27821a) or (mask & 0xffff0000) == 0 then return end
    startup_polls = startup_polls + 1
    if startup_polls <= 64 then
        machine:logerror(string.format('nse2_startup_poll: n=%d address=00010004 data=%04x mask=%04x pc=%08x t=%.9f observer_writes=0\n',
            startup_polls, (data >> 16) & 0xffff, (mask >> 16) & 0xffff, pc, machine.time:as_double()))
    end
end)
_G.nse2_upload_tap = space:install_write_tap(0x10008, 0x10fff, 'nse2_upload_words', function(offset, data, mask)
    for lane = 0, 1 do
        local shift = (1 - lane) * 16
        if (mask & (0xffff << shift)) ~= 0 then
            upload_writes = upload_writes + 1
            if upload_writes <= 4096 then
                machine:logerror(string.format('nse2_upload_word: n=%d address=%08x data=%04x mask=%04x pc=%08x t=%.9f observer_writes=0\n',
                    upload_writes, offset + lane * 2, (data >> shift) & 0xffff,
                    (mask >> shift) & 0xffff, cpu.state['R15'].value, machine.time:as_double()))
            end
        end
    end
end)
local nv_reads = 0
local dsp_state_writes = 0
_G.nse2_dsp_state_tap = space:install_write_tap(0x10b9f4, 0x10b9f7, 'nse2_dsp_state', function(_, data, mask)
    for lane = 0, 1 do
        local shift = (3 - lane) * 8
        if (mask & (255 << shift)) ~= 0 then
            dsp_state_writes = dsp_state_writes + 1
            if dsp_state_writes <= 64 then
                machine:logerror(string.format('nse2_state_byte: address=%08x value=%02x pc=%08x t=%.9f observer_writes=0\n',
                    0x10b9f4 + lane, (data >> shift) & 255, cpu.state['R15'].value, machine.time:as_double()))
            end
        end
    end
end)
local verdict_writes = 0
_G.nse2_selftest_tap = space:install_write_tap(0x10fca0, 0x10fcbf, 'nse2_selftest_results', function(offset, data, mask)
    for lane = 0, 3 do
        local shift = (3 - lane) * 8
        if (mask & (255 << shift)) ~= 0 then
            verdict_writes = verdict_writes + 1
            if verdict_writes <= 128 then
                machine:logerror(string.format('nse2_selftest_write: address=%08x value=%02x pc=%08x t=%.9f observer_writes=0\n',
                    offset + lane, (data >> shift) & 255, cpu.state['R15'].value, machine.time:as_double()))
            end
        end
    end
end)
_G.nse2_nv_verdict_tap = space:install_write_tap(0x10fcb0, 0x10fcb3, 'nse2_nv_verdict', function(_, data, mask)
    if cpu.state['R15'].value ~= 0x27dc14 or (mask & 0x0000ff00) == 0 then return end
    machine:logerror(string.format('nse2_nv_verdict: address=0010fcb2 value=%02x pc=0027dc14 t=%.9f observer_writes=0\n',
        (data >> 8) & 255, machine.time:as_double()))
end)
_G.nse2_nv_checksum_tap = space:install_read_tap(0x28a0c4, 0x28a0c7, 'nse2_nv_checksum', function()
    if cpu.state['R15'].value ~= 0x28a0c4 then return end
    local caller = cpu.state['R14'].value
    if caller < 0x258edc or caller > 0x258fbe then return end
    local source, length = cpu.state['R0'].value, cpu.state['R1'].value
    local sp = cpu.state['R13'].value
    assert(source >= 0x100000 and source + length <= 0x110000 and length <= 480)
    assert(sp >= 0x100000 and sp + 4 <= 0x110000)
    local sum = 0
    for i = 0, length - 1 do sum = (sum + space:read_u8(source + i)) & 0xffff end
    machine:logerror(string.format('nse2_nv_checksum: lr=%08x length=%d expected=%08x stored=%08x t=%.9f observer_writes=0\n',
        caller, length, (~sum) & 0xffffffff, space:read_u32(sp), machine.time:as_double()))
end)
_G.nse2_nv_entry_tap = space:install_read_tap(0x293d90, 0x293d93, 'nse2_nv_entry', function()
    if cpu.state['R15'].value ~= 0x293d92 then return end
    nv_reads = nv_reads + 1
    if nv_reads <= 256 then
        machine:logerror(string.format('nse2_nv_read: n=%d address=%04x destination=%08x length=%d lr=%08x t=%.9f observer_writes=0\n',
            nv_reads, cpu.state['R0'].value, cpu.state['R1'].value,
            cpu.state['R2'].value, cpu.state['R14'].value, machine.time:as_double()))
    end
end)
_G.nse2_shared_tap = space:install_write_tap(0x10000, 0x10007, 'nse2_shared', function(offset, data, mask)
    for lane = 0, 1 do
        local shift = (1 - lane) * 16
        if (mask & (0xffff << shift)) ~= 0 then
            shared_writes = shared_writes + 1
            if shared_writes <= 32 then
                machine:logerror(string.format('nse2_shared: address=%08x data=%04x mask=%04x pc=%08x t=%.9f observer_writes=0\n',
                    offset + lane * 2, (data >> shift) & 0xffff,
                    (mask >> shift) & 0xffff,
                    cpu.state['R15'].value, machine.time:as_double()))
            end
        end
    end
end)
_G.nse2_io_write_tap = space:install_write_tap(0x20020, 0x2003f, 'nse2_io_write', function(offset, data, mask)
    for lane = 0, 3 do
        local address, shift = offset + lane, (3 - lane) * 8
        if (mask & (255 << shift)) ~= 0 then
            local value = (data >> shift) & 255
            if address == 0x20020 then signal_writes = signal_writes + 1 end
            if address == 0x20028 then selected = value end
            if address == 0x2002a then tx = tx + 1 end
            if selected == 0x21 and address == 0x2002c then commands = commands + 1 end
            if selected == 0x21 and address == 0x2002b then pixels = pixels + 1 end
        end
    end
end)
_G.nse2_io_read_tap = space:install_read_tap(0x2002c, 0x2002f, 'nse2_io_read', function(offset, data, mask)
    if offset == 0x2002c and (mask & 0x00ff0000) ~= 0 then rx = rx + 1 end
end)
local next_sample = 1
emu.register_frame_done(function()
    local now = machine.time:as_double()
    if now < next_sample then return end
    machine:logerror(string.format('nse2_state: t=%.9f pc=%08x cpsr=%08x sp=%08x shared_writes=%d gensio_tx=%d gensio_rx=%d lcd_commands=%d lcd_bytes=%d eeprom_signal_writes=%d observer_writes=0\n',
        now, cpu.state['R15'].value, cpu.state['CPSR'].value, cpu.state['R13'].value,
        shared_writes, tx, rx, commands, pixels, signal_writes))
    if next_sample == 1 or next_sample == 7 then
        machine.screens[':screen']:snapshot('nse2_' .. next_sample .. '.png')
    end
    next_sample = next_sample + 1
end, 'NSE-2 passive first boot')

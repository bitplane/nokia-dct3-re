-- Observe own firmware bus accesses; never write guest memory or registers.
local machine = manager.machine
assert(machine.system.name == 'noki5110i' or machine.system.name == 'nse2r4t' or machine.system.name == 'nse2r4v')
local cpu = machine.devices[':maincpu']
local space = cpu.spaces.program
local reset_entries = 0
local receive_timer_arms = 0
local fiq5_reads = 0
_G.nse2_fiq5_status_tap = space:install_read_tap(0x20008, 0x2000b, 'nse2_fiq5_status', function(_, data, mask)
    if (mask & 0xff000000) == 0 or (data & 0x20000000) == 0 then return end
    fiq5_reads = fiq5_reads + 1
    if fiq5_reads > 8 then return end
    machine:logerror(string.format('nse2_fiq5_status: status=%02x current=%04x destination=%04x pc=%08x t=%.9f observer_writes=0\n',
        (data >> 24) & 255, space:read_u16(0x20004), space:read_u16(0x20006),
        cpu.state['R15'].value, machine.time:as_double()))
end)
_G.nse2_receive_timer_tap = space:install_read_tap(0x254d38, 0x254d3b, 'nse2_receive_timer', function()
    if cpu.state['R15'].value ~= 0x254d3a or cpu.state['R0'].value ~= 0x1d then return end
    receive_timer_arms = receive_timer_arms + 1
    if receive_timer_arms > 32 then return end
    machine:logerror(string.format('nse2_receive_timer: ticks=%04x lr=%08x node_ticks=%04x node_state=%02x t=%.9f observer_writes=0\n',
        cpu.state['R1'].value, cpu.state['R14'].value, space:read_u16(0x100298),
        space:read_u8(0x10029c), machine.time:as_double()))
end)
local packet_changes = 0
_G.nse2_packet_count_tap = space:install_write_tap(0x10c764, 0x10c767, 'nse2_packet_count', function(_, data, mask)
    if (mask & 0xffff) == 0 then return end
    packet_changes = packet_changes + 1
    if packet_changes > 64 then return end
    machine:logerror(string.format('nse2_packet_count: value=%04x mask=%04x pc=%08x lr=%08x t=%.9f observer_writes=0\n',
        data & 0xffff, mask & 0xffff, cpu.state['R15'].value, cpu.state['R14'].value, machine.time:as_double()))
end)
local reset_reasons = 0
_G.nse2_reset_reason_tap = space:install_write_tap(0x10ff78, 0x10ff7b, 'nse2_reset_reason', function(_, data, mask)
    if (mask & 0xff000000) == 0 then return end
    reset_reasons = reset_reasons + 1
    if reset_reasons > 16 then return end
    machine:logerror(string.format('nse2_reset_reason: value=%02x pc=%08x lr=%08x t=%.9f observer_writes=0\n',
        (data >> 24) & 255, cpu.state['R15'].value, cpu.state['R14'].value, machine.time:as_double()))
end)
_G.nse2_reset_entry_tap = space:install_read_tap(0x299c14, 0x299c17, 'nse2_reset_entry', function()
    if cpu.state['R15'].value ~= 0x299c16 then return end
    reset_entries = reset_entries + 1
    if reset_entries > 8 then return end
    local sp, stack = cpu.state['R13'].value, {}
    if sp >= 0x100000 and sp + 64 <= 0x110000 then
        for offset = 0, 60, 4 do stack[#stack + 1] = string.format('%08x', space:read_u32(sp + offset)) end
    end
    machine:logerror(string.format('nse2_reset_entry: reason=%08x lr=%08x stack=%s t=%.9f observer_writes=0\n',
        cpu.state['R0'].value, cpu.state['R14'].value, table.concat(stack, ','), machine.time:as_double()))
end)
local dsp = machine.devices[':dsp_c54x:cpu']
local native_ring_writes = {}
local native_work_writers = {}
if dsp then
    _G.nse2_native_ring_taps = {}
    for _, address in ipairs({0x852, 0x853, 0x854, 0x855, 0x8e4, 0x8e5, 0x866, 0x6bc, 0x6be, 0x6e3}) do
        native_ring_writes[address] = 0
        _G.nse2_native_ring_taps[#_G.nse2_native_ring_taps + 1] =
            dsp.spaces.data:install_write_tap(address, address, 'nse2_native_ring', function(_, data, mask)
                native_ring_writes[address] = native_ring_writes[address] + 1
                if address == 0x6bc then
                    local pc = dsp.state['PC'].value
                    local entry = native_work_writers[pc] or {count=0, set=0, bits=0}
                    entry.count = entry.count + 1
                    if (data & 1) ~= 0 then entry.set = entry.set + 1 end
                    entry.bits = entry.bits | data
                    native_work_writers[pc] = entry
                end
                if address == 0x853 and native_ring_writes[address] <= 32 then
                    local previous = dsp.spaces.data:read_u16(address)
                    if previous < 0x52 and data ~= previous then
                        local header = dsp.spaces.data:read_u16(0x800 + previous)
                        local length, payload = header >> 8, {}
                        -- The 0x52-word ring reserves one word to distinguish
                        -- full/empty: up to 160 payload bytes plus the header.
                        if length <= 0xa0 then
                            for index = 1, (length + 1) // 2 do
                                payload[#payload + 1] = string.format('%04x',
                                    dsp.spaces.data:read_u16(0x800 + ((previous + index) % 0x52)))
                            end
                            machine:logerror(string.format('nse2_native_host_packet: previous=%04x next=%04x header=%04x words=%s t=%.9f observer_writes=0\n',
                                previous, data, header, table.concat(payload, ','), machine.time:as_double()))
                        else
                            machine:logerror(string.format('nse2_native_host_packet_invalid: previous=%04x next=%04x header=%04x t=%.9f observer_writes=0\n',
                                previous, data, header, machine.time:as_double()))
                        end
                    end
                end
                if native_ring_writes[address] > 16 then return end
                machine:logerror(string.format('nse2_native_ring: address=%04x data=%04x mask=%04x pc=%04x t=%.9f observer_writes=0\n',
                    address, data, mask, dsp.state['PC'].value, machine.time:as_double()))
            end)
    end
    local writes = 0
    _G.nse2_native_identity_tap = dsp.spaces.data:install_write_tap(0x803, 0x803, 'nse2_native_identity', function(_, data, mask)
        writes = writes + 1
        if writes <= 64 then
            machine:logerror(string.format('nse2_native_identity: n=%d data=%04x mask=%04x pc=%04x t=%.9f observer_writes=0\n',
                writes, data, mask, dsp.state['PC'].value, machine.time:as_double()))
        end
        if data == 4 and mask == 0xffff and dsp.state['PC'].value == 0xf0f then
            local words = {}
            for address = 0xf00, 0xfb9 do
                words[#words + 1] = string.format('%04x', dsp.spaces.program:read_u16(address))
            end
            machine:logerror(string.format('nse2_native_verifier_image: words=186 body=%s observer_writes=0\n', table.concat(words)))
        end
    end)
end
local shared_writes, tx, rx, commands, pixels, signal_writes = 0, 0, 0, 0, 0, 0
local selected = 0
local upload_writes = 0
local initial_native_polled = false
local startup_polls = 0
_G.nse2_startup_poll_tap = space:install_read_tap(0x10004, 0x10007, 'nse2_startup_polls', function(_, data, mask)
    local pc = cpu.state['R15'].value
    if (pc ~= 0x2781fa and pc ~= 0x27821a) or (mask & 0xffff0000) == 0 then return end
    if dsp then initial_native_polled = true end
    startup_polls = startup_polls + 1
    if startup_polls <= 64 then
        machine:logerror(string.format('nse2_startup_poll: n=%d address=00010004 data=%04x mask=%04x pc=%08x t=%.9f observer_writes=0\n',
            startup_polls, (data >> 16) & 0xffff, (mask >> 16) & 0xffff, pc, machine.time:as_double()))
    end
end)
_G.nse2_upload_tap = space:install_write_tap(0x10008, 0x10fff, 'nse2_upload_words', function(offset, data, mask)
    -- Native success starts a large ROM checksum transfer; this tap records
    -- only the initial verifier staging, not a silently truncated full dump.
    if dsp and initial_native_polled then return end
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
local average_count = 0
_G.nse2_average_count_tap = space:install_write_tap(0x10a560, 0x10a563, 'nse2_average_count', function(_, data, mask)
    if (mask & 0x0000ff00) == 0 or cpu.state['R15'].value ~= 0x2645d0 then return end
    local sp = cpu.state['R13'].value
    assert(sp >= 0x100000 and sp + 4 <= 0x110000)
    average_count = average_count + 1
    if average_count <= 128 then
        machine:logerror(string.format('nse2_average_count: remaining=%02x first=%04x second=%04x pc=002645d0 t=%.9f observer_writes=0\n',
            (data >> 8) & 255, space:read_u16(sp), space:read_u16(sp + 2), machine.time:as_double()))
    elseif average_count == 129 then
        machine:logerror('nse2_average_count_cap: reached=128\n')
    end
end)
local sample_returns = 0
_G.nse2_report14_samples = space:install_read_tap(0x2645b0, 0x2645b3, 'nse2_report14_samples', function()
    if cpu.state['R15'].value ~= 0x2645b2 then return end
    local first, second = cpu.state['R5'].value, cpu.state['R4'].value
    if first < 0x100000 or first + 2 > 0x110000 or second < 0x100000 or second + 2 > 0x110000 then return end
    sample_returns = sample_returns + 1
    if sample_returns <= 16 then
        machine:logerror(string.format('nse2_report14_sample: valid=%d first=%04x second=%04x remaining=%02x pc=002645b2 t=%.9f observer_writes=0\n',
            cpu.state['R0'].value, space:read_u16(first), space:read_u16(second),
            space:read_u8(0x10a562), machine.time:as_double()))
    end
end)
local key_reads = 0
_G.nse2_task_message_taps = {}
_G.nse2_key_init_tap = space:install_read_tap(0x299048, 0x29904b, 'nse2_key_init', function()
    if cpu.state['R15'].value ~= 0x299048 then return end
    local context = cpu.state['R4'].value
    machine:logerror(string.format('nse2_key_init: lr=%08x context=%08x t=%.9f observer_writes=0\n',
        cpu.state['R14'].value, context, machine.time:as_double()))
    if context < 0x100000 or context + 4 > 0x110000 then return end
    local address = context + 2
    if _G.nse2_task_message_taps[address] then return end
    local messages = 0
    _G.nse2_task_message_taps[address] = space:install_write_tap(address & ~3, (address & ~3) + 3,
        'nse2_startup_messages', function(offset, data, mask)
            for lane = 0, 1 do
                local shift = (1 - lane) * 16
                if offset + lane * 2 == address and (mask & (0xffff << shift)) ~= 0 then
                    messages = messages + 1
                    if messages <= 256 then
                        machine:logerror(string.format('nse2_startup_message: address=%08x value=%04x pc=%08x t=%.9f observer_writes=0\n',
                            address, (data >> shift) & 0xffff, cpu.state['R15'].value, machine.time:as_double()))
                    elseif messages == 257 then
                        machine:logerror(string.format('nse2_startup_message_cap: reached=256 t=%.9f\n', machine.time:as_double()))
                    end
                end
            end
        end)
end)
_G.nse2_start_mode_tap = space:install_write_tap(0x10c5cc, 0x10c5cf, 'nse2_start_mode', function(_, data, mask)
    if (mask & 0xff000000) == 0 then return end
    machine:logerror(string.format('nse2_start_mode: value=%02x pc=%08x t=%.9f observer_writes=0\n',
        (data >> 24) & 255, cpu.state['R15'].value, machine.time:as_double()))
end)
_G.nse2_key_decode_tap = space:install_read_tap(0x2b382c, 0x2b3847, 'nse2_key_decode', function(offset, data, mask)
    if cpu.state['R15'].value ~= 0x299044 then return end
    for lane = 0, 3 do
        local shift = (3 - lane) * 8
        if (mask & (255 << shift)) ~= 0 then
            key_reads = key_reads + 1
            if key_reads <= 64 then
                machine:logerror(string.format('nse2_key_decode: raw=%d key=%02x pc=00299044 t=%.9f observer_writes=0\n',
                    offset + lane - 0x2b382c, (data >> shift) & 255, machine.time:as_double()))
            end
        end
    end
end)
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
            if dsp and address == 0x20033 then
                machine:logerror(string.format('nse2_key_mask: value=%02x pc=%08x t=%.9f observer_writes=0\n',
                    value, cpu.state['R15'].value, machine.time:as_double()))
            end
            if address == 0x20028 then selected = value end
            if address == 0x2002a then tx = tx + 1 end
            if selected == 0x21 and address == 0x2002c then
                commands = commands + 1
                if dsp and commands <= 128 then
                    machine:logerror(string.format('nse2_lcd_command: n=%d value=%02x pc=%08x t=%.9f observer_writes=0\n',
                        commands, value, cpu.state['R15'].value, machine.time:as_double()))
                end
            end
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
    if dsp then
        machine:logerror(string.format('nse2_native_state: t=%.9f pc=%04x illegal=%d idle=%d observer_writes=0\n',
            now, dsp.state['PC'].value, dsp.state['ILLEGAL'].value, dsp.state['IDLE'].value))
        if next_sample == 1 or next_sample == 7 or next_sample == 15 or next_sample == 30 then
            local values = {}
            for _, address in ipairs({0x852, 0x853, 0x854, 0x855, 0x8e4, 0x8e5, 0x866, 0x6bc, 0x6be, 0x6e3}) do
                values[#values + 1] = string.format('%04x:%04x:%d', address,
                    dsp.spaces.data:read_u16(address), native_ring_writes[address])
            end
            machine:logerror(string.format('nse2_native_transport: t=%.9f pc=%04x imr=%04x ifr=%04x words=%s mcu_packets=%04x observer_writes=0\n',
                now, dsp.state['PC'].value, dsp.state['IMR'].value, dsp.state['IFR'].value,
                table.concat(values, ','), space:read_u16(0x10c766)))
            machine:logerror(string.format('nse2_native_scan_state: t=%.9f count0285=%04x repeat0e13=%04x ar5=%04x ar6=%04x ar7=%04x observer_writes=0\n',
                now, dsp.spaces.data:read_u16(0x285), dsp.spaces.data:read_u16(0xe13),
                dsp.state['AR5'].value, dsp.state['AR6'].value, dsp.state['AR7'].value))
            machine:logerror(string.format('nse2_native_internal_timer: t=%.9f tim=%04x prd=%04x tcr=%04x observer_writes=0\n',
                now, dsp.state['TIM'].value, dsp.state['PRD'].value, dsp.state['TCR'].value))
            if next_sample == 30 then
                local sites = {}
                for pc in pairs(native_work_writers) do sites[#sites + 1] = pc end
                table.sort(sites)
                for _, pc in ipairs(sites) do
                    local entry = native_work_writers[pc]
                    machine:logerror(string.format('nse2_native_work_writer: pc=%04x writes=%d bit0_set=%d bits=%04x t=%.9f observer_writes=0\n',
                        pc, entry.count, entry.set, entry.bits, now))
                end
            end
        end
        machine:logerror(string.format('nse2_ui_state: t=%.9f state_10c664=%04x flags_10c5c9=%02x phase_10ffac=%02x observer_writes=0\n',
            now, space:read_u16(0x10c664), space:read_u8(0x10c5c9), space:read_u8(0x10ffac)))
    end
    machine:logerror(string.format('nse2_state: t=%.9f pc=%08x cpsr=%08x sp=%08x shared_writes=%d gensio_tx=%d gensio_rx=%d lcd_commands=%d lcd_bytes=%d eeprom_signal_writes=%d observer_writes=0\n',
        now, cpu.state['R15'].value, cpu.state['CPSR'].value, cpu.state['R13'].value,
        shared_writes, tx, rx, commands, pixels, signal_writes))
    if next_sample == 1 or next_sample == 7 then
        machine.screens[':screen']:snapshot('nse2_' .. next_sample .. '.png')
    end
    next_sample = next_sample + 1
end, 'NSE-2 passive first boot')

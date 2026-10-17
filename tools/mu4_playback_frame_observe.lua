-- Passive RAM/bus evidence. GENPC selects context, not instruction ownership.
local machine = manager.machine
assert(machine.system.name == 'mu4nand', 'frame observation requires mu4nand')
local cpu = assert(machine.devices[':cpu'])
local data = assert(cpu.spaces.data)
local handles, reads, copies, snapshots, outputs = {}, 0, 0, 0, 0
local inside, closed = false, false
local function snapshot(name, base, count)
    inside = true
    local file = assert(io.open(name, 'wb'))
    for address = base, base + count - 1 do
        file:write(string.pack('>I2', data:read_u16(address)))
    end
    file:close()
    inside = false
    machine:logerror(string.format('mu4_frame_snapshot: file=%s base=%04x words=%u t=%.9f context_only=1 observer_writes=0\n',
        name, base, count, machine.time:as_double()))
end
handles[1] = data:install_read_tap(0x162e, 0x1e2d, 'mu4_frame_bit_reads', function(address, value, mask)
    if inside or machine:side_effects_disabled() then return end
    local time, pc = machine.time:as_double(), cpu.state.GENPC.value
    if time < 0.99 or time > 1.25 or pc < 0x2510c or pc > 0x25143 then return end
    reads = reads + 1
    if reads <= 4096 then
        machine:logerror(string.format('mu4_frame_bit_read: ordinal=%u address=%04x word=%04x mask=%04x t=%.9f context_pc=%06x ownership=data_bus observer_writes=0\n',
            reads, address, value, mask, time, pc))
    end
    if snapshots == 0 then
        snapshots = 1
        snapshot('mu4_frame_initial_buffer.be16', 0x162e, 0x800)
    end
end)
handles[2] = data:install_write_tap(0x1000, 0x137f, 'mu4_frame_copy_context', function(address, value, mask)
    if inside or machine:side_effects_disabled() then return end
    local time, pc = machine.time:as_double(), cpu.state.GENPC.value
    if time < 1.0 or time > 1.25 or pc ~= 0x2a367 then return end
    local object = cpu.state.AR4.value
    if object < 0x60 or object > 0xfff2 or copies >= 512 then return end
    local offset = data:read_u16(object + 12)
    local destination = data:read_u16(object + 10)
    local count = cpu.state.AR1.value
    -- The source advances both object cursors before entering MVDD.
    if address ~= ((destination + offset - count) & 0xffff) then return end
    copies = copies + 1
    local fields = {}
    for i = 0, 13 do fields[#fields + 1] = string.format('%04x', data:read_u16(object + i)) end
    machine:logerror(string.format('mu4_frame_copy: ordinal=%u address=%04x word=%04x mask=%04x object=%04x count=%04x fields=%s t=%.9f context_pc=%06x pointer_ownership=unvalidated observer_writes=0\n',
        copies, address, value, mask, object, count, table.concat(fields, ','), time, pc))
    local source, extent, cursor = data:read_u16(object + 6), data:read_u16(object + 7), data:read_u16(object + 8)
    if cursor == count and source >= 0x60 and source + extent <= 0x10000 and extent <= 0x800 and outputs < 24 then
        outputs = outputs + 1
        snapshot(string.format('mu4_frame_output_%02u.be16', outputs), source, extent)
    end
end)
emu.register_frame_done(function()
    if not closed and machine.time:as_double() > 1.25 then
        closed = true
        handles[1]:remove()
        handles[2]:remove()
    end
end)
handles[3] = emu.add_machine_stop_notifier(function()
    local capped = reads > 4096 or copies >= 512 or outputs >= 24
    machine:logerror(string.format('mu4_frame_terminal: reads=%u copies=%u snapshots=%u outputs=%u closed=%u capped=%u t=%.9f observer_writes=0 audio_acceptance=0\n',
        reads, copies, snapshots, outputs, closed and 1 or 0, capped and 1 or 0, machine.time:as_double()))
end)
_G.mu4_frame_observation_handles = handles

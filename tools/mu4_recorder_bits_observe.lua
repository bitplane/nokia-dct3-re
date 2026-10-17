-- Bounded low-level bit-writer observation; no guest writes or codec claim.
local machine = manager.machine
assert(machine.system.name == 'mu4nand', 'recorder bit observation requires mu4nand')
local cpu = assert(machine.devices[':cpu'])
local data = assert(cpu.spaces.data)
local sampling, closed, samples = false, false, 0
local pending = {}
local parent_handles, parent_counts = {}, {}
local function parent_access(kind, address, value)
    local pc, time = cpu.state.GENPC.value, machine.time:as_double()
    if sampling or machine:side_effects_disabled() or closed or time < 0.8 or time > 1.145 or
       pc < 0x3bd93 or pc > 0x3bdd9 or (parent_counts[kind] or 0) >= 512 then return end
    parent_counts[kind] = (parent_counts[kind] or 0) + 1
    sampling = true
    local sp = cpu.state.SP.value
    local threshold, length = data:read_u16((sp+6)&0xffff), data:read_u16((sp+8)&0xffff)
    sampling = false
    machine:logerror(string.format(
        'mu4_record_escape_access: kind=%s address=%04x value=%04x ordinal=%u sp=%04x threshold=%04x length=%04x a=%010x b=%010x st1=%04x t=%.9f context_pc=%06x boundary=data_bus ownership=unvalidated observer_writes=0\n',
        kind, address, value, parent_counts[kind], sp, threshold, length,
        cpu.state.A.value & 0xffffffffff, cpu.state.B.value & 0xffffffffff, cpu.state.ST1.value, time, pc))
end
parent_handles[1] = data:install_read_tap(8, 0x0b, 'mu4_record_escape_mapped', function(address,value) parent_access('read',address,value) end)
parent_handles[2] = data:install_read_tap(0x6000, 0x7fff, 'mu4_record_escape_stack_read', function(address,value) parent_access('read',address,value) end)
parent_handles[3] = data:install_write_tap(0x6000, 0x7fff, 'mu4_record_escape_stack_write', function(address,value) parent_access('write',address,value) end)
local tap = data:install_read_tap(0x6000, 0x7fff, 'mu4_recorder_bit_arguments', function(address, value)
    local pc, time = cpu.state.GENPC.value, machine.time:as_double()
    if sampling or machine:side_effects_disabled() or closed or time < 0.8 or time > 1.145 or
       samples >= 2048 or pc < 0x3ac17 or pc > 0x3ac1f then return end
    local sp = cpu.state.SP.value
    if address == ((sp + 10) & 0xffff) then
        pending[sp] = {value = value, time = time}
        return
    end
    if address ~= ((sp + 11) & 0xffff) then return end
    local entry = pending[sp]
    if not entry then
        machine:logerror('mu4_record_bits_unpaired: observer_writes=0\n')
        return
    end
    pending[sp] = nil
    sampling = true
    local page, caller = data:read_u16((sp + 8) & 0xffff), data:read_u16((sp + 9) & 0xffff)
    local receiver = cpu.state.AR1.value
    -- AR1's prologue write can still be pending at the earlier value read.
    -- Sample it at the later width read, and keep both real read values.
    local parent_page, parent = 0, 0
    if caller == 0xacdd or caller == 0xace5 or caller == 0xacee then
        parent_page, parent = data:read_u16((sp + 16) & 0xffff), data:read_u16((sp + 17) & 0xffff)
    end
    local fields = {}
    if receiver >= 0x80 and receiver <= 0xfff9 then
        for offset = 0, 6 do fields[#fields + 1] = string.format('%04x', data:read_u16(receiver + offset)) end
    end
    sampling = false
    samples = samples + 1
    machine:logerror(string.format(
        'mu4_record_bits_input: ordinal=%u value=%04x width=%u receiver=%04x fields=%s caller=%02x%04x parent=%02x%04x sp=%04x value_t=%.9f t=%.9f context_pc=%06x boundary=paired_data_reads ownership=unvalidated observer_writes=0\n',
        samples, entry.value, value, receiver, #fields > 0 and table.concat(fields, ',') or 'unobserved',
        page, caller, parent_page, parent, sp, entry.time, time, pc))
end)
emu.register_frame_done(function()
    if not closed and machine.time:as_double() > 1.145 then
        tap:remove()
        for _, handle in ipairs(parent_handles) do handle:remove() end
        closed = true
        machine:logerror(string.format('mu4_record_bits_closed: samples=%u capped=%u t=%.9f observer_writes=0\n',
            samples, samples == 2048 and 1 or 0, machine.time:as_double()))
    end
end)
_G.mu4_record_bits_tap = tap
_G.mu4_record_escape_taps = parent_handles

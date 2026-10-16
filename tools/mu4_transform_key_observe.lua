-- Read-only initializer context for the original AA88/AA99 transforms.
local machine = manager.machine
assert(machine.system.name == 'mu4nand', 'MU4 key context requires mu4nand')
local cpu = assert(machine.devices[':cpu'])
local data = assert(cpu.spaces.data)
local handles, sampling = {}, false
local writers = {}
local caller_reads = {}
local function observe_caller_read(address, value)
    local pc = cpu.state.GENPC.value
    local time = machine.time:as_double()
    local site = pc >= 0x2ab07 and pc <= 0x2ab45 and 'copy_caller' or
                 pc >= 0x2b25c and pc <= 0x2b26a and 'initializer_wrapper' or nil
    if sampling or machine:side_effects_disabled() or time < 0.8182 or time > 1.7 or not site or (caller_reads[site] or 0) >= 32 then return end
    caller_reads[site] = (caller_reads[site] or 0) + 1
    machine:logerror(string.format(
        'mu4_transform_caller_read: site=%s address=%04x value=%04x ordinal=%u t=%.9f context_pc=%06x reader_identity=unvalidated observer_writes=0\n',
        site, address, value, caller_reads[site], time, pc))
end
handles[#handles + 1] = data:install_read_tap(0xd700, 0xd7ff, 'mu4_transform_caller_stack', observe_caller_read)
handles[#handles + 1] = data:install_read_tap(0xe3b0, 0xe41f, 'mu4_transform_caller_object', observe_caller_read)
handles[#handles + 1] = data:install_read_tap(0xa500, 0xa53f, 'mu4_transform_caller_source', observe_caller_read)
local copy_reads = 0
handles[#handles + 1] = data:install_read_tap(0xd700, 0xd7ff, 'mu4_copy_argument_reads', function(address, value)
    local pc = cpu.state.GENPC.value
    if sampling or machine:side_effects_disabled() or copy_reads >= 16 or pc < 0x2d01a or pc > 0x2d01f then return end
    copy_reads = copy_reads + 1
    machine:logerror(string.format(
        'mu4_transform_copy_argument_read: address=%04x value=%04x ordinal=%u t=%.9f context_pc=%06x reader_identity=unvalidated observer_writes=0\n',
        address, value, copy_reads, machine.time:as_double(), pc))
end)
-- Allocations move when CPU arithmetic changes; observe the bounded heap
-- neighborhood, then qualify addresses against the captured caller pointer.
for _, range in ipairs({{0xff60, 0xffbf}, {0xe3b0, 0xe3ff}}) do
    local reads = 0
    handles[#handles + 1] = data:install_read_tap(range[1], range[2], 'mu4_key_buffer_reads_' .. range[1],
        function(address, value)
            local pc = cpu.state.GENPC.value
            if sampling or machine:side_effects_disabled() or reads >= 36 or
               not ((pc >= 0x3f7cf and pc <= 0x3f80d) or (pc >= 0x2cd14 and pc <= 0x2cd52)) then return end
            reads = reads + 1
            machine:logerror(string.format(
                'mu4_transform_key_read: address=%04x value=%04x ordinal=%u t=%.9f context_pc=%06x reader_identity=unvalidated observer_writes=0\n',
                address, value, reads, machine.time:as_double(), pc))
        end)
end
for _, range in ipairs({{0xff60, 0xffbf}, {0xe3b0, 0xe3ff},
                       {0xd760, 0xd760}, {0xe405, 0xe405}, {0xa513, 0xa513}, {0xe3db, 0xe3db}}) do
    handles[#handles + 1] = data:install_write_tap(range[1], range[2], 'mu4_key_buffer_' .. range[1],
        function(address, value, mask)
            if sampling or machine:side_effects_disabled() then return end
            local previous = writers[address]
            writers[address] = {value = value, mask = mask, pc = cpu.state.GENPC.value,
                                time = machine.time:as_double(), count = previous and previous.count + 1 or 1,
                                prior_value = previous and previous.value, prior_pc = previous and previous.pc}
        end)
end
local specs = {
    {name = 'aa88', base = 0xfc6c, first = 0x3f7cf, last = 0x3f80d},
    {name = 'aa99', base = 0xb9de, first = 0x2cd14, last = 0x2cd52},
}
local repeat_samples = 0
handles[#handles + 1] = data:install_write_tap(0x1a, 0x1a, 'mu4_key_copy_repeat', function(_, value, mask)
    local pc = cpu.state.GENPC.value
    if sampling or machine:side_effects_disabled() or repeat_samples >= 8 or
       not ((pc >= 0x3e438 and pc <= 0x3e448) or (pc >= 0x2d01a and pc <= 0x2d02a)) then return end
    repeat_samples = repeat_samples + 1
    sampling = true
    local sp = cpu.state.SP.value
    local caller = data:read_u16(sp) * 0x10000 + data:read_u16((sp + 1) & 0xffff)
    local source, count = data:read_u16((sp + 2) & 0xffff), data:read_u16((sp + 3) & 0xffff)
    sampling = false
    machine:logerror(string.format(
        'mu4_transform_copy_repeat: value=%04x mask=%04x ordinal=%u sp=%04x frame_return=%06x frame_source=%04x frame_count=%04x ar3=%04x t=%.9f context_pc=%06x ownership=unvalidated observer_writes=0\n',
        value, mask, repeat_samples, sp, caller, source, count, cpu.state.AR3.value, machine.time:as_double(), pc))
    if source == 0xe3e2 and (count == 6 or count == 7) then
        for address = source, source + count - 1 do
            local writer = writers[address]
            if writer then
                machine:logerror(string.format(
                    'mu4_transform_copy_source_writer: address=%04x value=%04x count=%u prior_value=%s prior_pc=%s t=%.9f context_pc=%06x ownership=unvalidated observer_writes=0\n',
                    address, writer.value, writer.count,
                    writer.prior_value and string.format('%04x', writer.prior_value) or 'unobserved',
                    writer.prior_pc and string.format('%06x', writer.prior_pc) or 'unobserved', writer.time, writer.pc))
            else
                machine:logerror(string.format('mu4_transform_copy_source_unobserved: address=%04x scope=installed_tap_epoch observer_writes=0\n', address))
            end
        end
    end
end)
for _, spec in ipairs(specs) do
    handles[#handles + 1] = data:install_write_tap(spec.base, spec.base, 'mu4_key_context_' .. spec.name,
        function(_, value, mask)
            local pc = cpu.state.GENPC.value
            if sampling or machine:side_effects_disabled() or spec.seen or pc < spec.first or pc > spec.last then return end
            spec.seen, sampling = true, true
            local sp = cpu.state.SP.value
            -- Recovered initializer locals and caller frame, not a CPU operand API.
            local pointer, length = data:read_u16((sp + 2) & 0xffff), data:read_u16((sp + 14) & 0xffff)
            local words = {}
            local count = 0
            if pointer >= 0x80 and length > 0 and length <= 256 and pointer + length <= 0x10000 then
                count = math.min(length, 64)
                for offset = 0, count - 1 do words[#words + 1] = string.format('%04x', data:read_u16(pointer + offset)) end
            end
            sampling = false
            machine:logerror(string.format(
                'mu4_transform_key_context: segment=%s pointer=%04x length=%04x captured=%u words=%s state_base=%04x first_write=%04x mask=%04x sp=%04x t=%.9f context_pc=%06x ownership=unvalidated observer_writes=0\n',
                spec.name, pointer, length, count, count > 0 and table.concat(words, ',') or 'unobserved',
                spec.base, value, mask, sp, machine.time:as_double(), pc))
            if spec.name == 'aa99' then
                for _, field in ipairs({{0xd760, 'caller_slot'}, {0xe405, 'copy_count_field'},
                                        {0xa513, 'source_word3'}, {0xe3db, 'copied_word3'}}) do
                    local writer = writers[field[1]]
                    if writer then
                        machine:logerror(string.format(
                            'mu4_transform_length_writer: role=%s address=%04x value=%04x count=%u t=%.9f context_pc=%06x ownership=unvalidated observer_writes=0\n',
                            field[2], field[1], writer.value, writer.count, writer.time, writer.pc))
                    else
                        machine:logerror(string.format('mu4_transform_length_unobserved: role=%s address=%04x scope=installed_tap_epoch observer_writes=0\n', field[2], field[1]))
                    end
                end
            end
            for offset = 0, count - 1 do
                local address = pointer + offset
                local writer = writers[address]
                if writer then
                    machine:logerror(string.format(
                        'mu4_transform_key_preceding_write: segment=%s address=%04x value=%04x mask=%04x count=%u t=%.9f context_pc=%06x ownership=unvalidated observer_writes=0\n',
                        spec.name, address, writer.value, writer.mask, writer.count, writer.time, writer.pc))
                else
                    machine:logerror(string.format(
                        'mu4_transform_key_preceding_unobserved: segment=%s address=%04x scope=installed_tap_epoch ownership=unvalidated observer_writes=0\n', spec.name, address))
                end
            end
        end)
end
local closed = false
emu.register_frame_done(function()
    if not closed and machine.time:as_double() > 1.8 then
        for _, tap in ipairs(handles) do tap:remove() end
        closed = true
        machine:logerror(string.format('mu4_transform_key_taps_closed: count=%u t=%.9f observer_writes=0\n', #handles, machine.time:as_double()))
    end
end)
_G.mu4_transform_key_taps = handles

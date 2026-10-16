-- Read-only playback bus/context observations; no instruction ownership or audio claim.
local machine = manager.machine
assert(machine.system.name == 'mu4nand', 'MU4 playback context requires mu4nand')
local cpu = assert(machine.devices[':cpu'])
local data = assert(cpu.spaces.data)
local handles, read_counts, latch_writes, sampling = {}, {}, 0, false
local gates = {}
for _, direction in ipairs({'read', 'write'}) do
    for _, address in ipairs({0xba4f, 0xba51}) do
        local state = {accesses = 0, samples = 0}
        gates[direction .. address] = state
        local install = direction == 'read' and data.install_read_tap or data.install_write_tap
        handles[#handles + 1] = install(data, address, address, 'mu4_playback_gate_' .. direction .. address,
            function(_, value)
                local time = machine.time:as_double()
                if sampling or machine:side_effects_disabled() or time < 0.8182 or time > 1.7 then return end
                state.accesses = state.accesses + 1
                if state.samples < 32 and (state.accesses <= 8 or state.last ~= value) then
                    state.samples = state.samples + 1
                    machine:logerror(string.format(
                        'mu4_playback_gate_access: direction=%s address=%04x value=%04x ordinal=%u access=%u t=%.9f context_pc=%06x ownership=unvalidated observer_writes=0\n',
                        direction, address, value, state.samples, state.accesses, time, cpu.state.GENPC.value))
                end
                state.last = value
            end)
    end
end
local gate_stop = emu.add_machine_stop_notifier(function()
    for _, direction in ipairs({'read', 'write'}) do
        for _, address in ipairs({0xba4f, 0xba51}) do
            local state = gates[direction .. address]
            machine:logerror(string.format(
                'mu4_playback_gate_terminal: direction=%s address=%04x accesses=%u samples=%u last=%s selection=common_first_session_window observer_writes=0 audio_acceptance=0\n',
                direction, address, state.accesses, state.samples,
                state.last and string.format('%04x', state.last) or 'unobserved'))
        end
    end
end)
_G.mu4_playback_gate_stop = gate_stop
local queue_counts = {}
local result_counts = {}
local service_counts = {}
local published, post_flag_captured = false, false
local context_writes = {}
handles[#handles + 1] = data:install_write_tap(0xac36, 0xac38, 'mu4_playback_flag_context', function(address, value, mask)
    local time = machine.time:as_double()
    if sampling or machine:side_effects_disabled() or time < 0.8181 or time > 1.7 then return end
    local count = (context_writes[address] or 0) + 1
    context_writes[address] = count
    if count <= 16 then
        machine:logerror(string.format(
            'mu4_playback_flag_context_write: address=%04x value=%04x mask=%04x ordinal=%u t=%.9f context_pc=%06x ownership=unvalidated observer_writes=0\n',
            address, value, mask, count, time, cpu.state.GENPC.value))
    end
end)
handles[#handles + 1] = data:install_write_tap(0x15d9, 0x15d9, 'mu4_playback_publication_selection', function(_, value)
    local pc, time = cpu.state.GENPC.value, machine.time:as_double()
    if not sampling and not machine:side_effects_disabled() and time >= 0.8182 and time <= 1.7 and
       value == 0 and pc >= 0x293e1 and pc <= 0x293e5 then published = true end
end)
handles[#handles + 1] = data:install_write_tap(0xba66, 0xba66, 'mu4_playback_post_publication_flag', function(_, value)
    local pc, time = cpu.state.GENPC.value, machine.time:as_double()
    if sampling or machine:side_effects_disabled() or not published or post_flag_captured or
       time > 1.7 or pc < 0x25331 or pc > 0x25335 then return end
    post_flag_captured, sampling = true, true
    local pointer = cpu.state.AR1.value
    local remaining, high, low = data:read_u16(0xa854), data:read_u16(0xa858), data:read_u16(0xa859)
    local index, base = data:read_u16(0xb331), data:read_u16(0xb332)
    local window = {}
    for offset = -2, 1 do window[#window + 1] = string.format('%04x', data:read_u16((base + index + offset) & 0xffff)) end
    sampling = false
    machine:logerror(string.format(
        'mu4_playback_post_flag: value=%04x remaining=%04x reservoir=%04x%04x index=%04x base=%04x window=%s ar1=%04x t=%.9f context_pc=%06x ownership=unvalidated observer_writes=0\n',
        value, remaining, high, low, index, base, table.concat(window, ','), pointer, time, pc))
    -- The live pointer selects a candidate address, not retained operand ownership.
    -- Log actual accesses separately; never promote the snapshot to a bus read.
    local reads = 0
    handles[#handles + 1] = data:install_read_tap(pointer, pointer, 'mu4_playback_post_flag_pointer', function(address, word)
        local context = cpu.state.GENPC.value
        if sampling or machine:side_effects_disabled() or context < 0x25331 or context > 0x2533f or reads >= 4 then return end
        reads = reads + 1
        machine:logerror(string.format(
            'mu4_playback_post_flag_read: address=%04x value=%04x ordinal=%u t=%.9f context_pc=%06x reader_identity=unvalidated observer_writes=0\n',
            address, word, reads, machine.time:as_double(), context))
    end)
end)
for _, range in ipairs({{0xaa54, 0xaa62}, {0x15d9, 0x15d9}, {0x15ef, 0x15f8}, {0xd796, 0xd7af}, {0x13c0, 0x147f}}) do
    handles[#handles + 1] = data:install_read_tap(range[1], range[2], 'mu4_playback_service_reads_' .. range[1],
        function(address, value)
            local pc, time = cpu.state.GENPC.value, machine.time:as_double()
            if sampling or machine:side_effects_disabled() or time < 0.8182 or time > 1.7 or
               not ((pc >= 0x20495 and pc <= 0x205ff) or (pc >= 0x298b8 and pc <= 0x298cf)) then return end
            local count = (service_counts[address] or 0) + 1
            service_counts[address] = count
            if count <= 16 then
                machine:logerror(string.format(
                    'mu4_playback_service_read: address=%04x value=%04x ordinal=%u t=%.9f context_pc=%06x reader_identity=unvalidated observer_writes=0\n',
                    address, value, count, time, pc))
            end
        end)
end
for _, address in ipairs({0x15d9, 0xd79c, 0xd7ac}) do
    local count = 0
    handles[#handles + 1] = data:install_write_tap(address, address, 'mu4_playback_service_counter_' .. address,
        function(_, value, mask)
            local time = machine.time:as_double()
            -- These addresses are reused before AA99; do not spend this
            -- session's cap on bootstrap-stack aliases.
            if sampling or machine:side_effects_disabled() or time < 0.8181 or time > 1.7 then return end
            count = count + 1
            if count <= 16 then
                machine:logerror(string.format(
                    'mu4_playback_service_counter_write: address=%04x value=%04x mask=%04x ordinal=%u t=%.9f context_pc=%06x writer_identity=unvalidated observer_writes=0\n',
                    address, value, mask, count, time, cpu.state.GENPC.value))
            end
        end)
end
local parser_writes = {}
local prefix_writes = {}
local merge_reads = 0
local merge_writes = {}
local transform_state_captured = false
local transform_reads = 0
local nibble_reads = 0
handles[#handles + 1] = data:install_read_tap(0xd700, 0xd77f, 'mu4_playback_transform_nibbles', function(address, value)
    local time = machine.time:as_double()
    local pc = cpu.state.GENPC.value
    if sampling or machine:side_effects_disabled() or time < 0.8182 or time > 1.7 or pc < 0x2c62b or pc > 0x2c642 then return end
    nibble_reads = nibble_reads + 1
    if nibble_reads <= 64 then
        machine:logerror(string.format(
            'mu4_playback_nibble_read: address=%04x value=%04x ordinal=%u t=%.9f context_pc=%06x reader_identity=unvalidated observer_writes=0\n',
            address, value, nibble_reads, time, pc))
    end
end)
handles[#handles + 1] = data:install_read_tap(0xb9de, 0xba2f, 'mu4_playback_transform_state', function(address, value)
    local time = machine.time:as_double()
    local pc = cpu.state.GENPC.value
    if sampling or machine:side_effects_disabled() or time < 0.8182 or time > 1.7 or pc < 0x2c9ac or pc > 0x2cd13 then return end
    transform_reads = transform_reads + 1
    if transform_reads <= 96 then
        machine:logerror(string.format(
            'mu4_playback_transform_read: address=%04x value=%04x ordinal=%u t=%.9f context_pc=%06x reader_identity=unvalidated observer_writes=0\n',
            address, value, transform_reads, time, pc))
    end
    if transform_state_captured then return end
    transform_state_captured, sampling = true, true
    local state, input, stack = {}, {}, {}
    local sp = cpu.state.SP.value
    for address = 0xb9de, 0xba2f do state[#state + 1] = string.format('%04x', data:read_u16(address)) end
    for address = 0xaa24, 0xaa43 do input[#input + 1] = string.format('%04x', data:read_u16(address)) end
    for offset = 0, 23 do stack[#stack + 1] = string.format('%04x', data:read_u16((sp + offset) & 0xffff)) end
    sampling = false
    machine:logerror(string.format(
        'mu4_playback_transform_snapshot: state_base=b9de state=%s input_base=aa24 input=%s sp=%04x stack=%s t=%.9f context_pc=%06x ownership=unvalidated observer_writes=0\n',
        table.concat(state, ','), table.concat(input, ','), sp, table.concat(stack, ','), time, pc))
end)
handles[#handles + 1] = data:install_write_tap(0xaa24, 0xaa27, 'mu4_playback_merge_input', function(address, value, mask)
    local time = machine.time:as_double()
    if sampling or machine:side_effects_disabled() or time < 0.8182 or time > 1.7 then return end
    merge_writes[address] = (merge_writes[address] or 0) + 1
    if merge_writes[address] <= 32 then
        machine:logerror(string.format(
            'mu4_playback_merge_write: address=%04x value=%04x mask=%04x ordinal=%u t=%.9f context_pc=%06x writer_identity=unvalidated observer_writes=0\n',
            address, value, mask, merge_writes[address], time, cpu.state.GENPC.value))
    end
end)
handles[#handles + 1] = data:install_read_tap(0xaa24, 0xaa63, 'mu4_playback_prefix_merge', function(address, value)
    local time = machine.time:as_double()
    local pc = cpu.state.GENPC.value
    if sampling or machine:side_effects_disabled() or time < 0.8182 or time > 1.7 or pc < 0x29e70 or pc > 0x29e7c then return end
    merge_reads = merge_reads + 1
    if merge_reads <= 32 then
        machine:logerror(string.format(
            'mu4_playback_merge_read: address=%04x value=%04x ordinal=%u t=%.9f context_pc=%06x reader_identity=unvalidated observer_writes=0\n',
            address, value, merge_reads, time, pc))
    end
end)
handles[#handles + 1] = data:install_write_tap(0x162e, 0x1631, 'mu4_playback_buffer_prefix', function(address, value, mask)
    local time = machine.time:as_double()
    if sampling or machine:side_effects_disabled() or time < 0.8182 or time > 1.7 then return end
    prefix_writes[address] = (prefix_writes[address] or 0) + 1
    if prefix_writes[address] <= 32 then
        machine:logerror(string.format(
            'mu4_playback_prefix_write: address=%04x value=%04x mask=%04x ordinal=%u t=%.9f context_pc=%06x writer_identity=unvalidated observer_writes=0\n',
            address, value, mask, prefix_writes[address], time, cpu.state.GENPC.value))
    end
end)
handles[#handles + 1] = data:install_write_tap(0xa888, 0xa88c, 'mu4_playback_parser_fields', function(address, value, mask)
    local time = machine.time:as_double()
    if sampling or machine:side_effects_disabled() or time < 0.8182 or time > 1.7 then return end
    parser_writes[address] = (parser_writes[address] or 0) + 1
    if parser_writes[address] <= 32 then
        machine:logerror(string.format(
            'mu4_playback_parser_write: address=%04x value=%04x mask=%04x ordinal=%u t=%.9f context_pc=%06x writer_identity=unvalidated observer_writes=0\n',
            address, value, mask, parser_writes[address], time, cpu.state.GENPC.value))
    end
end)
handles[#handles + 1] = data:install_write_tap(0x15ee, 0x15f8, 'mu4_playback_method_result', function(address, value, mask)
    local time = machine.time:as_double()
    if time < 0.8182 or time > 1.7 then return end
    result_counts[address] = (result_counts[address] or 0) + 1
    if result_counts[address] <= 8 then
        machine:logerror(string.format(
            'mu4_playback_result_write: address=%04x value=%04x mask=%04x ordinal=%u t=%.9f context_pc=%06x writer_identity=unvalidated observer_writes=0\n',
            address, value, mask, result_counts[address], time, cpu.state.GENPC.value))
    end
end)
for _, range in ipairs({{0x8044, 0x8049}, {0x80a3, 0x80a5}}) do
    handles[#handles + 1] = data:install_write_tap(range[1], range[2], 'mu4_playback_queue_' .. range[1],
        function(address, value, mask)
            local time = machine.time:as_double()
            -- Common interval within the independently verified first sessions of both files.
            if time < 0.8182 or time > 1.7 then return end
            queue_counts[address] = (queue_counts[address] or 0) + 1
            if queue_counts[address] <= 32 then
                machine:logerror(string.format(
                    'mu4_playback_queue_write: address=%04x value=%04x mask=%04x ordinal=%u t=%.9f context_pc=%06x writer_identity=unvalidated observer_writes=0\n',
                    address, value, mask, queue_counts[address], time, cpu.state.GENPC.value))
            end
        end)
end
handles[#handles + 1] = data:install_read_tap(0x804a, 0x804f, 'mu4_playback_descriptor_reads', function(address, value)
    local time = machine.time:as_double()
    if sampling or machine:side_effects_disabled() or time < 0.8 or time > 2.4 then return end
    read_counts[address] = (read_counts[address] or 0) + 1
    if read_counts[address] <= 8 then
        machine:logerror(string.format(
            'mu4_playback_descriptor_read: address=%04x value=%04x ordinal=%u t=%.9f context_pc=%06x reader_identity=unvalidated observer_writes=0\n',
            address, value, read_counts[address], time, cpu.state.GENPC.value))
    end
end)
handles[#handles + 1] = data:install_write_tap(0xaa56, 0xaa56, 'mu4_playback_processing_latch', function(_, value, mask)
    local time = machine.time:as_double()
    if time < 0.8 or time > 2.4 then return end
    latch_writes = latch_writes + 1
    if latch_writes <= 16 then
        machine:logerror(string.format(
            'mu4_playback_latch_write: address=aa56 value=%04x mask=%04x ordinal=%u t=%.9f context_pc=%06x writer_identity=unvalidated observer_writes=0\n',
            value, mask, latch_writes, time, cpu.state.GENPC.value))
    end
end)
_G.mu4_playback_control_taps = handles
local callback_reads = {}
handles[#handles + 1] = data:install_read_tap(0xd7b0, 0xd7b5, 'mu4_playback_callback_descriptor', function(address, value)
    local time = machine.time:as_double()
    if sampling or machine:side_effects_disabled() or time < 0.8182 or time > 1.7 then return end
    callback_reads[address] = (callback_reads[address] or 0) + 1
    if callback_reads[address] <= 8 then
        machine:logerror(string.format(
            'mu4_playback_callback_descriptor_read: address=%04x value=%04x ordinal=%u t=%.9f context_pc=%06x reader_identity=unvalidated observer_writes=0\n',
            address, value, callback_reads[address], time, cpu.state.GENPC.value))
    end
end)
local times, next_sample = {0.9, 1.1, 1.4}, 1
local taps_closed = false
local function words(pointer)
    if pointer < 0x80 or pointer > 0xffe0 then return 'unobserved' end
    local values = {}
    for offset = 0, 31 do values[#values + 1] = string.format('%04x', data:read_u16(pointer + offset)) end
    return table.concat(values, ',')
end
emu.register_frame_done(function()
    local time = machine.time:as_double()
    -- Filtering inside a Lua tap still charges every hot-stack bus access.
    -- All observation windows end by 2.4 seconds; detach instead of lingering.
    if not taps_closed and time > 2.4 then
        for _, tap in ipairs(handles) do tap:remove() end
        taps_closed = true
        machine:logerror(string.format('mu4_playback_taps_closed: count=%u t=%.9f observer_writes=0\n', #handles, time))
    end
    if next_sample > #times or time < times[next_sample] then return end
    sampling = true
    local receiver = data:read_u16(0xaa59)
    machine:logerror(string.format(
        'mu4_playback_descriptor_snapshot: ordinal=%u address=804a words=%s t=%.9f context_only=1 observer_writes=0 audio_acceptance=0\n',
        next_sample, words(0x804a), time))
    machine:logerror(string.format(
        'mu4_playback_control_snapshot: ordinal=%u receiver=%04x words=%s aa54=%04x aa55=%04x aa56=%04x aa5a=%04x aa5b=%04x aa5c=%04x aa5d=%04x t=%.9f context_only=1 observer_writes=0 audio_acceptance=0\n',
        next_sample, receiver, words(receiver), data:read_u16(0xaa54), data:read_u16(0xaa55),
        data:read_u16(0xaa56), data:read_u16(0xaa5a), data:read_u16(0xaa5b),
        data:read_u16(0xaa5c), data:read_u16(0xaa5d), time))
    next_sample = next_sample + 1
    sampling = false
end)

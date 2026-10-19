-- Diagnostic mathematical port-input stimuli, not a GSM RF model.
local machine = manager.machine
local cpu = assert(machine.devices[":dsp_c54x:cpu"])
machine:logerror('rom4_rf_profile: system=' .. machine.system.name .. '\n')
local memory = cpu.spaces["data"]
local mode = os.getenv("ROM4_RF_SCORE_MODE") or "zero"
assert(mode == "zero" or mode == "dc" or mode == "tone")
local reads, results, nonzero, second, accepted = 0, 0, 0, 0, 0
local scan_reads = 0
local maximum = 0
local observing = false
local publication_counts = {}
local flag_writes = 0
local counter_min, counter_max = 0xffff, 0
local counter_writers = {}
local mode_writers = {}
local message_counts = {}
local search_requests = 0
local taps = {}
local port3 = cpu.spaces["io"]:read_u16(3)
local port3_writes, port3_transitions, reads_on, reads_off = 0, 0, 0, 0
local window_modes = {}
local window_states, last_clear = {}, nil
taps[#taps + 1] = cpu.spaces["io"]:install_write_tap(3, 3, "rf_window_control", function(_, value, mask)
    local next_value = (port3 & ~mask) | (value & mask)
    port3_writes = port3_writes + 1
    if (port3 & 1) ~= (next_value & 1) then
        port3_transitions = port3_transitions + 1
        if (next_value & 1) == 0 then last_clear = machine.time:as_double() end
    end
    if port3_writes <= 8 or ((port3 & 1) ~= (next_value & 1) and port3_transitions <= 32) then
        machine:logerror(string.format("rom4_rf_window_write: t=%.9f pc=%04x old=%04x value=%04x mask=%04x\n",
            machine.time:as_double(), cpu.state["PC"].value, port3, next_value, mask))
    end
    port3 = next_value
end)
local mcu = assert(machine.devices[":maincpu"])
-- The header store follows the literal load at 26be48; taps report its owner PC.
assert(mcu.spaces["program"]:read_u16(0x26be4a) == 0x8020,
    "RF header STRH owner differs from the checked NSE-1 image")
local mcu_ring_writes = 0
local post_fetches = 0
local consumer_case_observations = 0
local consumer_stage_counts = {}
taps[#taps + 1] = mcu.spaces["program"]:install_read_tap(0x211e80, 0x211eff,
    "rf_task10_consumer_case", function(address)
        local pc = mcu.state["PC"].value
        if pc ~= 0x211eb0 then
            if pc ~= 0x211eb8 and pc ~= 0x211ecc and pc ~= 0x211ed6 then return end
            if address ~= (pc & ~3) then return end
            consumer_stage_counts[pc] = (consumer_stage_counts[pc] or 0) + 1
            if consumer_stage_counts[pc] > 16 then return end
            local space = mcu.spaces["program"]
            local object = space:read_u32(mcu.state["R13"].value + 0x18)
            machine:logerror(string.format(
                "rom4_rf_task10_stage: pc=%08x object=%08x status=%04x control=%08x r1=%08x t=%.9f\n",
                pc, object, space:read_u16(object),
                space:read_u32(mcu.state["R4"].value + 0x7c),
                mcu.state["R1"].value, machine.time:as_double()))
            return
        end
        if address ~= 0x211eb0 then return end
        if consumer_case_observations >= 16 then return end
        consumer_case_observations = consumer_case_observations + 1
        local space = mcu.spaces["program"]
        local object = space:read_u32(mcu.state["R13"].value + 0x18)
        machine:logerror(string.format(
            "rom4_rf_task10_consumer: pc=%08x object=%08x status=%04x control=%08x t=%.9f\n",
            mcu.state["PC"].value, object, space:read_u16(object),
            space:read_u32(mcu.state["R4"].value + 0x7c), machine.time:as_double()))
    end)
local consumer_counter_writes = 0
local consumer_decision_counts = {}
taps[#taps + 1] = mcu.spaces["program"]:install_read_tap(0x2114e0, 0x2115bf,
    "rf_task10_helper_decision", function(address)
        local pc = mcu.state["PC"].value
        if pc ~= 0x2114fc and pc ~= 0x211508 and pc ~= 0x21152a and pc ~= 0x2115a2 then return end
        if address ~= (pc & ~3) then return end
        consumer_decision_counts[pc] = (consumer_decision_counts[pc] or 0) + 1
        if consumer_decision_counts[pc] > 16 then return end
        local space = mcu.spaces["program"]
        local object = mcu.state["R8"].value
        if object < 0x100000 or object >= 0x180000 then return end
        machine:logerror(string.format(
            "rom4_rf_task10_decision: pc=%08x object=%08x status=%04x count=%04x flags=%02x r0=%08x t=%.9f\n",
            pc, object, space:read_u16(object), space:read_u16(0x1084ac),
            space:read_u8(0x1084bf), mcu.state["R0"].value, machine.time:as_double()))
    end)
local consumer_helper_calls = 0
taps[#taps + 1] = mcu.spaces["program"]:install_read_tap(0x210f40, 0x210f9f,
    "rf_task10_consumer_helper", function(address)
        if mcu.state["PC"].value ~= 0x210f5c or address ~= 0x210f5c then return end
        if consumer_helper_calls >= 16 then return end
        consumer_helper_calls = consumer_helper_calls + 1
        local object = mcu.state["R0"].value
        if object < 0x100000 or object >= 0x180000 then return end
        machine:logerror(string.format(
            "rom4_rf_task10_helper: pc=%08x object=%08x status=%04x caller=%08x t=%.9f\n",
            mcu.state["PC"].value, object, mcu.spaces["program"]:read_u16(object),
            mcu.state["R14"].value, machine.time:as_double()))
    end)
taps[#taps + 1] = mcu.spaces["program"]:install_write_tap(0x10fee0, 0x10fee3,
    "rf_task10_consumer_counter", function(address, value, mask)
        if mcu.state["PC"].value ~= 0x211ec4 then return end
        consumer_counter_writes = consumer_counter_writes + 1
        if consumer_counter_writes > 16 then return end
        machine:logerror(string.format(
            "rom4_rf_task10_counter: pc=%08x address=%08x value=%08x mask=%08x t=%.9f\n",
            mcu.state["PC"].value, address, value, mask, machine.time:as_double()))
    end)
for _, address in ipairs({0x295444, 0x298688, 0x299894}) do
    local reads_at_blob = 0
    taps[#taps + 1] = mcu.spaces["program"]:install_read_tap(address, address + 19,
        string.format("rf_tuning_blob_%08x", address), function(offset, value, mask)
            if observing then return end
            reads_at_blob = reads_at_blob + 1
            if reads_at_blob <= 24 then
                machine:logerror(string.format(
                    "rom4_rf_tuning_blob_read: t=%.9f blob=%08x address=%08x value=%08x mask=%08x pc=%08x r0=%08x r1=%08x r2=%08x r3=%08x\n",
                    machine.time:as_double(), address, offset, value, mask,
                    mcu.state["PC"].value, mcu.state["R0"].value,
                    mcu.state["R1"].value, mcu.state["R2"].value, mcu.state["R3"].value))
            end
        end)
end
for _, window in ipairs({{0x211a00, 0x211bff}, {0x20cb80, 0x20cbff}}) do
    local observations = 0
    taps[#taps + 1] = mcu.spaces["program"]:install_read_tap(window[1], window[2],
        string.format("rf_task10_lifecycle_%08x", window[1]), function(offset)
            local pc = mcu.state["PC"].value
            local space = mcu.spaces["program"]
            local status = 0
            if window[1] == 0x211a00 then
                if pc >= 0x211a9a and pc <= 0x211aa4 then
                    local object = pc == 0x211a9a and mcu.state["R0"].value
                        or space:read_u32(mcu.state["R13"].value + 0x18)
                    if object >= 0x100000 and object < 0x180000 then status = space:read_u16(object) end
                    if status ~= 0x1390 then return end
                elseif pc ~= 0x211a54 and pc ~= 0x211a66 then return end
            elseif pc ~= 0x20cb80 then return end
            if observations >= 24 then return end
            observations = observations + 1
            machine:logerror(string.format(
                "rom4_rf_task10_lifecycle: window=%08x t=%.9f address=%08x pc=%08x r0=%08x lr=%08x status=%04x\n",
                window[1], machine.time:as_double(), offset, mcu.state["PC"].value,
                mcu.state["R0"].value, mcu.state["R14"].value, status))
        end)
end
taps[#taps + 1] = mcu.spaces["program"]:install_read_tap(0x254c80, 0x254cff,
    "rf_post_fetch_window", function(offset)
        if offset ~= 0x254c84 or mcu.state["PC"].value ~= 0x254c84
            or mcu.state["R0"].value ~= 10 then return end
        if post_fetches >= 16 then return end
        post_fetches = post_fetches + 1
        machine:logerror(string.format(
            "rom4_rf_post_fetch: t=%.9f address=%08x pc=%08x r0=%08x r1=%08x lr=%08x\n",
            machine.time:as_double(), offset, mcu.state["PC"].value,
            mcu.state["R0"].value, mcu.state["R1"].value, mcu.state["R14"].value))
    end)
taps[#taps + 1] = mcu.spaces["program"]:install_write_tap(0x100000, 0x17ffff,
    "rf_status_header_write", function(offset, value, mask)
        if mcu.state["PC"].value == 0x26be4a then
            machine:logerror(string.format(
                "rom4_rf_status_header_write: t=%.9f pc=%08x address=%08x value=%08x mask=%08x\n",
                machine.time:as_double(), mcu.state["PC"].value, offset, value, mask))
        end
    end)
taps[#taps + 1] = mcu.spaces["program"]:install_write_tap(0x101c8, 0x101cb,
    "rf_mcu_ring_consumer", function(offset, value, mask)
        mcu_ring_writes = mcu_ring_writes + 1
        if mcu_ring_writes <= 32 then
            machine:logerror(string.format(
                "rom4_rf_mcu_ring_write: t=%.9f pc=%08x address=%08x value=%08x mask=%08x\n",
                machine.time:as_double(), mcu.state["PC"].value, offset, value, mask))
        end
    end)
for _, address in ipairs({0x291c34, 0x26be28, 0x26be3c, 0x26be58}) do
    local count = 0
    taps[#taps + 1] = mcu.spaces["program"]:install_read_tap(address & ~3, address | 3,
        string.format("rf_mcu_dispatch_%08x", address), function(offset)
            local pc = mcu.state["PC"].value
            if pc ~= address and pc ~= address + 2 then return end
            count = count + 1
            if count <= 16 then
                local object = mcu.state[address == 0x291c34 and "R5" or "R4"].value
                machine:logerror(string.format(
                    "rom4_rf_mcu_dispatch: address=%08x t=%.9f object=%08x state=%02x r0=%08x r1=%08x\n",
                    address, machine.time:as_double(), object,
                    mcu.spaces["program"]:read_u8(0x108427),
                    mcu.state["R0"].value, mcu.state["R1"].value))
            end
        end)
end
taps[#taps + 1] = memory:install_write_tap(0x06fd, 0x06fd,
    "rf_scan_mode_writers", function(offset, value)
        local pc = cpu.state["PC"].value
        mode_writers[pc] = (mode_writers[pc] or 0) + 1
        if mode_writers[pc] <= 4 then
            machine:logerror(string.format("rom4_rf_scan_mode_write: t=%.9f pc=%04x value=%04x\n",
                machine.time:as_double(), pc, value))
        end
    end)
taps[#taps + 1] = memory:install_write_tap(0x1256, 0x1256,
    "rf_candidate_counter_writers", function(offset, value)
        local pc = cpu.state["PC"].value
        counter_writers[pc] = (counter_writers[pc] or 0) + 1
        if counter_writers[pc] <= 4 then
            machine:logerror(string.format("rom4_rf_counter_write: t=%.9f pc=%04x value=%04x\n",
                machine.time:as_double(), pc, value))
        end
    end)
taps[#taps + 1] = memory:install_write_tap(0x1266, 0x1266,
    "rf_candidate_flags", function(offset, value)
        flag_writes = flag_writes + 1
        if flag_writes <= 24 then
            machine:logerror(string.format("rom4_rf_flag_write: t=%.9f pc=%04x value=%04x\n",
                machine.time:as_double(), cpu.state["PC"].value, value))
        end
    end)
local tuning_writes = {}
local tuning_field_writes = {}
for _, space_name in ipairs({"data", "program"}) do
    -- Program overlay writes update data storage without traversing data-space taps.
    taps[#taps + 1] = cpu.spaces[space_name]:install_write_tap(0x216a, 0x216c,
        "rf_programming_field_writers_" .. space_name, function(offset, value)
            if observing then return end
            local key = space_name .. offset
            tuning_field_writes[key] = (tuning_field_writes[key] or 0) + 1
            if tuning_field_writes[key] <= 12 then
                machine:logerror(string.format(
                    "rom4_rf_tuning_field_write: space=%s t=%.9f pc=%04x address=%04x value=%04x a=%010x b=%010x ar2=%04x ar3=%04x\n",
                    space_name, machine.time:as_double(), cpu.state["PC"].value, offset, value,
                    cpu.state["A"].value, cpu.state["B"].value,
                    cpu.state["AR2"].value, cpu.state["AR3"].value))
            end
        end)
end
local tuning_stages = {}
taps[#taps + 1] = cpu.spaces["program"]:install_read_tap(0x3ffb, 0x4048,
    "rf_programming_stages", function(offset)
        local pc = cpu.state["PC"].value
        if observing or (pc ~= offset and pc ~= offset + 1) then return end
        if offset ~= 0x3ffb and offset ~= 0x4006 and offset ~= 0x400f
            and offset ~= 0x402e and offset ~= 0x403a and offset ~= 0x4048 then return end
        tuning_stages[offset] = (tuning_stages[offset] or 0) + 1
        if tuning_stages[offset] <= 4 then
            machine:logerror(string.format(
                "rom4_rf_tuning_stage: t=%.9f address=%04x pc=%04x a=%010x b=%010x ar2=%04x flags6be=%04x flags216a=%04x addend216b=%04x addend216c=%04x field60=%04x selected1867=%04x\n",
                machine.time:as_double(), offset, pc, cpu.state["A"].value,
                cpu.state["B"].value, cpu.state["AR2"].value,
                memory:read_u16(0x06be), memory:read_u16(0x216a),
                memory:read_u16(0x216b), memory:read_u16(0x216c),
                memory:read_u16(0x60), memory:read_u16(0x1867)))
        end
    end)
taps[#taps + 1] = cpu.spaces["io"]:install_write_tap(0x30, 0x32,
    "rf_programming_observation", function(offset, value)
        if observing then return end
        tuning_writes[offset] = (tuning_writes[offset] or 0) + 1
        if tuning_writes[offset] <= 16 then
            machine:logerror(string.format(
                "rom4_rf_tuning_write: t=%.9f pc=%04x port=%04x value=%04x a=%010x ar2=%04x flags216a=%04x field60=%04x selected1867=%04x\n",
                machine.time:as_double(), cpu.state["PC"].value, offset, value,
                cpu.state["A"].value, cpu.state["AR2"].value,
                memory:read_u16(0x216a), memory:read_u16(0x60), memory:read_u16(0x1867)))
        end
    end)
taps[#taps + 1] = cpu.spaces["io"]:install_read_tap(0x27, 0x27,
    "mathematical_rf_input", function()
        local value = 0
        if mode == "dc" then
            value = reads % 2 == 0 and 2048 or 0
        elseif mode == "tone" then
            -- One turn per sixteen word pairs; no physical sample-rate claim.
            local angle = 2 * math.pi * (reads // 2 % 16) / 16
            local component = reads % 2 == 0 and math.cos(angle) or math.sin(angle)
            value = math.floor(2048 * component + 0.5) & 0xffff
        end
        reads = reads + 1
        local state = memory:read_u16(0x00ac)
        local window = window_modes[state] or {on = 0, off = 0}
        window_modes[state] = window
        if (port3 & 1) ~= 0 then reads_on = reads_on + 1; window.on = window.on + 1
        else reads_off = reads_off + 1; window.off = window.off + 1 end
        local key = (state << 16) | port3
        local group = window_states[key] or {mode = state, control = port3, reads = 0, aged = 0, minimum = nil, maximum = 0}
        window_states[key] = group
        group.reads = group.reads + 1
        if (port3 & 1) == 0 and last_clear then
            local age = math.floor((machine.time:as_double() - last_clear) * 1000000 + 0.5)
            group.aged = group.aged + 1
            group.minimum = math.min(group.minimum or age, age)
            group.maximum = math.max(group.maximum, age)
        end
        if reads <= 64 then
            machine:logerror(string.format(
                "rom4_rf_sample_consumption: index=%d t=%.12f pc=%04x counter=%04x\n",
                reads, machine.time:as_double(), cpu.state["PC"].value,
                cpu.spaces["io"]:read_u16(0x0d)))
        end
        local pc = cpu.state["PC"].value
        if pc >= 0x330a and pc <= 0x3326 then
            scan_reads = scan_reads + 1
            if scan_reads <= 64 then
                machine:logerror(string.format(
                    "rom4_rf_scan_consumption: index=%d t=%.12f pc=%04x counter=%04x\n",
                    scan_reads, machine.time:as_double(), pc,
                    cpu.spaces["io"]:read_u16(0x0d)))
            end
        end
        return value
    end)
for _, address in ipairs({0x0ad1, 0x0add, 0x0ae4, 0x0ca3, 0x0cef,
    0x0a5c, 0x0ac2, 0x2420, 0x3129, 0x3130, 0x3132,
    0x3138, 0x313a, 0x313d, 0x313f, 0x3142, 0x314d, 0x3151, 0x3153, 0x315c, 0x316b, 0x37ce,
    0x37e1, 0x3807, 0x3d5e,
    0x0b6a, 0x0b6c, 0x0b78, 0x2523, 0x25fe,
    0x0b00, 0x0b0b, 0x0b0d, 0x0b1e, 0x0b24, 0x0b2e, 0x0b3a, 0x0b42, 0x0b4a}) do
    taps[#taps + 1] = cpu.spaces["program"]:install_read_tap(address, address,
        string.format("rf_score_%04x", address), function(offset)
            if observing then return end
            local pc = cpu.state["PC"].value
            if pc ~= offset and pc ~= offset + 1 then return end
            if address == 0x3d5e then
                search_requests = search_requests + 1
                if search_requests <= 4 then
                    local cursor = cpu.state["AR2"].value
                    local origin = cursor
                    local words = {}
                    for index = 1, 33 do
                        words[#words + 1] = string.format("%04x", memory:read_u16(cursor))
                        cursor = cursor == 0x0851 and 0x0800 or (cursor + 1) & 0xffff
                    end
                    machine:logerror(string.format(
                        "rom4_rf_search_request: t=%.9f ar2=%04x words=%s\n",
                        machine.time:as_double(), origin, table.concat(words, ",")))
                end
                return
            end
            if address == 0x37e1 or address == 0x3807 then
                publication_counts[address] = (publication_counts[address] or 0) + 1
                if publication_counts[address] <= 16 then
                    machine:logerror(string.format(
                        "rom4_rf_native_publish_exit: address=%04x t=%.9f type=%04x bytes=%d producer=%04x consumer=%04x control6e1=%04x\n",
                        address, machine.time:as_double(), cpu.state["AR4"].value,
                        cpu.state["AR5"].value, memory:read_u16(0x08e4),
                        memory:read_u16(0x08e5), memory:read_u16(0x06e1)))
                end
                return
            end
            if address == 0x37ce then
                local kind, length = cpu.state["AR4"].value, cpu.state["AR5"].value
                message_counts[kind] = (message_counts[kind] or 0) + 1
                if message_counts[kind] <= 4 and length <= 64 then
                    local words = {}
                    for index = 0, (length + 1) // 2 - 1 do
                        words[#words + 1] = string.format("%04x",
                            memory:read_u16((cpu.state["AR3"].value + index) & 0xffff))
                    end
                    machine:logerror(string.format(
                        "rom4_rf_native_publication: t=%.9f type=%04x bytes=%d words=%s\n",
                        machine.time:as_double(), kind, length, table.concat(words, ",")))
                end
                return
            end
            if address >= 0x3138 and address <= 0x316b then
                publication_counts[address] = (publication_counts[address] or 0) + 1
                if publication_counts[address] <= 4 then
                    machine:logerror(string.format(
                        "rom4_rf_post_scan: address=%04x t=%.9f a=%010x control6e3=%04x flags1949=%04x cursor1983=%04x selected1867=%04x cursor_word=%04x active1836=%04x pending1869=%04x\n",
                        address, machine.time:as_double(), cpu.state["A"].value & 0xffffffffff,
                        memory:read_u16(0x06e3), memory:read_u16(0x1949), memory:read_u16(0x1983),
                        memory:read_u16(0x1867), memory:read_u16(memory:read_u16(0x1983)),
                        memory:read_u16(0x1836), memory:read_u16(0x1869)))
                end
                return
            end
            if address == 0x0a5c or address == 0x0ac2 or address == 0x2420
                or address == 0x3129 or address == 0x3130 or address == 0x3132 then
                publication_counts[address] = (publication_counts[address] or 0) + 1
                if publication_counts[address] <= 4 then
                    machine:logerror(string.format(
                        "rom4_rf_scan_entry: address=%04x t=%.9f a=%010x b=%010x ar1=%04x ar4=%04x active1836=%04x pending1869=%04x flags6bc=%04x mode6fd=%04x cursor1983=%04x selected1867=%04x cursor_word=%04x\n",
                        address, machine.time:as_double(), cpu.state["A"].value & 0xffffffffff,
                        cpu.state["B"].value & 0xffffffffff, cpu.state["AR1"].value,
                        cpu.state["AR4"].value, memory:read_u16(0x1836), memory:read_u16(0x1869),
                        memory:read_u16(0x06bc), memory:read_u16(0x06fd), memory:read_u16(0x1983),
                        memory:read_u16(0x1867), memory:read_u16(memory:read_u16(0x1983))))
                end
                return
            end
            if address >= 0x0b00 and address <= 0x0b4a then
                publication_counts[address] = (publication_counts[address] or 0) + 1
                if address == 0x0b0b then
                    local counter = memory:read_u16(0x1256)
                    counter_min = math.min(counter_min, counter)
                    counter_max = math.max(counter_max, counter)
                end
                if publication_counts[address] <= 4 then
                    machine:logerror(string.format(
                        "rom4_rf_candidate_history: address=%04x t=%.9f a=%010x b=%010x ar1=%04x current=%04x previous=%04x phase=%04x counter1256=%04x counter1257=%04x count1344=%04x\n",
                        address, machine.time:as_double(), cpu.state["A"].value & 0xffffffffff,
                        cpu.state["B"].value & 0xffffffffff, cpu.state["AR1"].value,
                        memory:read_u16(0x1258), memory:read_u16(0x1259), memory:read_u16(0x126a),
                        memory:read_u16(0x1256), memory:read_u16(0x1257), memory:read_u16(0x1344)))
                end
                return
            end
            if address == 0x0b6a or address == 0x0b6c or address == 0x0b78
                or address == 0x2523 or address == 0x25fe then
                publication_counts[address] = (publication_counts[address] or 0) + 1
                if publication_counts[address] <= 4 then
                    machine:logerror(string.format(
                        "rom4_rf_continuation: address=%04x t=%.9f a=%010x b=%010x sp=%04x flags=%04x\n",
                        address, machine.time:as_double(), cpu.state["A"].value & 0xffffffffff,
                        cpu.state["B"].value & 0xffffffffff, cpu.state["SP"].value,
                        memory:read_u16(0x1266)))
                end
                return
            end
            if address == 0x0ca3 or address == 0x0cef then
                publication_counts[address] = (publication_counts[address] or 0) + 1
                if publication_counts[address] <= 4 then
                    machine:logerror(string.format(
                        "rom4_rf_selection_tail: address=%04x t=%.9f a=%010x score=%04x flags=%04x count=%04x\n",
                        address, machine.time:as_double(), cpu.state["A"].value & 0xffffffffff,
                        memory:read_u16(0x198d), memory:read_u16(0x1266), memory:read_u16(0x1344)))
                end
                return
            end
            if address == 0x0add then second = second + 1; return end
            if address == 0x0ae4 then
                accepted = accepted + 1
                if accepted <= 12 then
                    machine:logerror(string.format(
                        "rom4_rf_selection: t=%.9f a=%010x ar2=%04x ar3=%04x ar4=%04x ar5=%04x prior=%04x\n",
                        machine.time:as_double(), cpu.state["A"].value & 0xffffffffff,
                        cpu.state["AR2"].value, cpu.state["AR3"].value,
                        cpu.state["AR4"].value, cpu.state["AR5"].value,
                        memory:read_u16(0x198d)))
                end
                return
            end
            local value = cpu.state["A"].value & 0xffffffffff
            results = results + 1
            if value ~= 0 then nonzero = nonzero + 1 end
            maximum = math.max(maximum, value)
            if results <= 8 then
                machine:logerror(string.format(
                    "rom4_rf_score: mode=%s t=%.9f a=%010x threshold=%04x prior=%04x flag6fd=%04x\n",
                    mode, machine.time:as_double(), value, memory:read_u16(0x219b),
                    memory:read_u16(0x198d), memory:read_u16(0x06fd)))
            end
        end)
end
local stop = emu.add_machine_stop_notifier(function()
    observing = true
    machine:logerror(string.format("rom4_rf_window_summary: writes=%d transitions=%d reads_on=%d reads_off=%d physical_gate_claim=0\n",
        port3_writes, port3_transitions, reads_on, reads_off))
    for state, window in pairs(window_modes) do
        machine:logerror(string.format("rom4_rf_window_mode: mode=%04x reads_on=%d reads_off=%d\n", state, window.on, window.off))
    end
    for _, group in pairs(window_states) do
        machine:logerror(string.format("rom4_rf_window_state: mode=%04x control=%04x reads=%d aged=%d minimum_us=%d maximum_us=%d\n",
            group.mode, group.control, group.reads, group.aged, group.minimum or 0, group.maximum))
    end
    for _, target in ipairs({0x0b6a, 0x0a5c, 0x0ac2}) do
    for _, space_name in ipairs({"program", "data"}) do
        local space = cpu.spaces[space_name]
        local matches = 0
        for address = 0, 0xffff do
            if space:read_u16(address) == target then
                matches = matches + 1
                local words = {}
                for position = math.max(0, address - 4), math.min(0xffff, address + 4) do
                    words[#words + 1] = string.format("%04x", space:read_u16(position))
                end
                machine:logerror(string.format(
                    "rom4_rf_continuation_reference: target=%04x space=%s address=%04x words=%s\n",
                    target, space_name, address, table.concat(words, ",")))
            end
        end
        machine:logerror(string.format(
            "rom4_rf_continuation_reference_summary: target=%04x space=%s words=65536 matches=%d exhaustive_dispatch_claim=0\n",
            target, space_name, matches))
    end
    end
    for _, start in ipairs({0x0a5c, 0x0a9c, 0x0ae4, 0x0b24, 0x0b64, 0x0ba4, 0x0be4, 0x0ca3, 0x0cef, 0x2523, 0x25fe}) do
        local words = {}
        for index = 0, 63 do
            words[#words + 1] = string.format("%04x", cpu.spaces["program"]:read_u16(start + index))
        end
        machine:logerror(string.format("rom4_rf_selection_code: address=%04x words=%s\n",
            start, table.concat(words, ",")))
    end
    for pc, count in pairs(counter_writers) do
        machine:logerror(string.format("rom4_rf_counter_writer_count: pc=%04x count=%d\n", pc, count))
    end
    for pc, count in pairs(mode_writers) do
        machine:logerror(string.format("rom4_rf_scan_mode_writer_count: pc=%04x count=%d\n", pc, count))
    end
    for kind, count in pairs(message_counts) do
        machine:logerror(string.format("rom4_rf_native_publication_count: type=%04x count=%d\n", kind, count))
    end
    for _, address in ipairs({0x0ca3, 0x0cef, 0x0a5c, 0x0ac2, 0x2420, 0x3129, 0x3130, 0x3132,
        0x3138, 0x313a, 0x313d, 0x313f, 0x3142, 0x314d, 0x3151, 0x3153, 0x315c, 0x316b,
        0x0b6a, 0x0b6c, 0x0b78, 0x2523, 0x25fe,
        0x0b00, 0x0b0b, 0x0b0d, 0x0b1e, 0x0b24, 0x0b2e, 0x0b3a, 0x0b42, 0x0b4a}) do
        machine:logerror(string.format("rom4_rf_selection_tail_count: address=%04x count=%d\n",
            address, publication_counts[address] or 0))
    end
    machine:logerror(string.format("rom4_rf_candidate_counter_range: minimum=%04x maximum=%04x\n",
        counter_min, counter_max))
    machine:logerror(string.format("rom4_rf_cpu_state: illegal=%d pc=%04x\n",
        cpu.state["ILLEGAL"].value, cpu.state["PC"].value))
    machine:logerror(string.format(
        "rom4_rf_score_summary: mode=%s reads=%d results=%d nonzero=%d maximum=%010x second=%d accepted=%d acquisition_claim=0\n",
        mode, reads, results, nonzero, maximum, second, accepted))
end)
_G.rom4_rf_score = {taps = taps, stop = stop}

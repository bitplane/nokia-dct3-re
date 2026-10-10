-- Diagnostic mathematical port-input stimuli, not a GSM RF model.
local machine = manager.machine
local cpu = assert(machine.devices[":dsp_c54x:cpu"])
local memory = cpu.spaces["data"]
local mode = os.getenv("ROM4_RF_SCORE_MODE") or "zero"
assert(mode == "zero" or mode == "dc" or mode == "tone")
local reads, results, nonzero, second, accepted = 0, 0, 0, 0, 0
local maximum = 0
local observing = false
local publication_counts = {}
local flag_writes = 0
local counter_min, counter_max = 0xffff, 0
local counter_writers = {}
local mode_writers = {}
local message_counts = {}
local taps = {}
local mcu = assert(machine.devices[":maincpu"])
local mcu_ring_writes = 0
taps[#taps + 1] = mcu.spaces["program"]:install_write_tap(0x100000, 0x17ffff,
    "rf_status_header_write", function(offset, value, mask)
        if mcu.state["PC"].value == 0x26be48 then
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
        return value
    end)
for _, address in ipairs({0x0ad1, 0x0add, 0x0ae4, 0x0ca3, 0x0cef,
    0x0a5c, 0x0ac2, 0x2420, 0x3129, 0x3130, 0x3132,
    0x3138, 0x313a, 0x313d, 0x313f, 0x3142, 0x314d, 0x3151, 0x3153, 0x315c, 0x316b, 0x37ce,
    0x37e1, 0x3807,
    0x0b6a, 0x0b6c, 0x0b78, 0x2523, 0x25fe,
    0x0b00, 0x0b0b, 0x0b0d, 0x0b1e, 0x0b24, 0x0b2e, 0x0b3a, 0x0b42, 0x0b4a}) do
    taps[#taps + 1] = cpu.spaces["program"]:install_read_tap(address, address,
        string.format("rf_score_%04x", address), function(offset)
            if observing then return end
            local pc = cpu.state["PC"].value
            if pc ~= offset and pc ~= offset + 1 then return end
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
                        "rom4_rf_post_scan: address=%04x t=%.9f a=%010x control6e3=%04x flags1949=%04x request1983=%04x active1836=%04x pending1869=%04x\n",
                        address, machine.time:as_double(), cpu.state["A"].value & 0xffffffffff,
                        memory:read_u16(0x06e3), memory:read_u16(0x1949), memory:read_u16(0x1983),
                        memory:read_u16(0x1836), memory:read_u16(0x1869)))
                end
                return
            end
            if address == 0x0a5c or address == 0x0ac2 or address == 0x2420
                or address == 0x3129 or address == 0x3130 or address == 0x3132 then
                publication_counts[address] = (publication_counts[address] or 0) + 1
                if publication_counts[address] <= 4 then
                    machine:logerror(string.format(
                        "rom4_rf_scan_entry: address=%04x t=%.9f a=%010x b=%010x ar1=%04x ar4=%04x active1836=%04x pending1869=%04x flags6bc=%04x mode6fd=%04x\n",
                        address, machine.time:as_double(), cpu.state["A"].value & 0xffffffffff,
                        cpu.state["B"].value & 0xffffffffff, cpu.state["AR1"].value,
                        cpu.state["AR4"].value, memory:read_u16(0x1836), memory:read_u16(0x1869),
                        memory:read_u16(0x06bc), memory:read_u16(0x06fd)))
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
    machine:logerror(string.format(
        "rom4_rf_score_summary: mode=%s reads=%d results=%d nonzero=%d maximum=%010x second=%d accepted=%d acquisition_claim=0\n",
        mode, reads, results, nonzero, maximum, second, accepted))
end)
_G.rom4_rf_score = {taps = taps, stop = stop}

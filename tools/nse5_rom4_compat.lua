-- Observation-only: this composition tests compatibility, not fitted silicon.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local space = cpu.spaces.program
local next_sample = 1
local times = {0.1, 0.5, 1, 2, 4, 8}
local taps, entry_counts = {}, {}
local storage_cache_writes = 0
taps[#taps + 1] = space:install_write_tap(0x157424, 0x15742f,
    "nse5_compat_storage_cache_writer", function(offset, data, mask)
        storage_cache_writes = storage_cache_writes + 1
        if storage_cache_writes <= 16 then
            machine:logerror(string.format(
                "nse5_compat_storage_cache_write: address=%08x data=%08x mask=%08x pc=%08x lr=%08x t=%.9f\n",
                offset, data, mask, cpu.state["PC"].value,
                cpu.state["R14"].value, machine.time:as_double()))
        end
    end)
local menu_fixture = os.getenv("NSE5_COMPAT_MENU") == "1"
local menu_step = 0
local entries = {
    {0x46cb84, "storage_cache_copy"},
    {0x3fafd8, "storage_journal_done"},
    {0x432eae, "verifier"}, {0x3bc2a8, "service_init"},
    {0x45c61c, "optional_boot_call"}, {0x3bb818, "startup_index"},
    {0x4bc214, "arm_wrapper"}, {0x4caa04, "uif_irq7"},
    {0x3a2612, "startup_call_3a2612"}, {0x46bdd8, "init_46bdd8"},
    {0x46bc16, "read_46bc16"}, {0x3209e8, "init_3209e8"},
    {0x311eb0, "dispatch_311eb0"},
    {0x4cfc5c, "key_decoder"},
    {0x474112, "key_init"}, {0x4740f0, "key_irq0"},
    {0x474004, "key_scan"},
    {0x4cfcf6, "key_state_init"}, {0x4cfc18, "key_flags_set"},
    {0x4cfbc4, "key_unmask_or_post"},
    {0x39378c, "application_start"}, {0x393518, "startup_event_read"},
    {0x393528, "startup_event_dispatch"}, {0x393942, "startup_ready_loop"},
    {0x3939d0, "startup_ready_test"}, {0x3939e6, "startup_ready_done"},
    {0x393b04, "startup_alternate"},
    {0x4c8b94, "startup_report14"}, {0x4c8bd0, "startup_report15"},
    {0x49fddc, "subsystem_group_release"}, {0x49fe36, "subsystem_group_skip"},
    {0x469cc2, "startup_release_predicate"}, {0x469ce0, "startup_release_retry"},
    {0x3aef0c, "startup_selftest_request"},
    {0x311416, "startup_selftest_expiry"}, {0x311430, "startup_selftest_busy_expiry"},
    {0x469dbc, "dsp_control_forward"}, {0x30e728, "dsp_control_receive"},
    {0x30e7a6, "startup_selftest_reply"},
    {0x311506, "task2_receive_loop"},
    {0x4dea50, "task2_message_received"},
    {0x3bbe04, "task2_dsp_queue_post"},
    {0x3bbe98, "task2_dsp_queue_result"},
    {0x433626, "code_block_chunk"}, {0x4336a6, "code_block_partial"},
    {0x4e94d6, "system_stop"},
    {0x3af4d0, "identity_reply"}, {0x3af540, "identity_payload_check"},
    {0x3af64e, "identity_invalid"},
}
for _, entry in ipairs(entries) do
    local address, name = entry[1], entry[2]
    entry_counts[name] = 0
    taps[#taps + 1] = space:install_read_tap(address & ~3, (address & ~3) + 3,
        "nse5_compat_" .. name, function(offset, data, mask)
            local pc = cpu.state["PC"].value
            if pc ~= address then return end
            if name == "storage_cache_copy" and cpu.state["R0"].value ~= 0x157424 then return end
            if name == "storage_journal_done" and
                    space:read_u32(cpu.state["R6"].value) ~= 0x5fa000 then return end
            if name == "task2_message_received" and cpu.state["R14"].value ~= 0x311511 then return end
            if name == "task2_dsp_queue_post" and cpu.state["R14"].value ~= 0x469e0d then return end
            if name == "task2_dsp_queue_result" and
                    space:read_u32(cpu.state["R13"].value + 8) ~= 0x469e0d then return end
            entry_counts[name] = entry_counts[name] + 1
            if entry_counts[name] <= ((name == "startup_index" or name == "task2_dsp_queue_post" or
                    name == "task2_dsp_queue_result" or
                    name == "startup_event_dispatch") and 64 or 12) or
                    (name == "key_decoder" and cpu.state["R0"].value ~= 0xff) then
                machine:logerror(string.format(
                    "nse5_compat_entry: name=%s pc=%08x r0=%08x r1=%08x lr=%08x t=%.6f r4=%08x\n",
                    name, pc, cpu.state["R0"].value, cpu.state["R1"].value,
                    cpu.state["R14"].value, machine.time:as_double(),
                    cpu.state["R4"].value))
                if name == "storage_cache_copy" then
                    machine:logerror(string.format(
                        "nse5_compat_storage_cache_copy: destination=%08x source=%08x length=%08x lr=%08x t=%.9f\n",
                        cpu.state["R0"].value, cpu.state["R1"].value,
                        cpu.state["R2"].value, cpu.state["R14"].value,
                        machine.time:as_double()))
                end
                if name == "storage_journal_done" then
                    local cache = {}
                    for index = 0, 0x897 do
                        cache[#cache + 1] = string.format("%02x", space:read_u8(0x157424 + index))
                    end
                    machine:logerror("nse5_compat_storage_cache_snapshot: bytes=" .. table.concat(cache) .. "\n")
                end
                if name == "subsystem_group_skip" or name == "subsystem_group_release" then
                    local r5 = cpu.state["R5"].value
                    machine:logerror(string.format(
                        "nse5_compat_release: path=%s bootstrap=%02x mode=%02x r5=%08x r5_byte0=%02x r5_byte1=%02x predicate=%02x t=%.6f\n",
                        name, space:read_u8(0x16702c), space:read_u8(0x16ab88),
                        r5, space:read_u8(r5), space:read_u8(r5 + 1),
                        space:read_u8(0x17fe15), machine.time:as_double()))
                end
                if name == "code_block_chunk" or name == "code_block_partial" then
                    local state = cpu.state["R4"].value
                    machine:logerror(string.format(
                        "nse5_compat_code_block: name=%s request=%04x status=%04x remaining=%04x chunk=%04x source=%08x destination=%08x t=%.6f\n",
                        name, space:read_u16(0x100e2), space:read_u16(0x100e4),
                        space:read_u16(state + 6), space:read_u16(state + 8),
                        space:read_u32(state + 0x10), space:read_u32(state + 0x14),
                        machine.time:as_double()))
                end
                if name == "system_stop" then
                    local stack = {}
                    for index = 0, 7 do
                        stack[#stack + 1] = string.format("%08x", space:read_u32(
                            cpu.state["R13"].value + index * 4))
                    end
                    machine:logerror(string.format(
                        "nse5_compat_system_stop: code=%08x lr=%08x sp=%08x r1=%08x r2=%08x r3=%08x stack=%s t=%.9f\n",
                        cpu.state["R0"].value, cpu.state["R14"].value,
                        cpu.state["R13"].value, cpu.state["R1"].value,
                        cpu.state["R2"].value, cpu.state["R3"].value,
                        table.concat(stack, ":"), machine.time:as_double()))
                end
                if name == "identity_reply" or name == "identity_payload_check" or name == "identity_invalid" then
                    local message = name == "identity_reply" and cpu.state["R0"].value or cpu.state["R6"].value
                    local bytes = {}
                    for index = 0, 61 do
                        bytes[#bytes + 1] = string.format("%02x", space:read_u8(message + index))
                    end
                    machine:logerror(string.format(
                        "nse5_compat_identity_reply: name=%s message=%08x bytes=%s state=%08x flags=%02x t=%.9f\n",
                        name, message, table.concat(bytes), cpu.state["R12"].value,
                        space:read_u8(0x17fe15),
                        machine.time:as_double()))
                end
                if name == "dsp_control_receive" or name == "startup_selftest_reply" or
                        name == "task2_message_received" or name == "dsp_control_forward" then
                    local message = name ~= "startup_selftest_reply" and
                        cpu.state["R0"].value or cpu.state["R4"].value
                    local bytes = {}
                    local last = name == "dsp_control_forward" and 63 or 11
                    if name == "task2_message_received" and space:read_u8(message + 8) == 0x34 then
                        last = 23 -- 14-byte payload, including algorithm-0x82 MSID.
                    end
                    for index = 0, last do
                        bytes[#bytes + 1] = string.format("%02x", space:read_u8(message + index))
                    end
                    machine:logerror(string.format(
                        "nse5_compat_dsp_control: name=%s message=%08x bytes=%s flags=%02x t=%.6f\n",
                        name, message, table.concat(bytes), space:read_u8(0x17fe15),
                        machine.time:as_double()))
                end
                if name == "task2_dsp_queue_post" then
                    local message = cpu.state["R1"].value
                    local catalogue = space:read_u32(0x10003c)
                    machine:logerror(string.format(
                        "nse5_compat_task2_queue: primitive=%02x detail=%02x producer=%02x consumer=%02x capacity=%02x t=%.6f\n",
                        space:read_u8(message + 8), space:read_u8(message + 9),
                        space:read_u8(0x101acc), space:read_u8(0x101acd),
                        space:read_u8(catalogue + 2 * 12 + 7), machine.time:as_double()))
                end
                if name == "task2_dsp_queue_result" and cpu.state["R4"].value == 0 then
                    local message = cpu.state["R5"].value
                    machine:logerror(string.format(
                        "nse5_compat_task2_queue_failure: primitive=%02x detail=%02x t=%.6f\n",
                        space:read_u8(message + 8), space:read_u8(message + 9),
                        machine.time:as_double()))
                end
            end
        end)
end
local column_writes = 0
local request_buffer_writes = 0
taps[#taps + 1] = space:install_write_tap(0x104384, 0x1043a7,
    "nse5_compat_request_buffer", function(offset, data, mask)
        local when = machine.time:as_double()
        if when < 0.6 or when > 0.671 then return end
        request_buffer_writes = request_buffer_writes + 1
        if request_buffer_writes > 96 then return end
        machine:logerror(string.format(
            "nse5_compat_request_buffer: address=%08x data=%08x mask=%08x pc=%08x lr=%08x r0=%08x r1=%08x r2=%08x t=%.9f\n",
            offset, data, mask, cpu.state["PC"].value, cpu.state["R14"].value,
            cpu.state["R0"].value, cpu.state["R1"].value, cpu.state["R2"].value, when))
    end)
taps[#taps + 1] = space:install_write_tap(0x10048, 0x1004b,
    "nse5_compat_request_source", function(offset, data, mask)
        if machine.time:as_double() < 0.66 then return end
        machine:logerror(string.format(
            "nse5_compat_request_source: data=%08x mask=%08x pc=%08x lr=%08x r0=%08x r1=%08x r2=%08x r3=%08x r4=%08x r5=%08x t=%.9f\n",
            data, mask, cpu.state["PC"].value, cpu.state["R14"].value,
            cpu.state["R0"].value, cpu.state["R1"].value, cpu.state["R2"].value,
            cpu.state["R3"].value, cpu.state["R4"].value, cpu.state["R5"].value,
            machine.time:as_double()))
        if cpu.state["PC"].value == 0x432cd8 and (data & 0xffff) == 0x8184 then
            local bytes = {}
            for index = 0, 11 do
                bytes[#bytes + 1] = string.format("%02x", space:read_u8(0x157424 + index))
            end
            machine:logerror("nse5_compat_mask_input: bytes=" .. table.concat(bytes) .. "\n")
        end
    end)
taps[#taps + 1] = space:install_write_tap(0x20068, 0x2006b,
    "nse5_compat_column_mask", function(offset, data, mask)
        if (mask & 0xff) == 0 then return end
        column_writes = column_writes + 1
        machine:logerror(string.format(
            "nse5_compat_column_mask: value=%02x pc=%08x lr=%08x t=%.6f\n",
            data & 0xff, cpu.state["PC"].value, cpu.state["R14"].value,
            machine.time:as_double()))
    end)
local key_state_writes = 0
taps[#taps + 1] = space:install_write_tap(0x168990, 0x168997,
    "nse5_compat_key_state", function(offset, data, mask)
        for lane = 0, 3 do
            local address = offset + lane
            local shift = (3 - lane) * 8
            if (address == 0x168990 or address == 0x168994) and
                    ((mask >> shift) & 0xff) ~= 0 then
                key_state_writes = key_state_writes + 1
                if key_state_writes <= 64 then
                    machine:logerror(string.format(
                        "nse5_compat_key_state: address=%08x value=%02x pc=%08x lr=%08x t=%.6f\n",
                        address, (data >> shift) & 0xff, cpu.state["PC"].value,
                        cpu.state["R14"].value, machine.time:as_double()))
                end
            end
        end
    end)
local release_state_writes = 0
taps[#taps + 1] = space:install_write_tap(0x17fe14, 0x17fe17,
    "nse5_compat_release_state", function(offset, data, mask)
        if (mask & 0x00ff0000) == 0 then return end
        release_state_writes = release_state_writes + 1
        if release_state_writes <= 64 then
            machine:logerror(string.format(
                "nse5_compat_release_state: value=%02x pc=%08x lr=%08x t=%.6f\n",
                (data >> 16) & 0xff, cpu.state["PC"].value,
                cpu.state["R14"].value, machine.time:as_double()))
        end
    end)
local dsp = machine.devices[":dsp_c54x:cpu"]
local dsp_space = dsp.spaces["data"]
local identity_input_writes = 0
taps[#taps + 1] = dsp_space:install_write_tap(0x1f0c, 0x1f0d,
    "nse5_compat_identity_input_writer", function(offset, data, mask)
        identity_input_writes = identity_input_writes + 1
        if identity_input_writes <= 16 then
            machine:logerror(string.format(
                "nse5_compat_identity_input_write: address=%04x data=%04x mask=%04x pc=%04x t=%.9f\n",
                offset, data, mask, dsp.state["PC"].value, machine.time:as_double()))
        end
    end)
local capture_tail = os.getenv("NSE5_COMPAT_DSP_TAIL") == "1"
local program_tail, tail_index, tail_dumped = {}, 0, false
local previous_program_read
local helper_seen = false
local program_tap
local stack_writes = 0
if capture_tail then
    local ignored_uploads = 0
    taps[#taps + 1] = dsp.spaces["program"]:install_write_tap(0x2800, 0x2fff,
        "nse5_compat_dsp_upper_program_upload", function(offset, data, mask)
            ignored_uploads = ignored_uploads + 1
            if ignored_uploads <= 16 or (offset >= 0x2828 and offset <= 0x2836) then
                machine:logerror(string.format(
                    "nse5_compat_dsp_upper_program_upload: address=%04x value=%04x pc=%04x pmst=%04x t=%.9f\n",
                    offset, data, dsp.state["PC"].value,
                    dsp.state["PMST"].value, machine.time:as_double()))
            end
        end)
    taps[#taps + 1] = dsp_space:install_write_tap(0x0830, 0x0860,
        "nse5_compat_dsp_return_stack", function(offset, data, mask)
            stack_writes = stack_writes + 1
            if stack_writes <= 2048 then
                machine:logerror(string.format(
                    "nse5_compat_dsp_return_stack: address=%04x old=%04x value=%04x pc=%04x sp=%04x t=%.9f\n",
                    offset, dsp_space:read_u16(offset), data,
                    dsp.state["PC"].value, dsp.state["SP"].value,
                    machine.time:as_double()))
            end
        end)
end
local function dump_program_tail(reason, keep_capturing)
    tail_dumped = not keep_capturing
    machine:logerror("nse5_compat_dsp_program_tail_reason: " .. reason .. "\n")
    -- Extension words can appear: these are program reads, not decoded instructions.
    for index = math.max(1, tail_index - 63), tail_index do
        local row = program_tail[(index - 1) % 64 + 1]
        machine:logerror(string.format(
            "nse5_compat_dsp_program_tail: address=%04x word=%04x sp=%04x a=%010x b=%010x ar2=%04x ar3=%04x ar4=%04x st0=%04x st1=%04x\n",
            table.unpack(row)))
    end
end
if capture_tail then
    program_tap = dsp.spaces["program"]:install_read_tap(0, 0xffff,
        "nse5_compat_dsp_program_tail", function(offset, data, mask)
            if tail_dumped or offset ~= ((dsp.state["PC"].value - 1) & 0xffff) then return end
            tail_index = tail_index + 1
            program_tail[(tail_index - 1) % 64 + 1] = {
                offset, data, dsp.state["SP"].value, dsp.state["A"].value,
                dsp.state["B"].value, dsp.state["AR2"].value,
                dsp.state["AR3"].value, dsp.state["AR4"].value,
                dsp.state["ST0"].value, dsp.state["ST1"].value,
            }
            if not helper_seen and offset == 0x2754 then
                helper_seen = true
                dump_program_tail("first-uploaded-helper", true)
                for _, bounds in ipairs({{0x246a, 0x2475}, {0x2754, 0x2780},
                        {0x282d, 0x2875}, {0x45c2, 0x45f8}}) do
                    for address = bounds[1], bounds[2] do
                        machine:logerror(string.format(
                            "nse5_compat_dsp_live_word: address=%04x word=%04x\n",
                            address, dsp.spaces["program"]:read_u16(address)))
                    end
                end
            end
            if previous_program_read and previous_program_read >= 0x0800 and offset < 0x0800 then
                dump_program_tail("low-program-entry")
            end
            previous_program_read = offset
        end)
    taps[#taps + 1] = program_tap
end
local dsp_publications = 0
local dsp_service_pulses = 0
local payload_writes = 0
local source_tail = {}
local transform_entries = 0
local nonlinear_input, nonlinear_base, nonlinear_count
nonlinear_count = 0
taps[#taps + 1] = dsp.spaces["program"]:install_read_tap(0x7fe8, 0x7fe8,
    "nse5_compat_nonlinear_input", function(offset, data, mask)
        if dsp.state["PC"].value ~= 0x7fe9 then return end
        nonlinear_base = dsp.state["AR4"].value
        nonlinear_input = {}
        for index = 0, 5 do
            nonlinear_input[#nonlinear_input + 1] = string.format("%04x", dsp_space:read_u16(nonlinear_base + index))
        end
    end)
taps[#taps + 1] = dsp.spaces["program"]:install_read_tap(0x7ff9, 0x7ff9,
    "nse5_compat_nonlinear_output", function(offset, data, mask)
        if dsp.state["PC"].value ~= 0x7ffa or not nonlinear_input then return end
        nonlinear_count = nonlinear_count + 1
        if nonlinear_count <= 128 then
            local output = {}
            for index = 0, 5 do
                output[#output + 1] = string.format("%04x", dsp_space:read_u16(nonlinear_base + index))
            end
            machine:logerror(string.format(
                "nse5_compat_nonlinear: input=%s output=%s t=%.9f\n",
                table.concat(nonlinear_input, ":"), table.concat(output, ":"), machine.time:as_double()))
        end
        nonlinear_input = nil
    end)
local reverse_input, reverse_base, reverse_count, round_count, codec_input, codec_base
reverse_count, round_count = 0, 0
for _, address in ipairs({0x7f3f, 0x7f45, 0x7f54}) do
    taps[#taps + 1] = dsp.spaces["program"]:install_read_tap(address, address,
        "nse5_compat_round_" .. string.format("%04x", address), function(offset, data, mask)
            if dsp.state["PC"].value ~= address + 1 then return end
            if address == 0x7f3f then
                round_count = 0
                codec_base = dsp.state["AR4"].value
                local words, key, schedule = {}, {}, {}
                for index = 0, 5 do
                    words[#words + 1] = string.format("%04x", dsp_space:read_u16(codec_base + index))
                    key[#key + 1] = string.format("%04x", dsp_space:read_u16(dsp.state["AR5"].value + index))
                end
                for index = 0, 11 do
                    schedule[#schedule + 1] = string.format("%04x", dsp_space:read_u16(dsp.state["AR6"].value + index))
                end
                codec_input = {table.concat(words, ":"), table.concat(key, ":"), table.concat(schedule, ":")}
            elseif address == 0x7f45 then round_count = round_count + 1
            else
                machine:logerror(string.format(
                    "nse5_compat_rounds: count=%d schedule=%04x base=%04x t=%.9f\n",
                    round_count, dsp.state["AR6"].value, dsp.state["AR4"].value,
                    machine.time:as_double()))
            end
        end)
end
taps[#taps + 1] = dsp.spaces["program"]:install_read_tap(0x8001, 0x8001,
    "nse5_compat_reverse_input", function(offset, data, mask)
        if dsp.state["PC"].value ~= 0x8002 then return end
        reverse_base = dsp.state["AR4"].value
        reverse_input = {}
        for index = 0, 5 do
            reverse_input[#reverse_input + 1] = string.format("%04x", dsp_space:read_u16(reverse_base + index))
        end
    end)
taps[#taps + 1] = dsp.spaces["program"]:install_read_tap(0x8014, 0x8014,
    "nse5_compat_reverse_output", function(offset, data, mask)
        if dsp.state["PC"].value ~= 0x8015 or not reverse_input then return end
        reverse_count = reverse_count + 1
        if reverse_count <= 32 then
            local output = {}
            for index = 0, 5 do
                output[#output + 1] = string.format("%04x", dsp_space:read_u16(reverse_base + index))
            end
            machine:logerror(string.format(
                "nse5_compat_reverse: base=%04x input=%s output=%s t=%.9f\n",
                reverse_base, table.concat(reverse_input, ":"), table.concat(output, ":"),
                machine.time:as_double()))
            if codec_input and reverse_base == codec_base then
                machine:logerror(string.format(
                    "nse5_compat_codec: input=%s table=%s schedule=%s output=%s t=%.9f\n",
                    codec_input[1], codec_input[2], codec_input[3], table.concat(output, ":"),
                    machine.time:as_double()))
                codec_input = nil
            end
        end
        reverse_input = nil
    end)
local mix_input, mix_destination, mix_count
mix_count = 0
taps[#taps + 1] = dsp.spaces["program"]:install_read_tap(0x7fb1, 0x7fb1,
    "nse5_compat_mix_input", function(offset, data, mask)
        if dsp.state["PC"].value ~= 0x7fb2 then return end
        local table_address = dsp.state["AR2"].value
        mix_input = {}
        for index = 0, 2 do
            local address = dsp_space:read_u16(table_address + index)
            for word = 0, 1 do
                mix_input[#mix_input + 1] = string.format("%04x", dsp_space:read_u16((address + word) & 0xffff))
            end
        end
        mix_destination = dsp_space:read_u16(table_address + 3)
    end)
taps[#taps + 1] = dsp.spaces["program"]:install_read_tap(0x7fe7, 0x7fe7,
    "nse5_compat_mix_output", function(offset, data, mask)
        if dsp.state["PC"].value ~= 0x7fe8 or not mix_input then return end
        mix_count = mix_count + 1
        if mix_count <= 128 then
            machine:logerror(string.format(
                "nse5_compat_mix: input=%s output=%04x:%04x t=%.9f\n",
                table.concat(mix_input, ":"), dsp_space:read_u16(mix_destination),
                dsp_space:read_u16((mix_destination + 1) & 0xffff), machine.time:as_double()))
        end
        mix_input = nil
    end)
local rotation_input, rotation_base, rotation_count, rotation_other, rotation_other_input
rotation_count = 0
taps[#taps + 1] = dsp.spaces["program"]:install_read_tap(0x7f7b, 0x7f7b,
    "nse5_compat_rotation_input", function(offset, data, mask)
        if dsp.state["PC"].value ~= 0x7f7c then return end
        rotation_base = dsp.state["AR2"].value
        rotation_other = dsp.state["AR3"].value
        rotation_input = {}
        rotation_other_input = {}
        for index = 0, 1 do
            rotation_input[#rotation_input + 1] = string.format("%04x", dsp_space:read_u16(rotation_base + index))
            rotation_other_input[#rotation_other_input + 1] = string.format("%04x", dsp_space:read_u16(rotation_other + index))
        end
    end)
taps[#taps + 1] = dsp.spaces["program"]:install_read_tap(0x7f8b, 0x7f8b,
    "nse5_compat_rotation_output", function(offset, data, mask)
        if dsp.state["PC"].value ~= 0x7f8c or not rotation_input then return end
        rotation_count = rotation_count + 1
        if rotation_count <= 128 then
            local output, other_output = {}, {}
            for index = 0, 1 do
                output[#output + 1] = string.format("%04x", dsp_space:read_u16(rotation_base + index))
                other_output[#other_output + 1] = string.format("%04x", dsp_space:read_u16(rotation_other + index))
            end
            machine:logerror(string.format(
                "nse5_compat_rotation: base=%04x input=%s output=%s other=%04x other_input=%s other_output=%s t=%.9f\n",
                rotation_base, table.concat(rotation_input, ":"), table.concat(output, ":"),
                rotation_other, table.concat(rotation_other_input, ":"), table.concat(other_output, ":"),
                machine.time:as_double()))
        end
        rotation_input = nil
    end)
taps[#taps + 1] = dsp.spaces["program"]:install_read_tap(0x7f59, 0x7f59,
    "nse5_compat_transform_table", function(offset, data, mask)
        if dsp.state["PC"].value ~= 0x7f5a then return end
        local words = {}
        for address = 0x13dc, 0x13e1 do
            words[#words + 1] = string.format("%04x", dsp_space:read_u16(address))
        end
        machine:logerror(string.format(
            "nse5_compat_transform_table: words=%s ar2=%04x ar3=%04x ar4=%04x ar5=%04x t=%.9f\n",
            table.concat(words, ":"), dsp.state["AR2"].value, dsp.state["AR3"].value,
            dsp.state["AR4"].value, dsp.state["AR5"].value, machine.time:as_double()))
    end)
taps[#taps + 1] = dsp.spaces["program"]:install_read_tap(0x7f2d, 0x7f2d,
    "nse5_compat_transform_input", function(offset, data, mask)
        if dsp.state["PC"].value ~= 0x7f2e then return end
        transform_entries = transform_entries + 1
        if transform_entries > 8 then return end
        local registers, words, serial_words = {}, {}, {}
        for index = 0, 7 do
            registers[#registers + 1] = string.format("%04x", dsp.state["AR" .. index].value)
        end
        for address = 0x1200, 0x121f do
            words[#words + 1] = string.format("%04x", dsp_space:read_u16(address))
        end
        for address = 0x1f0c, 0x1f0f do
            serial_words[#serial_words + 1] = string.format("%04x", dsp_space:read_u16(address))
        end
        machine:logerror(string.format(
            "nse5_compat_transform_input: entry=%d ar=%s buffer=%s serial_words=%s st0=%04x st1=%04x t=%.9f\n",
            transform_entries, table.concat(registers, ":"), table.concat(words, ":"),
            table.concat(serial_words, ":"),
            dsp.state["ST0"].value, dsp.state["ST1"].value, machine.time:as_double()))
    end)
taps[#taps + 1] = dsp_space:install_write_tap(0x1200, 0x121f,
    "nse5_compat_response_buffer", function(offset, data, mask)
        if machine.time:as_double() > 0.674 then return end
        if offset == 0x120e then
            local input, table_words = {}, {}
            for address = 0x0820, 0x0838 do
                input[#input + 1] = string.format("%04x", dsp_space:read_u16(address))
            end
            for address = 0xb0bc, 0xb0cb do
                table_words[#table_words + 1] = string.format("%04x", dsp_space:read_u16(address))
            end
            machine:logerror(string.format(
                "nse5_compat_transform_source: value=%04x pc=%04x ar1=%04x ar2=%04x ar3=%04x hpi=%s table=%s t=%.9f\n",
                data, dsp.state["PC"].value, dsp.state["AR1"].value,
                dsp.state["AR2"].value, dsp.state["AR3"].value,
                table.concat(input, ":"), table.concat(table_words, ":"),
                machine.time:as_double()))
        end
        source_tail[#source_tail + 1] = string.format(
            "nse5_compat_response_buffer: address=%04x value=%04x pc=%04x sp=%04x t=%.9f\n",
            offset, data, dsp.state["PC"].value, dsp.state["SP"].value,
            machine.time:as_double())
        if #source_tail > 64 then table.remove(source_tail, 1) end
    end)
taps[#taps + 1] = dsp_space:install_write_tap(0x088c, 0x088c,
    "nse5_compat_payload_source", function(offset, data, mask)
        payload_writes = payload_writes + 1
        if payload_writes > 16 then return end
        for _, line in ipairs(source_tail) do machine:logerror(line) end
        local registers = {}
        for index = 0, 7 do
            registers[#registers + 1] = string.format("%04x", dsp.state["AR" .. index].value)
        end
        machine:logerror(string.format(
            "nse5_compat_payload_source: value=%04x pc=%04x sp=%04x ar=%s a=%010x b=%010x t=%.9f\n",
            data, dsp.state["PC"].value, dsp.state["SP"].value,
            table.concat(registers, ":"), dsp.state["A"].value,
            dsp.state["B"].value, machine.time:as_double()))
    end)
taps[#taps + 1] = dsp_space:install_write_tap(0x0029, 0x0029,
    "nse5_compat_dsp_service_register", function(offset, data, mask)
        dsp_service_pulses = dsp_service_pulses + 1
        if dsp_service_pulses <= 64 then
            machine:logerror(string.format(
                "nse5_compat_dsp_service_register: value=%04x pc=%04x request=%04x status=%04x t=%.6f\n",
                data, dsp.state["PC"].value, dsp_space:read_u16(0x0871),
                dsp_space:read_u16(0x0872), machine.time:as_double()))
        end
    end)
taps[#taps + 1] = dsp_space:install_write_tap(0x08e4, 0x08e4,
    "nse5_compat_dsp_publication", function(offset, data, mask)
        dsp_publications = dsp_publications + 1
        if dsp_publications <= 128 then
            local stack = {}
            for index = 0, 15 do
                stack[#stack + 1] = string.format("%04x", dsp_space:read_u16(
                    (dsp.state["SP"].value + index) & 0xffff))
            end
            machine:logerror(string.format(
                "nse5_compat_dsp_publication: producer=%04x pc=%04x sp=%04x t=%.6f stack=%s\n",
                data, dsp.state["PC"].value, dsp.state["SP"].value,
                machine.time:as_double(), table.concat(stack, ":")))
        end
    end)
emu.register_frame_done(function()
    -- Retain subscriptions for the whole run. A local table not captured by
    -- a live callback can be collected while the CPU is executing a tap.
    assert(#taps == #entries + 24 + (capture_tail and 3 or 0), "entry trace subscriptions lost")
    if capture_tail and not tail_dumped and dsp.state["ILLEGAL"].value ~= 0 then
        dump_program_tail("illegal-opcode")
    end
    if capture_tail and not tail_dumped and machine.time:as_double() >= 0.55 then
        dump_program_tail("bounded-upload-window")
    end
    if program_tap and tail_dumped then
        program_tap:remove()
        program_tap = nil
    end
    if menu_fixture then
        local now = machine.time:as_double()
        if (menu_step == 0 and now >= 4.2) or (menu_step == 1 and now >= 4.4) then
            local key
            for _, field in pairs(machine.ioport.ports[":COL.2"].fields) do
                if field.mask == 1 then key = field end
            end
            assert(key, "NSE-5 physical Menu switch missing")
            key:set_value(menu_step == 0 and 1 or 0)
            machine:logerror(string.format("nse5_compat_menu: pressed=%d t=%.6f\n",
                menu_step == 0 and 1 or 0, now))
            menu_step = menu_step + 1
        end
    end
    if next_sample > #times or machine.time:as_double() < times[next_sample] then return end
    machine.screens[":screen"]:snapshot(string.format("native_%02d.png", next_sample))
    machine:logerror(string.format(
        "nse5_compat_sample: t=%.6f pc=%08x result0=%04x result1=%04x idle=%02x irq=%02x irq_mask=%02x col_mask=%02x ready=%02x mode=%04x\n",
        machine.time:as_double(), cpu.state["PC"].value,
        space:read_u16(0x167036), space:read_u16(0x167038), space:read_u8(0x168f04),
        space:read_u8(0x20009), space:read_u8(0x2000b), space:read_u8(0x2006b),
        space:read_u8(0x16ab85), space:read_u16(0x1689e4)))
    next_sample = next_sample + 1
    if next_sample > #times then
        machine:logerror(string.format("nse5_compat_column_writes: count=%d\n", column_writes))
        local checklist = {}
        for index = 0, 10 do
            checklist[#checklist + 1] = string.format("%02x", space:read_u8(0x16a2b4 + index))
        end
        machine:logerror("nse5_compat_subsystem_checklist: bytes=" .. table.concat(checklist) .. "\n")
        for _, entry in ipairs(entries) do
            machine:logerror(string.format("nse5_compat_entries: name=%s count=%d\n",
                entry[2], entry_counts[entry[2]]))
        end
    end
end, "NSE-5 ROM4 compatibility observation")

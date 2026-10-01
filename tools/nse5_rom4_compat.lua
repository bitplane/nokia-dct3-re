-- Observation-only: this composition tests compatibility, not fitted silicon.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local space = cpu.spaces.program
local next_sample = 1
local times = {0.1, 0.5, 1, 2, 4, 8}
local taps, entry_counts = {}, {}
local menu_fixture = os.getenv("NSE5_COMPAT_MENU") == "1"
local menu_step = 0
local entries = {
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
}
for _, entry in ipairs(entries) do
    local address, name = entry[1], entry[2]
    entry_counts[name] = 0
    taps[#taps + 1] = space:install_read_tap(address & ~3, (address & ~3) + 3,
        "nse5_compat_" .. name, function(offset, data, mask)
            local pc = cpu.state["PC"].value
            if pc ~= address then return end
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
                if name == "subsystem_group_skip" or name == "subsystem_group_release" then
                    local r5 = cpu.state["R5"].value
                    machine:logerror(string.format(
                        "nse5_compat_release: path=%s bootstrap=%02x mode=%02x r5=%08x r5_byte0=%02x r5_byte1=%02x predicate=%02x t=%.6f\n",
                        name, space:read_u8(0x16702c), space:read_u8(0x16ab88),
                        r5, space:read_u8(r5), space:read_u8(r5 + 1),
                        space:read_u8(0x17fe15), machine.time:as_double()))
                end
                if name == "dsp_control_receive" or name == "startup_selftest_reply" or
                        name == "task2_message_received" then
                    local message = name ~= "startup_selftest_reply" and
                        cpu.state["R0"].value or cpu.state["R4"].value
                    local bytes = {}
                    for index = 0, 11 do
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
emu.register_frame_done(function()
    -- Retain subscriptions for the whole run. A local table not captured by
    -- a live callback can be collected while the CPU is executing a tap.
    assert(#taps == #entries + 3, "entry trace subscriptions lost")
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

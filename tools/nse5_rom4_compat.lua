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
}
for _, entry in ipairs(entries) do
    local address, name = entry[1], entry[2]
    entry_counts[name] = 0
    taps[#taps + 1] = space:install_read_tap(address & ~3, (address & ~3) + 3,
        "nse5_compat_" .. name, function(offset, data, mask)
            local pc = cpu.state["PC"].value
            if pc ~= address then return end
            entry_counts[name] = entry_counts[name] + 1
            if entry_counts[name] <= (name == "startup_index" and 64 or 12) or
                    (name == "key_decoder" and cpu.state["R0"].value ~= 0xff) then
                machine:logerror(string.format(
                    "nse5_compat_entry: name=%s pc=%08x r0=%08x r1=%08x lr=%08x t=%.6f\n",
                    name, pc, cpu.state["R0"].value, cpu.state["R1"].value,
                    cpu.state["R14"].value, machine.time:as_double()))
            end
        end)
end
emu.register_frame_done(function()
    -- Retain subscriptions for the whole run. A local table not captured by
    -- a live callback can be collected while the CPU is executing a tap.
    assert(#taps == #entries, "entry trace subscriptions lost")
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
        "nse5_compat_sample: t=%.6f pc=%08x result0=%04x result1=%04x idle=%02x irq=%02x irq_mask=%02x col_mask=%02x\n",
        machine.time:as_double(), cpu.state["PC"].value,
        space:read_u16(0x167036), space:read_u16(0x167038), space:read_u8(0x168f04),
        space:read_u8(0x20009), space:read_u8(0x2000b), space:read_u8(0x2006b)))
    next_sample = next_sample + 1
    if next_sample > #times then
        for _, entry in ipairs(entries) do
            machine:logerror(string.format("nse5_compat_entries: name=%s count=%d\n",
                entry[2], entry_counts[entry[2]]))
        end
    end
end, "NSE-5 ROM4 compatibility observation")

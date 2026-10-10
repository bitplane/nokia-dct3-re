-- Passive native ROM4 control-state census; no firmware or I/O writes.
local machine = manager.machine
local cpu = assert(machine.devices[":dsp_c54x:cpu"])
local memory = cpu.spaces["data"]
local addresses = {0x121f, 0x1835, 0x1836, 0x1869, 0x07fb, 0x1974, 0x00ac, 0x1973, 0x06bc, 0x06be, 0x06e3,
    0x00b1, 0x1949, 0x194c, 0x1951, 0x195b, 0x194f, 0x1950, 0x11fd}
local bounded_addresses = {[0x00b1] = true, [0x194c] = true,
    [0x1951] = true, [0x195b] = true,
    [0x194f] = true, [0x1950] = true, [0x11fd] = true}
local taps = {}
local write_counts = {}
for _, address in ipairs(addresses) do
    taps[#taps + 1] = memory:install_write_tap(address, address,
        string.format("mode_%04x", address), function(offset, data, mask)
            local pc = cpu.state["PC"].value
            local key = string.format("%04x:%04x", offset, pc)
            write_counts[key] = (write_counts[key] or 0) + 1
            if bounded_addresses[offset] and write_counts[key] > 4 then return end
            if offset >= 0x06bc and offset <= 0x06e3 and write_counts[key] > 4 then return end
            machine:logerror(string.format(
                "rom4_mode_write: t=%.9f pc=%06x address=%04x data=%04x mask=%04x\n",
                machine.time:as_double(), cpu.state["PC"].value, offset, data, mask))
        end)
end
-- Include a known executed copy instruction so zero candidate counts are
-- not mistaken for evidence when opcode-fetch observation is unavailable.
local program = cpu.spaces["program"]
local sites = {0x23d0, 0x2626, 0x4ef9, 0x5006, 0x3da2,
    0x09a4, 0x0ab8, 0x4de3, 0x4e00, 0x9b62, 0x9b9f, 0x9bb0,
    0x3660, 0x3d5e, 0x3d70, 0x3db8, 0x4dce, 0x30a1, 0x30bf, 0x30d6,
    0x3204, 0x3268, 0x3274, 0x32c3, 0x33f3,
    0x4414, 0x4428, 0x4433, 0x4460, 0x32f4, 0x33b4, 0x3249,
    0x330a, 0x3326, 0x332b, 0x3347, 0x3360, 0x337c, 0x33ac, 0x33b0, 0x42f6,
    0x33e9, 0x4424, 0x4c6f, 0x4ef7, 0x4f20, 0x50db,
    0x9a7a, 0x9aa8, 0x9b15, 0x9a99, 0x9acc, 0x9b20,
    0x98b7, 0x9903, 0x55bb, 0x5783, 0x578f, 0x579b, 0x57a3,
    0x5514, 0x5516, 0x5536, 0x5554, 0x6943,
    0x5e62, 0x5e6a, 0x545d, 0x5463, 0x5eed}
local counts = {}
local control_sites = {
    [0x09a4] = true, [0x0ab8] = true, [0x4de3] = true, [0x4e00] = true,
    [0x9b62] = true, [0x9b9f] = true, [0x9bb0] = true,
    [0x3660] = true, [0x3d5e] = true, [0x3d70] = true, [0x3db8] = true,
    [0x4dce] = true,
    [0x30bf] = true, [0x30d6] = true,
}
local packet_count = 0
local upload_reads = {}
taps[#taps + 1] = memory:install_write_tap(0x22c9, 0x22c9,
    "rom4_upload_word_publication", function(offset, value, mask)
        machine:logerror(string.format(
            "rom4_upload_word_write: t=%.9f pc=%04x address=%04x data=%04x mask=%04x\n",
            machine.time:as_double(), cpu.state["PC"].value, offset, value, mask))
    end)
taps[#taps + 1] = memory:install_read_tap(0x22c9, 0x22c9,
    "rom4_upload_word_reader", function(offset, value)
        if _G.rom4_observation_read then return end
        local pc = cpu.state["PC"].value
        upload_reads[pc] = (upload_reads[pc] or 0) + 1
        if upload_reads[pc] <= 2 then
            machine:logerror(string.format(
                "rom4_upload_word_read: t=%.9f pc=%04x address=%04x data=%04x\n",
                machine.time:as_double(), pc, offset, value))
        end
    end)
taps[#taps + 1] = memory:install_write_tap(0x0853, 0x0853,
    "rom4_host_consumption", function(offset, value, mask)
        local previous = memory:read_u16(offset)
        if previous == value or previous >= 0x52 or packet_count >= 128 then return end
        packet_count = packet_count + 1
        local header = memory:read_u16(0x0800 + previous)
        local payload = {}
        local bytes = header >> 8
        if bytes <= 0x80 and bytes % 2 == 0 then
            for index = 1, bytes // 2 do
                payload[#payload + 1] = string.format("%04x",
                    memory:read_u16(0x0800 + ((previous + index) % 0x52)))
            end
        end
        machine:logerror(string.format(
            "rom4_host_consumption: t=%.9f pc=%04x previous=%04x next=%04x header=%04x words=%s\n",
            machine.time:as_double(), cpu.state["PC"].value, previous, value,
            header, table.concat(payload, ",")))
    end)
for _, address in ipairs(sites) do
    counts[address] = 0
    taps[#taps + 1] = program:install_read_tap(address, address,
        string.format("dispatch_%04x", address), function(offset, data)
            local pc = cpu.state["PC"].value
            if pc == offset or pc == offset + 1 then
                counts[address] = counts[address] + 1
                if control_sites[address] and counts[address] <= 4 then
                    machine:logerror(string.format(
                        "rom4_control_fetch: t=%.9f address=%04x mode=%04x\n",
                        machine.time:as_double(), offset, memory:read_u16(0x00ac)))
                end
                if address >= 0x330a and address <= 0x33b0 and counts[address] <= 2 then
                    machine:logerror(string.format(
                        "rom4_mode1_fetch: t=%.9f address=%04x word=%04x fd=%04x bc=%04x high94=%04x high96=%04x\n",
                        machine.time:as_double(), offset, data, memory:read_u16(0x06fd),
                        memory:read_u16(0x06bc), memory:read_u16(0x2194), memory:read_u16(0x2196)))
                end
            end
        end)
end
local stop = emu.add_machine_stop_notifier(function()
    for _, start in ipairs({0xee38, 0xee3e, 0xee45, 0xee4e, 0xee57}) do
        local words = {}
        local terminated = false
        for index = 0, 31 do
            local word = memory:read_u16(start + index)
            words[#words + 1] = string.format("%04x", word)
            if word == 0x00ff then terminated = true; break end
        end
        machine:logerror(string.format(
            "rom4_schedule_list: address=%04x terminated=%d words=%s\n",
            start, terminated and 1 or 0, table.concat(words, ",")))
    end
    for pc, count in pairs(upload_reads) do
        machine:logerror(string.format(
            "rom4_upload_reader_count: pc=%04x count=%d\n", pc, count))
    end
    for _, address in ipairs(addresses) do
        machine:logerror(string.format("rom4_mode_final: address=%04x data=%04x\n",
            address, memory:read_u16(address)))
    end
    for _, address in ipairs(sites) do
        machine:logerror(string.format("rom4_dispatch_count: address=%04x count=%d\n",
            address, counts[address]))
    end
end)
assert(#taps == #addresses + #sites + 3 and stop)
_G.rom4_mode_observer = {taps = taps, stop = stop}

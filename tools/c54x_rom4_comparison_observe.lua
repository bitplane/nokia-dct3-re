-- Passive full-width comparison operands; CPU-bus writes do not cover uploads.
local machine = manager.machine
local cpu = assert(machine.devices[":dsp_c54x:cpu"])
local program, memory = cpu.spaces["program"], cpu.spaces["data"]
local taps, counts = {}, {}
for _, address in ipairs({0x2194, 0x2195, 0x2196, 0x2197, 0x06fd}) do
    counts[address] = 0
    taps[#taps + 1] = memory:install_write_tap(address, address,
        string.format("comparison_write_%04x", address), function(offset, data)
            counts[address] = counts[address] + 1
            if counts[address] <= 4 then
                machine:logerror(string.format("rom4_comparison_write: t=%.9f pc=%04x address=%04x data=%04x\n",
                    machine.time:as_double(), cpu.state["PC"].value, offset, data))
            end
        end)
end
for _, address in ipairs({0x3347, 0x3357, 0x3360, 0x3362, 0x336c, 0x336e,
    0x2400, 0x2402, 0x3382, 0x3385, 0x33a1, 0x33a4, 0x33ac}) do
    counts[address] = 0
    taps[#taps + 1] = program:install_read_tap(address, address,
        string.format("comparison_fetch_%04x", address), function(offset, data)
            local pc = cpu.state["PC"].value
            if pc == offset or pc == offset + 1 then
                counts[address] = counts[address] + 1
                if counts[address] <= 4 then
                    machine:logerror(string.format(
                        "rom4_comparison_fetch: t=%.9f address=%04x word=%04x pair94=%04x%04x pair96=%04x%04x a=%010x b=%010x ar2=%04x ar3=%04x\n",
                        machine.time:as_double(), offset, data, memory:read_u16(0x2194),
                        memory:read_u16(0x2195), memory:read_u16(0x2196), memory:read_u16(0x2197),
                        cpu.state["A"].value, cpu.state["B"].value,
                        cpu.state["AR2"].value, cpu.state["AR3"].value))
                    if address == 0x3347 then
                        local base, values = cpu.state["AR2"].value, {}
                        for index = 0, 15 do
                            values[#values + 1] = string.format("%04x", memory:read_u16((base + index) & 0xffff))
                        end
                        machine:logerror(string.format("rom4_reduction_input: base=%04x words=%s\n",
                            base, table.concat(values, ",")))
                    end
                end
            end
        end)
end
local stop = emu.add_machine_stop_notifier(function()
    for address, count in pairs(counts) do
        machine:logerror(string.format("rom4_comparison_count: address=%04x count=%d\n", address, count))
    end
end)
_G.rom4_comparison_observer = {taps = taps, stop = stop}

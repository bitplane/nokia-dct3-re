-- Passive native ROM4 control-state census; no firmware or I/O writes.
local machine = manager.machine
local cpu = assert(machine.devices[":dsp_c54x:cpu"])
local memory = cpu.spaces["data"]
local addresses = {0x121f, 0x1835, 0x07fb, 0x1974, 0x00ac, 0x1973}
local taps = {}
for _, address in ipairs(addresses) do
    taps[#taps + 1] = memory:install_write_tap(address, address,
        string.format("mode_%04x", address), function(offset, data, mask)
            machine:logerror(string.format(
                "rom4_mode_write: t=%.9f pc=%06x address=%04x data=%04x mask=%04x\n",
                machine.time:as_double(), cpu.state["PC"].value, offset, data, mask))
        end)
end
-- Include a known executed copy instruction so zero candidate counts are
-- not mistaken for evidence when opcode-fetch observation is unavailable.
local program = cpu.spaces["program"]
local sites = {0x23d0, 0x2626, 0x4ef9, 0x5006, 0x3da2,
    0x4414, 0x4428, 0x4433, 0x4460, 0x32f4, 0x33b4, 0x3249}
local counts = {}
for _, address in ipairs(sites) do
    counts[address] = 0
    taps[#taps + 1] = program:install_read_tap(address, address,
        string.format("dispatch_%04x", address), function(offset, data)
            local pc = cpu.state["PC"].value
            if pc == offset or pc == offset + 1 then
                counts[address] = counts[address] + 1
            end
        end)
end
local stop = emu.add_machine_stop_notifier(function()
    for _, address in ipairs(addresses) do
        machine:logerror(string.format("rom4_mode_final: address=%04x data=%04x\n",
            address, memory:read_u16(address)))
    end
    for _, address in ipairs(sites) do
        machine:logerror(string.format("rom4_dispatch_count: address=%04x count=%d\n",
            address, counts[address]))
    end
end)
assert(#taps == #addresses + #sites and stop)
_G.rom4_mode_observer = {taps = taps, stop = stop}

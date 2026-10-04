-- Passive observation after native loader isolation; never supplies a reply.
local machine = manager.machine
local cpu = assert(machine.devices[":maincpu"])
local memory = cpu.spaces["program"]
local taps = {}
taps[#taps + 1] = memory:install_write_tap(0x30000, 0x30003,
    "6250_runtime_doorbell", function(offset, value, mask)
        if machine.time:as_double() < 1.898 then return end
        machine:logerror(string.format(
            "6250_runtime_doorbell: data=%08x mask=%08x pc=%08x command=%04x argument=%04x pending=%04x t=%.6f\n",
            value, mask, cpu.state["PC"].value, memory:read_u16(0x100a8),
            memory:read_u16(0x100b8), memory:read_u16(0x100e0), machine.time:as_double()))
    end)
local captured = false
emu.register_periodic(function()
    if captured or machine.time:as_double() < 8 then return end
    captured = true
    local dsp = assert(machine.devices[":dsp_staged:cpu"])
    machine:logerror(string.format(
        "6250_runtime_boundary: arm_pc=%08x dsp_pc=%04x pending=%04x result=%04x/%04x t=%.6f\n",
        cpu.state["PC"].value, dsp.state["PC"].value, memory:read_u16(0x100e0),
        memory:read_u16(0x10000), memory:read_u16(0x10002), machine.time:as_double()))
    machine.screens[":screen"]:snapshot("6250_runtime.png")
end)
_G.noki6250_runtime_taps = taps

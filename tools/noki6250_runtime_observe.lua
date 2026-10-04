-- Passive observation after native loader isolation; never supplies a reply.
local machine = manager.machine
local cpu = assert(machine.devices[":maincpu"])
local memory = cpu.spaces["program"]
local taps = {}
local receivers = {}
taps[#taps + 1] = memory:install_read_tap(0x304494, 0x304497,
    "6250_service_control_consumer", function(offset, value, mask)
        if cpu.state["PC"].value ~= 0x304494 then return end
        local address = cpu.state["R0"].value
        if address < 0x100000 or address + 10 > 0x180000 then return end
        machine:logerror(string.format(
            "6250_service_control_consumer: class=%02x command=%02x status=%02x armed=%02x t=%.6f\n",
            memory:read_u8(address + 3), memory:read_u8(address + 8),
            memory:read_u8(address + 9), memory:read_u8(0x17fd15), machine.time:as_double()))
    end)
taps[#taps + 1] = memory:install_read_tap(0x3c363c, 0x3c363f,
    "6250_service_receiver", function(offset, value, mask)
        if cpu.state["PC"].value ~= 0x3c363c or memory:read_u8(0x100022) ~= 2 then return end
        local caller = cpu.state["R14"].value
        if receivers[caller] then return end
        receivers[caller] = true
        machine:logerror(string.format("6250_service_receiver: task=2 caller=%08x t=%.6f\n",
            caller, machine.time:as_double()))
    end)
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
    machine:logerror(string.format("6250_service_control_endpoint: flags=%02x fault0=%02x fault1=%02x\n",
        memory:read_u8(0x17fd15), memory:read_u8(0x17fbf0), memory:read_u8(0x17fbf1)))
    machine.screens[":screen"]:snapshot("6250_runtime.png")
end)
_G.noki6250_runtime_taps = taps

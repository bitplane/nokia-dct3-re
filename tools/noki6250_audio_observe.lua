-- Passive shared-control provenance during the physical incoming-call probe.
local source = debug.getinfo(1, "S").source:sub(2)
local directory = assert(source:match("^(.*[/])"))
dofile(directory .. "noki6250_call_observe.lua")
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local memory = cpu.spaces["program"]
_G.noki6250_audio_tap = memory:install_write_tap(0x100a8, 0x100ab, "6250_audio_control",
    function(offset, data, mask)
        machine:logerror(string.format(
            "6250_audio_control: address=%08x data=%08x mask=%08x pc=%08x caller=%08x r0=%08x r1=%08x r2=%08x t=%.6f\n",
            offset, data, mask, cpu.state["PC"].value, cpu.state["R14"].value,
            cpu.state["R0"].value, cpu.state["R1"].value, cpu.state["R2"].value,
            machine.time:as_double()))
    end)
_G.noki6250_audio_helper_tap = memory:install_read_tap(0x429a34, 0x429a37,
    "6250_audio_helper", function()
        if cpu.state["PC"].value ~= 0x429a34 or cpu.state["R0"].value ~= 8 then return end
        machine:logerror(string.format(
            "6250_audio_helper: command=%02x value=%04x commit=%x caller=%08x t=%.6f\n",
            cpu.state["R0"].value, cpu.state["R1"].value, cpu.state["R2"].value,
            cpu.state["R14"].value, machine.time:as_double()))
    end)
_G.noki6250_audio_field_tap = memory:install_read_tap(0x3fbc54, 0x3fbc57,
    "6250_audio_field", function()
        if cpu.state["PC"].value ~= 0x3fbc54 then return end
        local context = cpu.state["R4"].value
        local desired = cpu.state["R5"].value
        local selector = memory:read_u8(context + 0x10)
        machine:logerror(string.format(
            "6250_audio_field: context=%08x desired=%08x selector=%02x keep=%04x add=%04x before=%04x t=%.6f\n",
            context, desired, selector, memory:read_u16(0x263160),
            memory:read_u16(0x263154 + selector*2), memory:read_u16(desired),
            machine.time:as_double()))
    end)

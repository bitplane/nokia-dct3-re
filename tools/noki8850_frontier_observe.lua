-- Passive stock-input sleep/frontier observation; no firmware or MMIO writes.
local machine = manager.machine
local cpu = assert(machine.devices[":maincpu"])
local memory = cpu.spaces["program"]
local taps = {}
local counts = {flag = 0, control = 0}
taps[#taps + 1] = memory:install_write_tap(0x1381ec, 0x1381ef,
    "8850_frontier_flag", function(address, data, mask)
        if counts.flag >= 24 then return end
        counts.flag = counts.flag + 1
        machine:logerror(string.format(
            "8850_flag_write: address=%08x data=%08x mask=%08x pc=%08x lr=%08x t=%.6f\n",
            address, data, mask, cpu.state["PC"].value,
            cpu.state["LR"].value, machine.time:as_double()))
    end)
taps[#taps + 1] = memory:install_write_tap(0x2000c, 0x2000f,
    "8850_frontier_control", function(address, data, mask)
        if counts.control >= 24 then return end
        counts.control = counts.control + 1
        machine:logerror(string.format(
            "8850_control_write: data=%08x mask=%08x pc=%08x t=%.6f\n",
            data, mask, cpu.state["PC"].value, machine.time:as_double()))
    end)
local samples = coroutine.create(function()
    local previous = 0
    for _, time in ipairs({0.1, 0.5, 1, 2, 4, 8}) do
        if not emu.wait(time - previous) then return end
        previous = time
        machine:logerror(string.format(
            "8850_frontier: pc=%08x r5=%08x r6=%08x flag=%02x fiq=%02x mask=%02x ctrl=%02x timer0=%04x compare=%04x t=%.6f\n",
            cpu.state["PC"].value, cpu.state["R5"].value,
            cpu.state["R6"].value, memory:read_u8(0x1381ec),
            memory:read_u8(0x20008), memory:read_u8(0x2000a),
            memory:read_u8(0x2000c), memory:read_u16(0x20010),
            memory:read_u16(0x20012),
            machine.time:as_double()))
        machine.screens[":screen"]:snapshot(string.format("8850_%04d.png", time * 1000))
    end
end)
_G.noki8850_frontier_taps = taps
_G.noki8850_frontier_samples = samples
assert(coroutine.resume(samples))

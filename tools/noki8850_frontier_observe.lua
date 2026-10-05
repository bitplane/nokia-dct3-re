-- Passive stock-input sleep/frontier observation; no firmware or MMIO writes.
local machine = manager.machine
local cpu = assert(machine.devices[":maincpu"])
local memory = cpu.spaces["program"]
local taps = {}
local counts = {flag = 0, control = 0}
local nv_hits = 0
taps[#taps + 1] = memory:install_read_tap(0x2408c8, 0x2408cb,
    "8850_nv_entry", function(address, data, mask)
        if nv_hits >= 8 then return end
        nv_hits = nv_hits + 1
        machine:logerror(string.format("8850_nv_entry: pc=%08x lr=%08x t=%.6f\n",
            cpu.state["PC"].value, cpu.state["LR"].value, machine.time:as_double()))
    end)
taps[#taps + 1] = memory:install_read_tap(0x240c00, 0x240c03,
    "8850_nv_failure", function(address, data, mask)
        if cpu.state["PC"].value ~= 0x240c02 then return end
        local stack = cpu.state["SP"].value
        machine:logerror(string.format(
            "8850_nv_failure: computed=%04x stored=%04x companion=%04x faults=%08x t=%.6f\n",
            cpu.state["R9"].value & 0xffff, memory:read_u16(stack + 4),
            memory:read_u16(stack + 6), cpu.state["R6"].value,
            machine.time:as_double()))
    end)
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
    for _, time in ipairs({0.1, 0.5, 1, 2, 4, 8, 10, 16, 24, 32}) do
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
        local faults = {}
        for index = 0, 23 do
            faults[#faults + 1] = string.format("%02x", memory:read_u8(0x13fbe0 + index))
        end
        machine:logerror("8850_faults: bytes=" .. table.concat(faults) .. "\n")
        machine:logerror(string.format(
            "8850_ready: state=%02x shared_e4=%04x identity=%04x result=%04x t=%.6f\n",
            memory:read_u8(0x135664), memory:read_u16(0x100e4),
            memory:read_u16(0x10004), memory:read_u16(0x10002),
            machine.time:as_double()))
        machine:logerror(string.format(
            "8850_boot_state: selector=%02x source=%02x power_state=%02x t=%.6f\n",
            memory:read_u8(0x137fe0), memory:read_u8(0x13fec1),
            memory:read_u8(0x13ff00), machine.time:as_double()))
        local timer = 0x11174c + 0x51 * 12
        machine:logerror(string.format(
            "8850_ui_timer51: link=%08x delta=%04x owner=%02x flags=%02x state=%02x event=%04x t=%.6f\n",
            memory:read_u32(timer), memory:read_u16(timer + 4),
            memory:read_u8(timer + 6), memory:read_u8(timer + 7),
            memory:read_u8(timer + 8), memory:read_u16(timer + 10),
            machine.time:as_double()))
        machine.screens[":screen"]:snapshot(string.format("8850_%04d.png", time * 1000))
    end
end)
_G.noki8850_frontier_taps = taps
_G.noki8850_frontier_samples = samples
assert(coroutine.resume(samples))

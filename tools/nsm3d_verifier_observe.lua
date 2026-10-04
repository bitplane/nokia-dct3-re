-- Read-only NSM-3D v5.02 observation; this never supplies a DSP verdict.
local machine = manager.machine
local cpu = assert(machine.devices[":maincpu"])
local memory = cpu.spaces["program"]
local transfers = { [0x100fe] = 0, [0x10100] = 0 }
local order_errors, last = 0, nil
local release_tap = memory:install_write_tap(0x20000, 0x20003,
    "nsm3d_dsp_release", function(offset, data, mask)
        if offset ~= 0x20000 or (mask & 0x0000ff00) == 0 then return end
        local pc = cpu.state["PC"].value
        machine:logerror(string.format(
            "nsm3d_release: pc=%08x control=%02x result0=%04x result1=%04x retained0=%04x retained1=%04x pairs0=%d pairs1=%d order_errors=%d t=%.6f\n",
            pc, (data >> 8) & 0xff,
            memory:read_u16(0x10000), memory:read_u16(0x10002),
            memory:read_u16(0x12f026), memory:read_u16(0x12f028),
            transfers[0x100fe], transfers[0x10100], order_errors,
            machine.time:as_double()))
        if pc == 0x2cb4c0 then
            local descriptor = memory:read_u32(0x12f040)
            local fields = {}
            for index = 0, 5 do
                fields[#fields + 1] = string.format("%04x", memory:read_u16(descriptor + index * 2))
            end
            machine:logerror(string.format("nsm3d_loader_descriptor: address=%08x fields=%s\n",
                descriptor, table.concat(fields, "/")))
        end
    end)
-- The ARM program space is 32-bit, big-endian; observe both halfword lanes.
local tap = memory:install_write_tap(0x100fc, 0x10103,
    "nsm3d_ownership", function(offset, data, mask)
        local address, lane
        if offset == 0x100fc then address, lane = 0x100fe, 0x0000ffff
        elseif offset == 0x10100 then address, lane = 0x10100, 0xffff0000 end
        if address and (mask & lane) == lane and (data & lane) == 0 then
            transfers[address] = transfers[address] + 1
            if last == address then order_errors = order_errors + 1 end
            last = address
        end
    end)
local sample = coroutine.create(function()
    if not emu.wait(0.5) then return end
    local words = {}
    for index = 0, 222 do
        words[#words + 1] = string.format("%04x", memory:read_u16(0x11e00 + index * 2))
    end
    machine:logerror("nsm3d_verifier_program: words=" .. table.concat(words) .. "\n")
    for address = 0x110f6, 0x11102, 2 do
        machine:logerror(string.format("nsm3d_verifier_input: address=%08x value=%04x\n",
            address, memory:read_u16(address)))
    end
    if not emu.wait(7.5) then return end
    machine:logerror(string.format(
        "nsm3d_verifier_boundary: pc=%08x result0=%04x result1=%04x pairs0=%d pairs1=%d order_errors=%d\n",
        cpu.state["PC"].value, memory:read_u16(0x10000), memory:read_u16(0x10002),
        transfers[0x100fe], transfers[0x10100], order_errors))
end)
assert(coroutine.resume(sample))
assert(tap)
assert(release_tap)

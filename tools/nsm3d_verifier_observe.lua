-- Read-only NSM-3D v5.02 observation; this never supplies a DSP verdict.
local machine = manager.machine
local cpu = assert(machine.devices[":maincpu"])
local memory = cpu.spaces["program"]
local transfers = { [0x100fe] = 0, [0x10100] = 0 }
local order_errors, last = 0, nil
local rx_read_sites = {}
local observe_rx = true
local record_calls = 0
local record_rejection_seen = false
local record_path = {}
local record_rejection_tap = memory:install_read_tap(0x28d350, 0x28d573,
    "nsm3d_record_rejection", function(offset, data, mask)
        local pc = cpu.state["PC"].value
        if not record_rejection_seen and pc >= 0x28d350 and pc <= 0x28d573
                and record_path[#record_path] ~= string.format("%08x", pc) then
            record_path[#record_path + 1] = string.format("%08x", pc)
            if #record_path > 16 then table.remove(record_path, 1) end
        end
        if pc == 0x28d56e and not record_rejection_seen then
            record_rejection_seen = true
            machine:logerror(string.format("nsm3d_record_rejected: pc=%08x t=%.6f\n",
                pc, machine.time:as_double()))
            machine:logerror(string.format("nsm3d_record_rejection_path: pcs=%s\n",
                table.concat(record_path, ",")))
        end
    end)
local record_tap = memory:install_read_tap(0x28d250, 0x28d253,
    "nsm3d_record_consumer", function(offset, data, mask)
        if cpu.state["PC"].value ~= 0x28d250 then return end
        record_calls = record_calls + 1
        if record_calls > 4 then return end
        local address = cpu.state["R0"].value
        if address < 0x100000 or address + 60 > 0x180000 then return end
        local bytes = {}
        for index = 0, 59 do
            bytes[#bytes + 1] = string.format("%02x", memory:read_u8(address + index))
        end
        machine:logerror(string.format("nsm3d_record_received: message=%08x bytes=%s t=%.6f\n",
            address, table.concat(bytes), machine.time:as_double()))
    end)
local rx_tap = memory:install_read_tap(0x101c8, 0x101cb,
    "nsm3d_rx_indices", function(offset, data, mask)
        local pc = cpu.state["PC"].value
        if observe_rx and machine.time:as_double() >= 1.04 and not rx_read_sites[pc] then
            rx_read_sites[pc] = true
            machine:logerror(string.format(
                "nsm3d_rx_index_read: pc=%08x address=%08x data=%08x mask=%08x\n",
                pc, offset, data, mask))
        end
    end)
local doorbell_tap = memory:install_write_tap(0x30000, 0x30003,
    "nsm3d_dspif_writes", function(offset, data, mask)
        machine:logerror(string.format(
            "nsm3d_dspif_write: address=%08x data=%08x mask=%08x pc=%08x t=%.6f\n",
            offset, data, mask, cpu.state["PC"].value, machine.time:as_double()))
        if cpu.state["PC"].value == 0x2cb838 then
            machine:logerror(string.format(
                "nsm3d_control_request: command=%04x argument=%04x commit=%04x wire=%04x pending=%04x t=%.6f\n",
                cpu.state["R4"].value & 0xffff, cpu.state["R6"].value & 0xffff,
                cpu.state["R5"].value & 0xffff, memory:read_u16(0x100a8),
                memory:read_u16(0x100e0), machine.time:as_double()))
            -- Encoder saves r8 and r4-r7 before LR: caller is SP + 20.
            machine:logerror(string.format(
                "nsm3d_control_parameter: caller=%08x wrapper_caller=%08x address=000100b8 value=%04x t=%.6f\n",
                memory:read_u32(cpu.state["R13"].value + 20),
                memory:read_u32(cpu.state["R13"].value + 36),
                memory:read_u16(0x100b8), machine.time:as_double()))
        end
    end)
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
            local upload = {}
            for index = 0, 637 do
                upload[#upload + 1] = string.format("%04x", memory:read_u16(0x10a00 + index * 2))
            end
            machine:logerror("nsm3d_loader_upload: words=" .. table.concat(upload) .. "\n")
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
    local dsp = machine.devices[":dsp_staged:cpu"]
    if dsp then
        local fragment = {}
        for index = 0, 103 do
            fragment[#fragment + 1] = string.format("%04x", dsp.spaces["program"]:read_u16(0xff80 + index))
        end
        machine:logerror("nsm3d_program_fragment: words=" .. table.concat(fragment) .. "\n")
    end
    for address = 0x110f6, 0x11102, 2 do
        machine:logerror(string.format("nsm3d_verifier_input: address=%08x value=%04x\n",
            address, memory:read_u16(address)))
    end
    if not emu.wait(7.5) then return end
    -- Do not attribute the observer's own boundary reads to the MCU's PC.
    observe_rx = false
    local identity = {}
    for index = 0, 12 do
        identity[#identity + 1] = string.format("%02x", memory:read_u8(0x12da5c + index))
    end
    machine:logerror(string.format("nsm3d_identity_boundary: ready=%02x record=%s\n",
        memory:read_u8(0x12da3f), table.concat(identity)))
    local context = {}
    for index = 0, 19 do
        context[#context + 1] = string.format("%02x", memory:read_u8(0x12da3c + index))
    end
    machine:logerror("nsm3d_record_context: bytes=" .. table.concat(context) .. "\n")
    if dsp then
        local fields = {}
        for address = 0x110f6, 0x11102, 2 do
            fields[#fields + 1] = string.format("%04x", memory:read_u16(address))
        end
        machine:logerror(string.format("nsm3d_loader_boundary: pc=%04x selector=%04x ack=%04x pending=%04x fields=%s t=%.6f\n",
            dsp.state["PC"].value, memory:read_u16(0x100e2), memory:read_u16(0x100e4),
            memory:read_u16(0x100e0), table.concat(fields, "/"), machine.time:as_double()))
    end
    machine:logerror(string.format(
        "nsm3d_verifier_boundary: pc=%08x result0=%04x result1=%04x pairs0=%d pairs1=%d order_errors=%d\n",
        cpu.state["PC"].value, memory:read_u16(0x10000), memory:read_u16(0x10002),
        transfers[0x100fe], transfers[0x10100], order_errors))
    for _, address in ipairs({0x100a4, 0x100a6, 0x101c8, 0x101ca, 0x100dc, 0x100e4}) do
        machine:logerror(string.format("nsm3d_shared_boundary: address=%08x value=%04x\n",
            address, memory:read_u16(address)))
    end
    local screen = machine.screens[":screen"]
    local snapshot = os.getenv("NOKIA_DCT3_SNAPSHOT_DIR")
    if screen and snapshot then screen:snapshot(snapshot .. "/nsm3d-frontier.png") end
end)
assert(coroutine.resume(sample))
assert(tap)
assert(release_tap)
-- Keep the tap userdata rooted for the entire run, including GC triggered
-- by the larger loader capture. A local assertion is not a lifetime root.
_G.nsm3d_observer_handles = {tap, release_tap, doorbell_tap, rx_tap, record_tap, record_rejection_tap}

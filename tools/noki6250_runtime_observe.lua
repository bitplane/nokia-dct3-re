-- Passive observation after native loader isolation; never supplies a reply.
local machine = manager.machine
local cpu = assert(machine.devices[":maincpu"])
local memory = cpu.spaces["program"]
local taps = {}
local receivers = {}
local nv_writers = {}
local nv_copy_count = 0
local lcd_commands, lcd_runs = {}, {}
local lcd_data_count, lcd_since_command = 0, 0
local lcd_nonzero_count = 0
local lcd_frame_nonzero, lcd_frame_ff = 0, 0
taps[#taps + 1] = memory:install_write_tap(0x17fd14, 0x17fd17,
    "6250_startup_flags", function(offset, value, mask)
        if (mask & 0xff0000) == 0 then return end
        machine:logerror(string.format("6250_startup_flags: data=%02x pc=%08x t=%.6f\n",
            (value >> 16) & 0xff, cpu.state["PC"].value, machine.time:as_double()))
    end)
taps[#taps + 1] = memory:install_read_tap(0x48078c, 0x48078f,
    "6250_nv_record_copy", function(offset, value, mask)
        if cpu.state["PC"].value ~= 0x48078c then return end
        local destination = cpu.state["R0"].value
        local length = cpu.state["R2"].value
        if destination >= 0x15c28a or destination + length <= 0x15c154 then return end
        machine:logerror(string.format("6250_nv_record_copy: destination=%08x source=%08x length=%x caller=%08x t=%.6f\n",
            destination, cpu.state["R1"].value, length, cpu.state["R14"].value, machine.time:as_double()))
    end)
taps[#taps + 1] = memory:install_read_tap(0x514648, 0x51464b,
    "6250_nv_copy_calls", function(offset, value, mask)
        if cpu.state["PC"].value ~= 0x514648 then return end
        local destination = cpu.state["R0"].value
        local length = cpu.state["R2"].value
        if destination >= 0x15c28a or destination + length <= 0x15c154 then return end
        if nv_copy_count >= 32 then return end
        nv_copy_count = nv_copy_count + 1
        machine:logerror(string.format("6250_nv_copy: destination=%08x source=%08x length=%x caller=%08x t=%.6f\n",
            destination, cpu.state["R1"].value, length, cpu.state["R14"].value, machine.time:as_double()))
    end)
taps[#taps + 1] = memory:install_write_tap(0x15c154, 0x15c28b,
    "6250_nv_shadow_writers", function(offset, value, mask)
        local pc = cpu.state["PC"].value
        if nv_writers[pc] then return end
        nv_writers[pc] = true
        machine:logerror(string.format("6250_nv_shadow_writer: pc=%08x caller=%08x address=%08x data=%08x mask=%08x r0=%08x r1=%08x r2=%08x t=%.6f\n",
            pc, cpu.state["R14"].value, offset, value, mask,
            cpu.state["R0"].value, cpu.state["R1"].value, cpu.state["R2"].value, machine.time:as_double()))
    end)
taps[#taps + 1] = memory:install_write_tap(0x17fbe0, 0x17fbf7,
    "6250_startup_faults", function(offset, value, mask)
        for lane = 0, 3 do
            local shift = (3 - lane) * 8
            if ((mask >> shift) & 0xff) ~= 0 then
                local byte = (value >> shift) & 0xff
                if byte ~= 0 then
                    machine:logerror(string.format("6250_startup_fault: offset=%02x data=%02x pc=%08x t=%.6f\n",
                        offset + lane - 0x17fbe0, byte, cpu.state["PC"].value, machine.time:as_double()))
                    if offset + lane == 0x17fbec and cpu.state["PC"].value == 0x304330 then
                        local stack = cpu.state["R13"].value
                        machine:logerror(string.format(
                            "6250_nv_sum_failure: computed=%04x stored0254=%04x companion0170=%04x t=%.6f\n",
                            cpu.state["R6"].value & 0xffff, memory:read_u16(stack + 4),
                            memory:read_u16(stack + 6), machine.time:as_double()))
                        local bytes = {}
                        for index = 0x120, 0x255 do
                            bytes[#bytes + 1] = string.format("%02x", memory:read_u8(0x15c034 + index))
                        end
                        machine:logerror("6250_nv_sum_shadow: bytes=" .. table.concat(bytes) .. "\n")
                    end
                end
            end
        end
    end)
taps[#taps + 1] = memory:install_write_tap(0x2006c, 0x2006f,
    "6250_lcd_commands", function(offset, value, mask)
        if (mask & 0xff00) == 0 then return end
        local command = (value >> 8) & 0xff
        lcd_commands[command] = (lcd_commands[command] or 0) + 1
        if #lcd_runs < 80 then
            lcd_runs[#lcd_runs + 1] = string.format("%02x:%d", command, lcd_since_command)
        end
        lcd_since_command = 0
    end)
taps[#taps + 1] = memory:install_write_tap(0x2002c, 0x2002f,
    "6250_lcd_data", function(offset, value, mask)
        if (mask & 0xff00) == 0 then return end
        lcd_data_count = lcd_data_count + 1
        local byte = (value >> 8) & 0xff
        if byte ~= 0 then
            lcd_nonzero_count = lcd_nonzero_count + 1
            lcd_frame_nonzero = lcd_frame_nonzero + 1
        end
        if byte == 0xff then lcd_frame_ff = lcd_frame_ff + 1 end
        if lcd_data_count % 768 == 0 then
            machine:logerror(string.format("6250_lcd_transfer: index=%d nonzero=%d ff=%d pc=%08x t=%.6f\n",
                lcd_data_count // 768, lcd_frame_nonzero, lcd_frame_ff,
                cpu.state["PC"].value, machine.time:as_double()))
            lcd_frame_nonzero, lcd_frame_ff = 0, 0
        end
        lcd_since_command = lcd_since_command + 1
    end)
taps[#taps + 1] = memory:install_write_tap(0x17fe24, 0x17fe27,
    "6250_lifecycle_status", function(offset, value, mask)
        if (mask & 0xff000000) == 0 then return end
        machine:logerror(string.format("6250_lifecycle_status: data=%02x pc=%08x t=%.6f\n",
            (value >> 24) & 0xff, cpu.state["PC"].value, machine.time:as_double()))
    end)
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
local captured = 0
emu.register_periodic(function()
    local deadline = captured == 0 and 8 or 20
    if captured >= 2 or machine.time:as_double() < deadline then return end
    captured = captured + 1
    local dsp = assert(machine.devices[":dsp_staged:cpu"])
    machine:logerror(string.format(
        "6250_runtime_boundary: arm_pc=%08x dsp_pc=%04x pending=%04x result=%04x/%04x t=%.6f\n",
        cpu.state["PC"].value, dsp.state["PC"].value, memory:read_u16(0x100e0),
        memory:read_u16(0x10000), memory:read_u16(0x10002), machine.time:as_double()))
    machine:logerror(string.format("6250_service_control_endpoint: flags=%02x fault0=%02x fault1=%02x\n",
        memory:read_u8(0x17fd15), memory:read_u8(0x17fbf0), memory:read_u8(0x17fbf1)))
    local faults = {}
    for index = 0, 23 do faults[#faults + 1] = string.format("%02x", memory:read_u8(0x17fbe0 + index)) end
    machine:logerror("6250_startup_fault_endpoint: bytes=" .. table.concat(faults) .. "\n")
    machine:logerror(string.format("6250_lcd_runs: data_total=%d commands=%s\n",
        lcd_data_count, table.concat(lcd_runs, ",")))
    machine:logerror(string.format("6250_lcd_payload: nonzero_bytes=%d t=%.6f\n",
        lcd_nonzero_count, machine.time:as_double()))
    local counts = {}
    for command = 0, 255 do
        if lcd_commands[command] then
            counts[#counts + 1] = string.format("%02x:%d", command, lcd_commands[command])
        end
    end
    machine:logerror("6250_lcd_commands: counts=" .. table.concat(counts, ",") .. "\n")
    machine.screens[":screen"]:snapshot(captured == 1 and "6250_runtime.png" or "6250_runtime20.png")
end)
_G.noki6250_runtime_taps = taps

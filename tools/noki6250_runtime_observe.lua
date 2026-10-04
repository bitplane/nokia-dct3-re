-- Passive observation after native loader isolation; never supplies a reply.
local machine = manager.machine
local cpu = assert(machine.devices[":maincpu"])
local memory = cpu.spaces["program"]
local taps = {}
local receivers = {}
local nv_writers = {}
local nv_copy_count = 0
local keypad_readers = {}
local scalar_posts = {}
local channel_confirmations = {}
taps[#taps + 1] = memory:install_read_tap(0x464738, 0x46473b,
    "6250_channel_confirmation", function(offset, value, mask)
        if cpu.state["PC"].value ~= 0x464738 then return end
        local object = cpu.state["R0"].value
        -- The pending-context address is supplied by the own-ROM literal.
        local pointer_address = memory:read_u32(0x4649e8)
        local context = memory:read_u32(pointer_address)
        local body = memory:read_u8(object + 4)
        local input, expected, pending = 0xffff, 0xff, 0xff
        if context >= 0x100000 and context < 0x180000 then
            input = memory:read_u16(context)
            expected, pending = memory:read_u8(context + 2), memory:read_u8(context + 3)
        end
        local key = string.format("%x:%x:%x:%x", body, input, expected, pending)
        if channel_confirmations[key] then return end
        channel_confirmations[key] = true
        machine:logerror(string.format("6250_channel_confirmation: body=%02x input=%04x expected=%02x pending=%02x t=%.6f\n",
            body, input, expected, pending, machine.time:as_double()))
    end)
local sim_accesses = {}
for _, direction in ipairs({"read", "write"}) do
    local install = direction == "read" and memory.install_read_tap or memory.install_write_tap
    taps[#taps + 1] = install(memory, 0x20034, 0x2003f,
        "6250_simi_" .. direction, function(offset, value, mask)
            local pc = cpu.state["PC"].value
            local key = string.format("%s:%x:%x:%x:%x", direction, offset, value, mask, pc)
            if sim_accesses[key] then return end
            sim_accesses[key] = true
            machine:logerror(string.format("6250_simi_access: direction=%s address=%08x data=%08x mask=%08x pc=%08x t=%.6f\n",
                direction, offset, value, mask, pc, machine.time:as_double()))
        end)
end
local analog_writes, analog_receiver_seen = {}, false
local analog_predicates = {}
local analog_samples = {}
taps[#taps + 1] = memory:install_read_tap(0x3daa80, 0x3daa83,
    "6250_analog_samples", function(offset, value, mask)
        if cpu.state["PC"].value ~= 0x3daa82 then return end
        local first = memory:read_u16(cpu.state["R5"].value)
        local second = memory:read_u16(cpu.state["R4"].value)
        local key = first * 0x10000 + second
        if analog_samples[key] then return end
        analog_samples[key] = true
        machine:logerror(string.format("6250_analog_samples: first=%04x second=%04x count=%02x t=%.6f\n",
            first, second, memory:read_u8(0x1691e2), machine.time:as_double()))
    end)
local analog_messages = {}
local analog_selectors = {}
taps[#taps + 1] = memory:install_read_tap(0x20000, 0x20003,
    "6250_reset_cause_read", function(offset, value, mask)
        if (mask & 0xff0000) == 0 or machine.time:as_double() > 0.1 then return end
        machine:logerror(string.format("6250_reset_cause_read: data=%08x mask=%08x pc=%08x t=%.6f\n",
            value, mask, cpu.state["PC"].value, machine.time:as_double()))
    end)
taps[#taps + 1] = memory:install_write_tap(0x20000, 0x20003,
    "6250_reset_control", function(offset, value, mask)
        if (mask & 0xff0000) == 0 then return end
        machine:logerror(string.format("6250_reset_control: value=%02x pc=%08x t=%.6f\n",
            (value >> 16) & 0xff, cpu.state["PC"].value, machine.time:as_double()))
    end)
for _, address in ipairs({0x17fd74, 0x17fe15}) do
    local aligned, shift = address & ~3, (3 - (address & 3)) * 8
    taps[#taps + 1] = memory:install_write_tap(aligned, aligned + 3,
        string.format("6250_analog_selector_%x", address), function(offset, value, mask)
            if ((mask >> shift) & 0xff) == 0 then return end
            local byte = (value >> shift) & 0xff
            local key = string.format("%x:%x:%x", address, byte, cpu.state["PC"].value)
            if analog_selectors[key] then return end
            analog_selectors[key] = true
            machine:logerror(string.format("6250_analog_selector: address=%08x value=%02x pc=%08x t=%.6f\n",
                address, byte, cpu.state["PC"].value, machine.time:as_double()))
        end)
end
taps[#taps + 1] = memory:install_read_tap(0x508830, 0x508833,
    "6250_analog_messages", function(offset, value, mask)
        if cpu.state["PC"].value ~= 0x508832 then return end
        local caller = cpu.state["R14"].value
        if caller < 0x30af20 or caller >= 0x30d800 then return end
        if analog_messages[caller] then return end
        analog_messages[caller] = true
        local address, bytes = cpu.state["R0"].value, {}
        if address < 0x200000 or address >= 0x5a0000 then return end
        for index = 0, 160 do
            local byte = memory:read_u8(address + index)
            if byte == 0 then break end
            bytes[#bytes + 1] = byte >= 32 and byte <= 126 and string.char(byte) or " "
        end
        machine:logerror(string.format("6250_analog_message: caller=%08x text=%s t=%.6f\n",
            caller, table.concat(bytes), machine.time:as_double()))
    end)
taps[#taps + 1] = memory:install_read_tap(0x4f9188, 0x4f918b,
    "6250_analog_predicate", function(offset, value, mask)
        if cpu.state["PC"].value ~= 0x4f918a then return end
        local argument, caller = cpu.state["R0"].value, cpu.state["R14"].value
        if caller ~= 0x30cedd then return end
        local cached = memory:read_u8(0x172cc8)
        if analog_predicates[cached] then return end
        analog_predicates[cached] = true
        machine:logerror(string.format("6250_analog_predicate: argument=%08x cached=%02x count=%02x t=%.6f\n",
            argument, cached, memory:read_u8(0x1704ad), machine.time:as_double()))
    end)
taps[#taps + 1] = memory:install_read_tap(0x3c3588, 0x3c358b,
    "6250_scalar_posts", function(offset, value, mask)
        if cpu.state["PC"].value ~= 0x3c3588 then return end
        local task, message = cpu.state["R0"].value, cpu.state["R1"].value
        if task > 32 or message > 0xffff then return end
        local key = task * 0x10000 + message
        if scalar_posts[key] then return end
        scalar_posts[key] = true
        machine:logerror(string.format("6250_scalar_post: task=%d message=%04x caller=%08x t=%.6f\n",
            task, message, cpu.state["R14"].value, machine.time:as_double()))
    end)
local lcd_commands, lcd_runs = {}, {}
local lcd_data_count, lcd_since_command = 0, 0
local lcd_nonzero_count = 0
local lcd_frame_nonzero, lcd_frame_ff = 0, 0
taps[#taps + 1] = memory:install_write_tap(0x1704c4, 0x1704c7,
    "6250_analog_lifecycle", function(offset, value, mask)
        local key = string.format("%08x:%08x:%08x", cpu.state["PC"].value, value, mask)
        if analog_writes[key] then return end
        analog_writes[key] = true
        machine:logerror(string.format("6250_analog_lifecycle: data=%08x mask=%08x pc=%08x t=%.6f\n",
            value, mask, cpu.state["PC"].value, machine.time:as_double()))
    end)
for _, watched in ipairs({{0x172c84, 0xff0000, 16, "readiness"},
                          {0x17fe38, 0xff000000, 24, "phase"}}) do
    local address, byte_mask, shift, name = table.unpack(watched)
    taps[#taps + 1] = memory:install_write_tap(address, address + 3,
        "6250_startup_" .. name, function(offset, value, mask)
            if (mask & byte_mask) == 0 then return end
            machine:logerror(string.format("6250_startup_%s: value=%02x pc=%08x t=%.6f\n",
                name, (value >> shift) & 0xff, cpu.state["PC"].value, machine.time:as_double()))
        end)
end
taps[#taps + 1] = memory:install_write_tap(0x172ca4, 0x172cab,
    "6250_startup_context", function(offset, value, mask)
        machine:logerror(string.format("6250_startup_context: address=%08x data=%08x mask=%08x pc=%08x t=%.6f\n",
            offset, value, mask, cpu.state["PC"].value, machine.time:as_double()))
    end)
taps[#taps + 1] = memory:install_read_tap(0x508460, 0x508463,
    "6250_keypad_suppression", function(offset, value, mask)
        if cpu.state["PC"].value ~= 0x508462 then return end
        machine:logerror(string.format("6250_keypad_suppression: caller=%08x t=%.6f\n",
            cpu.state["R14"].value, machine.time:as_double()))
    end)
taps[#taps + 1] = memory:install_write_tap(0x20068, 0x2006b,
    "6250_column_mask_writers", function(offset, value, mask)
        if (mask & 0xff) == 0 then return end
        machine:logerror(string.format("6250_column_mask: value=%02x pc=%08x caller=%08x t=%.6f\n",
            value & 0xff, cpu.state["PC"].value, cpu.state["R14"].value, machine.time:as_double()))
    end)
taps[#taps + 1] = memory:install_write_tap(0x174050, 0x174053,
    "6250_raw_matrix_key", function(offset, value, mask)
        if (mask & 0xff000000) == 0 then return end
        machine:logerror(string.format("6250_raw_matrix_key: value=%02x pc=%08x t=%.6f\n",
            (value >> 24) & 0xff, cpu.state["PC"].value, machine.time:as_double()))
    end)
taps[#taps + 1] = memory:install_read_tap(0x20028, 0x2002b,
    "6250_keypad_readers", function(offset, value, mask)
        if (mask & 0xff00) == 0 then return end
        local pc = cpu.state["PC"].value
        if keypad_readers[pc] then return end
        keypad_readers[pc] = true
        machine:logerror(string.format("6250_keypad_reader: pc=%08x columns=%02x t=%.6f\n",
            pc, (value >> 8) & 0xff, machine.time:as_double()))
    end)
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
        if cpu.state["PC"].value ~= 0x3c363c then return end
        local caller = cpu.state["R14"].value
        if caller == 0x30af9d and not analog_receiver_seen then
            analog_receiver_seen = true
            machine:logerror(string.format("6250_analog_receiver: task=%d t=%.6f\n",
                memory:read_u8(0x100022), machine.time:as_double()))
        end
        if memory:read_u8(0x100022) ~= 2 then return end
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
    machine:logerror(string.format("6250_analog_endpoint: event=%04x state=%04x t=%.6f\n",
        memory:read_u16(0x1704c4), memory:read_u16(0x1704c6), machine.time:as_double()))
    machine:logerror(string.format("6250_analog_fields: count=%02x field0e=%02x field11=%02x ccont10=%02x t=%.6f\n",
        memory:read_u8(0x1704ad), memory:read_u8(0x1704b2),
        memory:read_u8(0x1704b5), memory:read_u8(0x172cc8), machine.time:as_double()))
    machine:logerror(string.format("6250_analog_calibration: gain=%08x offset=%08x sample_count=%02x t=%.6f\n",
        memory:read_u32(0x17fd4c), memory:read_u32(0x17fd50),
        memory:read_u8(0x1691e2), machine.time:as_double()))
    machine:logerror(string.format("6250_startup_context_endpoint: counter=%02x input=%04x continuation=%04x t=%.6f\n",
        memory:read_u8(0x172ca4), memory:read_u16(0x172ca6), memory:read_u16(0x172ca8),
        machine.time:as_double()))
    machine:logerror(string.format("6250_startup_gate: phase=%02x readiness=%02x t=%.6f\n",
        memory:read_u8(0x17fe38), memory:read_u8(0x172c85), machine.time:as_double()))
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

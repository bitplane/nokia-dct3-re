-- Read-only NSM-1 compatibility observer; no synthetic DSP publications.
local machine = manager.machine
local cpu = machine.devices[':maincpu']
local memory = cpu.spaces['program']
local writes = 0
local handles = {}
local restart_payload_writes = 0
handles[#handles + 1] = memory:install_write_tap(0x10203c, 0x10203f,
    'nsm1_restart_payload', function(offset, value, mask)
        if machine.time:as_double() > 0.47 or restart_payload_writes >= 32 then return end
        restart_payload_writes = restart_payload_writes + 1
        machine:logerror(string.format(
            'nsm1_restart_payload: pc=%08x value=%08x mask=%08x r0=%08x r1=%08x r2=%08x caller=%08x task=%02x t=%.9f\n',
            cpu.state['PC'].value, value, mask, cpu.state['R0'].value,
            cpu.state['R1'].value, cpu.state['R2'].value, cpu.state['R14'].value,
            memory:read_u8(0x100022), machine.time:as_double()))
    end)
local restart_object_sends = 0
for _, sender in ipairs({0x275cb0}) do
    handles[#handles + 1] = memory:install_read_tap(sender, sender + 3,
        'nsm1_task2_send_' .. sender, function(offset, value, mask)
            if cpu.state['PC'].value ~= sender or cpu.state['R0'].value ~= 2 or
                restart_object_sends >= 32 then return end
            local object = cpu.state['R1'].value
            if object < 0x100000 or object > 0x11fff0 then return end
            restart_object_sends = restart_object_sends + 1
            local header = ''
            for index = 0, 11 do
                header = header .. string.format('%02x', memory:read_u8(object + index))
            end
            machine:logerror(string.format(
                'nsm1_task2_send: sender=%08x object=%08x header=%s caller=%08x source_task=%02x t=%.9f\n',
                sender, object, header, cpu.state['R14'].value,
                memory:read_u8(0x100022), machine.time:as_double()))
        end)
end
local restart_packets = 0
handles[#handles + 1] = memory:install_read_tap(0x2ab7c8, 0x2ab7cb,
    'nsm1_restart_packet', function(offset, value, mask)
        if cpu.state['PC'].value ~= 0x2ab7c8 or restart_packets >= 16 then return end
        local object = cpu.state['R0'].value
        if object < 0x100000 or object > 0x11ffc0 then return end
        restart_packets = restart_packets + 1
        local bytes = ''
        for index = 0, 15 do
            bytes = bytes .. string.format('%02x', memory:read_u8(object + index))
        end
        machine:logerror(string.format(
            'nsm1_restart_packet: object=%08x bytes=%s caller=%08x task=%02x t=%.9f\n',
            object, bytes, cpu.state['R14'].value, memory:read_u8(0x100022),
            machine.time:as_double()))
    end)
local restart_checks = 0
handles[#handles + 1] = memory:install_read_tap(0x27e7cc, 0x27e7cf,
    'nsm1_restart_check', function(offset, value, mask)
        if cpu.state['PC'].value ~= 0x27e7cc or restart_checks >= 16 then return end
        local context = cpu.state['R6'].value
        restart_checks = restart_checks + 1
        local bytes = ''
        local header = ''
        if context <= 0xffe0 or (context >= 0x100000 and context <= 0x11ffe0) or
            (context >= 0x200000 and context <= 0x3fffe0) then
            for index = 0, 6 do
                bytes = bytes .. string.format('%02x', memory:read_u8(context + 12 + index))
            end
            for index = 0, 11 do
                header = header .. string.format('%02x', memory:read_u8(context + index))
            end
        end
        machine:logerror(string.format(
            'nsm1_restart_check: context=%08x header=%s bytes=%s valid=%08x class=%08x marker=%02x t=%.9f\n',
            context, header, bytes, cpu.state['R5'].value, cpu.state['R12'].value,
            memory:read_u8(0x11fdd2), machine.time:as_double()))
    end)
local restart_requests = 0
handles[#handles + 1] = memory:install_read_tap(0x2c2a00, 0x2c2a03,
    'nsm1_restart_request', function(offset, value, mask)
        if cpu.state['PC'].value ~= 0x2c2a02 or restart_requests >= 16 then return end
        restart_requests = restart_requests + 1
        machine:logerror(string.format(
            'nsm1_restart_request: reason=%08x caller=%08x task=%02x t=%.9f\n',
            cpu.state['R0'].value, cpu.state['R14'].value,
            memory:read_u8(0x100022), machine.time:as_double()))
    end)
local reset_control_writes = 0
handles[#handles + 1] = memory:install_write_tap(0x20000, 0x20003,
    'nsm1_reset_control', function(offset, value, mask)
        if (mask & 0x00ff0000) == 0 or reset_control_writes >= 32 then return end
        reset_control_writes = reset_control_writes + 1
        machine:logerror(string.format(
            'nsm1_reset_control: pc=%08x data=%02x caller=%08x task=%02x t=%.9f\n',
            cpu.state['PC'].value, (value >> 16) & 0xff,
            cpu.state['R14'].value, memory:read_u8(0x100022),
            machine.time:as_double()))
    end)
local reason_writes = 0
handles[#handles + 1] = memory:install_write_tap(0x11fed4, 0x11fed7,
    'nsm1_restart_reason', function(offset, value, mask)
        if reason_writes >= 32 then return end
        reason_writes = reason_writes + 1
        machine:logerror(string.format(
            'nsm1_restart_reason: pc=%08x value=%08x mask=%08x caller=%08x task=%02x t=%.9f\n',
            cpu.state['PC'].value, value, mask, cpu.state['R14'].value,
            memory:read_u8(0x100022), machine.time:as_double()))
    end)
local boot_selections = 0
handles[#handles + 1] = memory:install_read_tap(0x2810a0, 0x2810a3,
    'nsm1_boot_selection', function(offset, value, mask)
        if cpu.state['PC'].value ~= 0x2810a0 or boot_selections >= 16 then return end
        boot_selections = boot_selections + 1
        machine:logerror(string.format(
            'nsm1_boot_selection: selected=%08x reason=%02x task=%02x t=%.9f\n',
            cpu.state['R0'].value, memory:read_u8(0x11fed5),
            memory:read_u8(0x100022), machine.time:as_double()))
    end)
local owner_messages = 0
local owner_message_sites = {}
handles[#handles + 1] = memory:install_read_tap(0x2c17fc, 0x2c17ff,
    'nsm1_owner_diagnostic', function(offset, value, mask)
        if cpu.state['PC'].value ~= 0x2c17fe or
            memory:read_u8(0x100022) ~= 0x14 or owner_messages >= 64 then return end
        local address = cpu.state['R0'].value
        if address < 0x200000 or address > 0x3fff80 then return end
        local text = ''
        for index = 0, 127 do
            local byte = memory:read_u8(address + index)
            if byte == 0 then break end
            if byte < 32 or byte > 126 then return end
            text = text .. string.char(byte)
        end
        local caller = cpu.state['R14'].value
        if text == '' or owner_message_sites[caller] then return end
        owner_message_sites[caller] = true
        owner_messages = owner_messages + 1
        machine:logerror(string.format(
            'nsm1_owner_diagnostic: address=%08x caller=%08x text=%s t=%.9f\n',
            address, caller, text, machine.time:as_double()))
    end)
local sim_sends = 0
local owner_scalar_sends = 0
handles[#handles + 1] = memory:install_read_tap(0x275cb0, 0x275cb3,
    'nsm1_owner_scalar_send', function(offset, value, mask)
        if cpu.state['PC'].value ~= 0x275cb0 or cpu.state['R0'].value ~= 0x14 then return end
        if owner_scalar_sends >= 64 then return end
        owner_scalar_sends = owner_scalar_sends + 1
        machine:logerror(string.format(
            'nsm1_owner_scalar_send: value=%08x caller=%08x source_task=%02x t=%.9f\n',
            cpu.state['R1'].value, cpu.state['R14'].value,
            memory:read_u8(0x100022), machine.time:as_double()))
    end)
local activation_requests = 0
local application_receives = 0
local lifecycle_receives = 0
local scalar_receives = 0
local completion_gates = 0
local report_owner_receives = 0
handles[#handles + 1] = memory:install_read_tap(0x221ee4, 0x221ee7,
    'nsm1_report_owner_receive', function(offset, value, mask)
        if cpu.state['PC'].value ~= 0x221ee6 or report_owner_receives >= 64 then return end
        report_owner_receives = report_owner_receives + 1
        machine:logerror(string.format(
            'nsm1_report_owner_receive: task=%02x value=%08x context=%08x state=%04x t=%.9f\n',
            memory:read_u8(0x100022), cpu.state['R0'].value,
            cpu.state['R4'].value, memory:read_u16(cpu.state['R4'].value + 0x34),
            machine.time:as_double()))
    end)
local readiness_nibble_writes = 0
local input_state_writes = 0
local owner_state_writes = 0
handles[#handles + 1] = memory:install_write_tap(0x111c68, 0x111c6b,
    'nsm1_owner_state', function(offset, value, mask)
        if owner_state_writes >= 32 then return end
        owner_state_writes = owner_state_writes + 1
        machine:logerror(string.format(
            'nsm1_owner_state: value=%08x mask=%08x pc=%08x task=%02x t=%.9f\n',
            value, mask, cpu.state['PC'].value, memory:read_u8(0x100022),
            machine.time:as_double()))
    end)
handles[#handles + 1] = memory:install_write_tap(0x1127d8, 0x1127db,
    'nsm1_input_state', function(offset, value, mask)
        if input_state_writes >= 32 then return end
        input_state_writes = input_state_writes + 1
        machine:logerror(string.format(
            'nsm1_input_state: value=%08x mask=%08x pc=%08x t=%.9f\n',
            value, mask, cpu.state['PC'].value, machine.time:as_double()))
    end)
handles[#handles + 1] = memory:install_write_tap(0x1126c0, 0x1126c3,
    'nsm1_readiness_nibble', function(offset, value, mask)
        if readiness_nibble_writes >= 64 then return end
        readiness_nibble_writes = readiness_nibble_writes + 1
        machine:logerror(string.format(
            'nsm1_readiness_nibble: value=%08x mask=%08x pc=%08x task=%02x t=%.9f\n',
            value, mask, cpu.state['PC'].value, memory:read_u8(0x100022),
            machine.time:as_double()))
    end)
handles[#handles + 1] = memory:install_read_tap(0x281298, 0x28129b,
    'nsm1_completion_gate', function(offset, value, mask)
        if cpu.state['PC'].value ~= 0x28129a or completion_gates >= 16 then return end
        local pointer = cpu.state['R6'].value
        if pointer < 0x100000 or pointer > 0x11ffff then return end
        completion_gates = completion_gates + 1
        machine:logerror(string.format(
            'nsm1_completion_gate: context=%08x low_state=%02x secondary=%02x t=%.9f\n',
            pointer, memory:read_u8(pointer) & 0x0f,
            memory:read_u8(0x1126c1) & 0x0f, machine.time:as_double()))
    end)
handles[#handles + 1] = memory:install_read_tap(0x2802a0, 0x2802a3,
    'nsm1_scalar_receive', function(offset, value, mask)
        if cpu.state['PC'].value ~= 0x2802a2 or scalar_receives >= 64 then return end
        scalar_receives = scalar_receives + 1
        machine:logerror(string.format(
            'nsm1_scalar_receive: task=%02x value=%08x t=%.9f\n',
            memory:read_u8(0x100022), cpu.state['R0'].value,
            machine.time:as_double()))
    end)
handles[#handles + 1] = memory:install_read_tap(0x21cf0c, 0x21cf0f,
    'nsm1_lifecycle_queue', function(offset, value, mask)
        if cpu.state['PC'].value ~= 0x21cf0c then return end
        local task = memory:read_u8(0x100022)
        local descriptor = 0x1014ac + task * 0x1c
        machine:logerror(string.format(
            'nsm1_lifecycle_queue: task=%02x head=%02x tail=%02x t=%.9f\n',
            task, memory:read_u8(descriptor + 0x10),
            memory:read_u8(descriptor + 0x11), machine.time:as_double()))
    end)
handles[#handles + 1] = memory:install_read_tap(0x21cf14, 0x21cf17,
    'nsm1_lifecycle_receive', function(offset, value, mask)
        if cpu.state['PC'].value ~= 0x21cf14 or lifecycle_receives >= 64 then return end
        local pointer = cpu.state['R0'].value
        if not ((pointer >= 0x100000 and pointer <= 0x11fffc) or
                (pointer >= 0x200000 and pointer <= 0x3ffffc)) then return end
        lifecycle_receives = lifecycle_receives + 1
        machine:logerror(string.format(
            'nsm1_lifecycle_receive: task=%02x pointer=%08x id=%04x caller=%08x t=%.9f\n',
            memory:read_u8(0x100022), pointer, memory:read_u16(pointer),
            cpu.state['R14'].value, machine.time:as_double()))
    end)
handles[#handles + 1] = memory:install_read_tap(0x2085c4, 0x2085c7,
    'nsm1_application_receive', function(offset, value, mask)
        if cpu.state['PC'].value ~= 0x2085c4 or application_receives >= 32 then return end
        local pointer = cpu.state['R0'].value
        if not ((pointer >= 0x100000 and pointer <= 0x11fffc) or
                (pointer >= 0x200000 and pointer <= 0x3ffffc)) then return end
        application_receives = application_receives + 1
        machine:logerror(string.format(
            'nsm1_application_receive: task=%02x pointer=%08x id=%04x t=%.9f\n',
            memory:read_u8(0x100022), pointer, memory:read_u16(pointer),
            machine.time:as_double()))
    end)
for _, address in ipairs({0x207718, 0x207afc, 0x208a7c, 0x20837c, 0x2085bc,
        0x29f340, 0x21e41c, 0x21cf0c, 0x21e9a8,
        0x27a4f0, 0x281370, 0x2bc112, 0x28029c, 0x28129a,
        0x223520, 0x223a14, 0x223a68, 0x28b760, 0x2263fa, 0x2bc4d4,
        0x2b3f12, 0x2bf2e6, 0x224e10, 0x22530c, 0x2bf1a0, 0x280ac0}) do
    local count = 0
    handles[#handles + 1] = memory:install_read_tap(address & ~3,
        (address & ~3) + 3, 'nsm1_activation_owner_' .. address,
        function(offset, value, mask)
            if cpu.state['PC'].value ~= address or count >= 16 then return end
            count = count + 1
            machine:logerror(string.format(
                'nsm1_activation_owner: pc=%08x caller=%08x r8=%08x sl=%08x gate=%02x task=%02x t=%.9f\n',
                address, cpu.state['R14'].value, cpu.state['R8'].value,
                cpu.state['R10'].value, memory:read_u8(0x111e71),
                memory:read_u8(0x100022),
                machine.time:as_double()))
        end)
end
handles[#handles + 1] = memory:install_read_tap(0x29cb90, 0x29cb93,
    'nsm1_activation_request', function(offset, value, mask)
        if cpu.state['PC'].value ~= 0x29cb90 or activation_requests >= 16 then return end
        activation_requests = activation_requests + 1
        machine:logerror(string.format(
            'nsm1_activation_request: caller=%08x selector=%02x t=%.9f\n',
            cpu.state['R14'].value, memory:read_u8(0x10e6d6),
            machine.time:as_double()))
    end)
local owner_queue_writes = 0
handles[#handles + 1] = memory:install_write_tap(0x1012c8, 0x1012cb,
    'nsm1_owner_first_queue_slot', function(offset, value, mask)
        if owner_queue_writes >= 32 then return end
        owner_queue_writes = owner_queue_writes + 1
        machine:logerror(string.format(
            'nsm1_owner_queue_write: pc=%08x value=%08x mask=%08x caller=%08x task=%02x t=%.9f\n',
            cpu.state['PC'].value, value, mask, cpu.state['R14'].value,
            memory:read_u8(0x100022), machine.time:as_double()))
    end)
local owner_queue_reads = 0
for _, address in ipairs({0x275e40, 0x275f3e}) do
    handles[#handles + 1] = memory:install_read_tap(address & ~3,
        (address & ~3) + 3, 'nsm1_owner_queue_' .. address,
        function(offset, value, mask)
            if cpu.state['PC'].value ~= address or
                memory:read_u8(0x100022) ~= 0x14 or owner_queue_reads >= 32 then return end
            owner_queue_reads = owner_queue_reads + 1
            local descriptor = 0x1014ac + 0x14 * 0x1c
            local column = address == 0x275e40 and 0x0c or 0x14
            local base = memory:read_u32(descriptor + column)
            local index = cpu.state['R0'].value
            machine:logerror(string.format(
                'nsm1_owner_queue: pc=%08x base=%08x index=%02x value=%08x t=%.9f\n',
                address, base, index, memory:read_u32(base + index * 4),
                machine.time:as_double()))
        end)
end
local descriptor_events = 0
for _, address in ipairs({0x275106, 0x275f9c}) do
    handles[#handles + 1] = memory:install_read_tap(address & ~3,
        (address & ~3) + 3, 'nsm1_descriptor_' .. address,
        function(offset, value, mask)
            if cpu.state['PC'].value ~= address then return end
            local r0 = cpu.state['R0'].value
            local index = r0
            if address == 0x275f9c then
                if r0 < 0x100000 or r0 > 0x11fff0 then return end
                index = memory:read_u8(r0 + 9)
            end
            if index ~= 0xe3 or descriptor_events >= 32 then return end
            descriptor_events = descriptor_events + 1
            machine:logerror(string.format(
                'nsm1_descriptor: pc=%08x index=%02x r0=%08x r1=%08x caller=%08x task=%02x t=%.9f\n',
                address, index, r0, cpu.state['R1'].value,
                cpu.state['R14'].value, memory:read_u8(0x100022),
                machine.time:as_double()))
        end)
end
for _, sender in ipairs({0x27641c, 0x275b60}) do
handles[#handles + 1] = memory:install_read_tap(sender, sender + 3,
    'nsm1_sim_sender_' .. sender, function(offset, value, mask)
        if cpu.state['PC'].value ~= sender or cpu.state['R0'].value ~= 0x16 then return end
        sim_sends = sim_sends + 1
        if sim_sends > 32 then return end
        machine:logerror(string.format(
            'nsm1_sim_send: sender=%08x argument=%08x caller=%08x count=%u t=%.9f\n',
            sender, cpu.state['R1'].value, cpu.state['R14'].value, sim_sends,
            machine.time:as_double()))
    end)
end
local sim_receive_entries = 0
local socket_continuations = 0
handles[#handles + 1] = memory:install_read_tap(0x28830c, 0x28830f,
    'nsm1_socket_continuation', function(offset, value, mask)
        if cpu.state['PC'].value ~= 0x28830e or socket_continuations >= 16 then return end
        socket_continuations = socket_continuations + 1
        machine:logerror(string.format(
            'nsm1_socket_continuation: event=%02x object=%08x flag14=%02x t=%.9f\n',
            cpu.state['R8'].value, cpu.state['R4'].value,
            memory:read_u8(0x10e6d6), machine.time:as_double()))
    end)
handles[#handles + 1] = memory:install_read_tap(0x288114, 0x288117,
    'nsm1_sim_receive_entry', function(offset, value, mask)
        if cpu.state['PC'].value ~= 0x288114 then return end
        sim_receive_entries = sim_receive_entries + 1
        if sim_receive_entries > 16 then return end
        local task = memory:read_u8(0x100022)
        local descriptor = 0x1014ac + task * 0x1c
        machine:logerror(string.format(
            'nsm1_sim_receive_entry: caller=%08x task=%02x descriptor=%08x receive_head=%02x receive_tail=%02x count=%u t=%.9f\n',
            cpu.state['R14'].value, task, descriptor,
            memory:read_u8(descriptor + 0x10), memory:read_u8(descriptor + 0x11),
            sim_receive_entries, machine.time:as_double()))
    end)
for _, address in ipairs({0x28812a, 0x288140}) do
    local count = 0
    handles[#handles + 1] = memory:install_read_tap(address & ~3,
        (address & ~3) + 3, 'nsm1_sim_receive_' .. address,
        function(offset, value, mask)
            if cpu.state['PC'].value ~= address or count >= 32 then return end
            local pointer = cpu.state['R0'].value
            local in_ram = pointer >= 0x100000 and pointer <= 0x11fff4
            local in_flash = pointer >= 0x200000 and pointer <= 0x3ffff4
            if not in_ram and not in_flash then return end
            count = count + 1
            local bytes = {}
            for index = 0, 11 do
                bytes[#bytes + 1] = string.format('%02x', memory:read_u8(pointer + index))
            end
            machine:logerror(string.format(
                'nsm1_sim_receive: pc=%08x pointer=%08x bytes=%s count=%u t=%.9f\n',
                address, pointer, table.concat(bytes), count, machine.time:as_double()))
        end)
end
local readiness_writes = 0
handles[#handles + 1] = memory:install_write_tap(0x10e6c8, 0x10e6d3,
    'nsm1_readiness_writers', function(offset, value, mask)
        readiness_writes = readiness_writes + 1
        if readiness_writes > 64 then return end
        machine:logerror(string.format(
            'nsm1_readiness_write: address=%08x value=%08x mask=%08x pc=%08x t=%.9f\n',
            offset, value, mask, cpu.state['PC'].value, machine.time:as_double()))
    end)
for _, address in ipairs({0x2b61bc, 0x2b61c4, 0x2b61cc, 0x2b61d4,
        0x2b61dc, 0x2b61e4, 0x2b61ec, 0x2b601a, 0x2b60ae}) do
    local count = 0
    handles[#handles + 1] = memory:install_read_tap(address & ~3,
        (address & ~3) + 3, 'nsm1_native_predicate_' .. address,
        function(offset, value, mask)
            if cpu.state['PC'].value ~= address or count >= 16 then return end
            count = count + 1
            machine:logerror(string.format(
                'nsm1_native_predicate: pc=%08x r0=%08x count=%u t=%.9f\n',
                address, cpu.state['R0'].value, count, machine.time:as_double()))
        end)
end
local mmio = {}
handles[#handles + 1] = memory:install_write_tap(0x20000, 0x200ff,
    'nsm1_native_mmio', function(offset, value, mask)
        local key = string.format('%08x/%08x', offset, mask)
        local count = (mmio[key] or 0) + 1
        mmio[key] = count
        if count <= 3 then
            machine:logerror(string.format(
                'nsm1_native_mmio: address=%08x value=%08x mask=%08x pc=%08x t=%.9f\n',
                offset, value, mask, cpu.state['PC'].value,
                machine.time:as_double()))
        end
    end)
handles[#handles + 1] = memory:install_write_tap(0x10000, 0x10103,
    'nsm1_native_shared', function(offset, value, mask)
        writes = writes + 1
        if writes <= 64 then
            machine:logerror(string.format(
                'nsm1_native_shared: address=%08x value=%08x mask=%08x pc=%08x t=%.9f\n',
                offset, value, mask, cpu.state['PC'].value,
                machine.time:as_double()))
        end
    end)
local next_sample = 0
local captured = false
emu.register_frame_done(function()
    local now = machine.time:as_double()
    if now < next_sample then return end
    next_sample = next_sample + 1
    machine:logerror(string.format(
        'nsm1_native_snapshot: pc=%08x sp=%08x lr=%08x shared_writes=%u t=%.9f\n',
        cpu.state['PC'].value, cpu.state['R13'].value,
        cpu.state['R14'].value, writes, now))
    if now >= 8 and not captured then
        captured = true
        machine:logerror(string.format(
            'nsm1_native_result: verifier_first=%04x verifier_second=%04x dsp_state=%02x service_state=%02x supervisor_latch=%02x t=%.9f\n',
            memory:read_u16(0x111a9e), memory:read_u16(0x111aa0),
            memory:read_u8(0x111a94), memory:read_u8(0x11fdd1),
            memory:read_u8(0x112864), now))
        machine:logerror(string.format(
            'nsm1_native_readiness: first=%02x selector=%02x second=%02x third=%02x t=%.9f\n',
            memory:read_u8(0x111ea0), memory:read_u8(0x10be8c),
            memory:read_u8(0x10e6ce), memory:read_u8(0x10e6d0), now))
        machine.screens[':screen']:snapshot('6150-native-compatibility.png')
        local keys = {}
        for key in pairs(mmio) do keys[#keys + 1] = key end
        table.sort(keys)
        for _, key in ipairs(keys) do
            machine:logerror(string.format(
                'nsm1_native_mmio_summary: lane=%s writes=%u\n', key, mmio[key]))
        end
    end
end, 'frame')
_G.nsm1_native_observer_handles = handles

-- Read-only NSM-1 compatibility observer; no synthetic DSP publications.
local machine = manager.machine
local cpu = machine.devices[':maincpu']
local memory = cpu.spaces['program']
local writes = 0
local handles = {}
local sim_sends = 0
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

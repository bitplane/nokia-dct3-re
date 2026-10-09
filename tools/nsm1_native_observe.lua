-- Read-only NSM-1 compatibility observer; no synthetic DSP publications.
local machine = manager.machine
local cpu = machine.devices[':maincpu']
local memory = cpu.spaces['program']
local writes = 0
local handles = {}
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

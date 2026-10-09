-- Read-only own NSE-6 observer; no fabricated DSP/firmware publications.
local machine = manager.machine
local cpu = machine.devices[':maincpu']
local memory = cpu.spaces['program']
local handles = {}
local counts = {}
local result_readers = {}
local report_events = {}
local report_raw_messages = {}
local mask_writes = 0
handles[#handles + 1] = memory:install_write_tap(0x20030, 0x20033,
    'nse6_keypad_mask', function(offset, value, mask)
        if (mask & 0xff) == 0 or mask_writes >= 32 then return end
        mask_writes = mask_writes + 1
        machine:logerror(string.format(
            'nse6_column_mask_write: value=%02x pc=%08x lr=%08x t=%.9f\n',
            value & 0xff, cpu.state['PC'].value, cpu.state['R14'].value,
            machine.time:as_double()))
    end)
for _, address in ipairs({0x13ff74, 0x13fde1, 0x1216bc, 0x11eb59}) do
    local writes = 0
    local late_writes = 0
    handles[#handles + 1] = memory:install_write_tap(address & ~3,
        (address & ~3) + 3, 'nse6_failure_' .. address,
        function(offset, value, mask)
            local lane = 0xff << ((3 - (address & 3)) * 8)
            if (mask & lane) == 0 then return end
            writes = writes + 1
            if machine.time:as_double() >= 7 then
                late_writes = late_writes + 1
                if late_writes > 16 then return end
            elseif writes > 32 then return end
            machine:logerror(string.format(
                '%s: address=%08x pc=%08x lr=%08x byte=%02x t=%.9f\n',
                address == 0x1216bc and 'nse6_wake_flag_write'
                    or address == 0x11eb59 and 'nse6_report_event_write'
                    or 'nse6_failure_write',
                address, cpu.state['PC'].value, cpu.state['R14'].value,
                (value >> ((3 - (address & 3)) * 8)) & 0xff,
                machine.time:as_double()))
            if address == 0x13fde1 and cpu.state['PC'].value == 0x240c90 then
                machine:logerror(string.format(
                    'nse6_status_failure: index=%02x status=%02x address=%08x t=%.9f\n',
                    cpu.state['R0'].value,
                    memory:read_u8(cpu.state['R2'].value), cpu.state['R2'].value,
                    machine.time:as_double()))
            end
        end)
end
handles[#handles + 1] = memory:install_read_tap(0x1205c8, 0x1205cf,
    'nse6_verifier_result_readers', function(offset, value, mask)
        if not ((offset == 0x1205c8 and (mask & 0x0000ffff) ~= 0)
            or (offset == 0x1205cc and (mask & 0xffff0000) ~= 0)) then return end
        local pc = cpu.state['PC'].value
        if result_readers[pc] then return end
        result_readers[pc] = true
        machine:logerror(string.format(
            'nse6_verifier_reader: pc=%08x address=%08x value=%08x mask=%08x t=%.9f\n',
            pc, offset, value, mask, machine.time:as_double()))
    end)
for _, address in ipairs({0x200040, 0x2000ec, 0x2dd100, 0x2b6118,
        0x2b6196, 0x2b61bc, 0x2b6200, 0x2d333c, 0x2dcef0,
        0x2e1194, 0x2de164, 0x2e049a, 0x2ca910, 0x243a24, 0x243ba4, 0x240c1e,
        0x240992, 0x240b94, 0x2dfe9e, 0x28d5ca,
        0x2d3398, 0x2d33a0, 0x2d33a8, 0x2d33b0, 0x2d33b8,
        0x2d33c0, 0x2d33c8, 0x2d33cc, 0x288a7a,
        0x21fb1e, 0x21fb94, 0x21fb98, 0x2dca88, 0x294c90,
        0x222532, 0x22254e, 0x21e012, 0x293da2}) do
    handles[#handles + 1] = memory:install_read_tap(address & ~3,
        (address & ~3) + 3, 'nse6_stage_' .. address, function(offset, value, mask)
            if cpu.state['PC'].value ~= address then return end
            counts[address] = (counts[address] or 0) + 1
            if address == 0x293da2 and counts[address] <= 16 then
                machine:logerror(string.format(
                    'nse6_sample_acquire: result=%08x remaining=%02x first=%04x second=%04x t=%.9f\n',
                    cpu.state['R0'].value, memory:read_u8(0x11eb5e),
                    memory:read_u16(cpu.state['R13'].value),
                    memory:read_u16(cpu.state['R13'].value + 2),
                    machine.time:as_double()))
            end
            if address == 0x21e012 then
                local message = cpu.state['R0'].value
                report_raw_messages[message] = (report_raw_messages[message] or 0) + 1
            end
            if address == 0x22254e then
                local event = memory:read_u16(cpu.state['R4'].value + 0x32)
                report_events[event] = (report_events[event] or 0) + 1
            end
            if address == 0x22254e and (counts[address] <= 32
                    or memory:read_u16(cpu.state['R4'].value + 0x32) == 0x49) then
                local context = cpu.state['R4'].value
                machine:logerror(string.format(
                    'nse6_report14_dispatch: state=%08x context=%08x event=%04x countdown=%02x flag_a=%02x flag_f=%02x sample=%04x lr=%08x t=%.9f\n',
                    cpu.state['R0'].value, context,
                    memory:read_u16(context + 0x32), memory:read_u8(context + 5),
                    memory:read_u8(context + 0xa), memory:read_u8(context + 0xf),
                    memory:read_u16(0x13fe54),
                    cpu.state['R14'].value, machine.time:as_double()))
            end
            if address == 0x288a7a and counts[address] <= 32 then
                local context = cpu.state['R4'].value
                if context >= 0x100000 and context <= 0x13fff8 then
                    machine:logerror(string.format(
                        'nse6_input_controller: context=%08x state=%04x event=%04x t=%.9f\n',
                        context, memory:read_u16(context + 4),
                        memory:read_u16(context + 2), machine.time:as_double()))
                end
            end
            if address >= 0x2d3398 and address <= 0x2d33cc
                    and counts[address] <= 4 then
                machine:logerror(string.format(
                    'nse6_park_predicate: pc=%08x result=%08x flag_address=%08x t=%.9f\n',
                    address, cpu.state['R0'].value, cpu.state['R5'].value,
                    machine.time:as_double()))
            end
            if address == 0x28d5ca and counts[address] <= 4 then
                machine:logerror(string.format(
                    'nse6_config_integrity: computed=%08x stored=%08x t=%.9f\n',
                    cpu.state['R0'].value,
                    memory:read_u32(cpu.state['R5'].value + 0x3c),
                    machine.time:as_double()))
            end
            if address == 0x2dfe9e and cpu.state['R1'].value == 0x6209 then
                machine:logerror(string.format(
                    'nse6_integrity_service: enabled=%02x bitmap=%02x class_mask=%02x t=%.9f\n',
                    memory:read_u8(0x13ff1c), memory:read_u8(0x13ff4c),
                    memory:read_u8(0x303766), machine.time:as_double()))
            end
            if (address == 0x240992 or address == 0x240b94)
                    and counts[address] <= 4 then
                machine:logerror(string.format(
                    'nse6_integrity_return: pc=%08x value=%08x t=%.9f\n',
                    address, cpu.state['R0'].value, machine.time:as_double()))
            end
            if address == 0x240c1e and counts[address] == 1 then
                local sp = cpu.state['R13'].value
                machine:logerror(string.format(
                    'nse6_security_gate: computed=%04x stored11e=%04x stored90=%04x t=%.9f\n',
                    cpu.state['R9'].value & 0xffff, memory:read_u16(sp + 4),
                    memory:read_u16(sp + 6), machine.time:as_double()))
            end
            if address == 0x2dcef0 and counts[address] <= 32 then
                machine:logerror(string.format(
                    'nse6_eeprom_request: address=%08x operation=%08x lr=%08x t=%.9f\n',
                    cpu.state['R0'].value, cpu.state['R1'].value,
                    cpu.state['R14'].value, machine.time:as_double()))
            end
            -- Observe at the caller's return target: translated straight-line
            -- instruction fetches need not trigger a mid-routine read tap.
            if address == 0x2d333c and counts[address] == 1 then
                machine:logerror(string.format(
                    'nse6_verifier_result: first=%04x second=%04x t=%.9f\n',
                    memory:read_u16(0x1205ca), memory:read_u16(0x1205cc),
                    machine.time:as_double()))
            end
            if counts[address] > 4 then return end
            machine:logerror(string.format(
                'nse6_stage_pc: pc=%08x lr=%08x sp=%08x t=%.9f\n',
                address, cpu.state['R14'].value, cpu.state['R13'].value,
                machine.time:as_double()))
        end)
end
local next_sample = 0
local captured = false
local late_captured = false
emu.register_frame_done(function()
    local now = machine.time:as_double()
    if now < next_sample then return end
    next_sample = next_sample + 1
    if now >= 28 and not late_captured then
        late_captured = true
        machine.screens[':screen']:snapshot('8810-stage-late.png')
        machine:logerror(string.format(
            'nse6_late_input_counts: scans=%d decodes=%d t=%.9f\n',
            counts[0x2de164] or 0, counts[0x2e049a] or 0, now))
    end
    machine:logerror(string.format(
        'nse6_stage_snapshot: pc=%08x sp=%08x first=%04x second=%04x readiness=%02x reports=%02x t=%.9f\n',
        cpu.state['PC'].value, cpu.state['R13'].value,
        memory:read_u16(0x100fe), memory:read_u16(0x10100),
        memory:read_u8(0x13ffa8), memory:read_u8(0x12147d), now))
    if now >= 8 and not captured then
        captured = true
        machine.screens[':screen']:snapshot('8810-stage.png')
        local addresses = {}
        for address in pairs(counts) do addresses[#addresses + 1] = address end
        table.sort(addresses)
        for _, address in ipairs(addresses) do
            machine:logerror(string.format('nse6_stage_count: pc=%08x count=%u\n',
                address, counts[address]))
        end
        for _, category in ipairs({{'event', report_events}, {'raw', report_raw_messages}}) do
            local keys = {}
            for key in pairs(category[2]) do keys[#keys + 1] = key end
            table.sort(keys)
            for _, key in ipairs(keys) do
                machine:logerror(string.format(
                    'nse6_report14_histogram: kind=%s value=%04x count=%u t=%.9f\n',
                    category[1], key, category[2][key], now))
            end
        end
    end
end, 'frame')
_G.nse6_stage_observer_handles = handles

-- Read-only own NSE-6 observer; no fabricated DSP/firmware publications.
local machine = manager.machine
local cpu = machine.devices[':maincpu']
local memory = cpu.spaces['program']
local ccont_watchdog = emu.item(assert(machine.devices[':ccont'].items['0/m_watchdog']))
local mad2_registers = emu.item(assert(machine.devices[':mad2'].items['0/m_regs']))
local ccont_registers = emu.item(assert(machine.devices[':ccont'].items['0/m_regs']))
local dsp = machine.devices[':dsp_c54x:cpu']
local native_frames = dsp and emu.item(assert(machine.devices[':dsp_c54x'].items['0/m_frame_timer_expiries']))
local native_slots = dsp and emu.item(assert(machine.devices[':dsp_c54x'].items['0/m_slot_timer_expiries']))
local handles = {}
local pending_memory_uploads = {}
local observing_native = false
if dsp then
    local parameter_writes = {}
    handles[#handles + 1] = dsp.spaces['data']:install_write_tap(0x06f4, 0x06f6,
        'nse6_native_serial_parameters', function(offset, value, mask)
            local key = string.format('%04x:%04x', offset, dsp.state['PC'].value)
            parameter_writes[key] = (parameter_writes[key] or 0) + 1
            if parameter_writes[key] > 4 then return end
            machine:logerror(string.format(
                'nse6_native_serial_parameters: address=%04x value=%04x mask=%04x pc=%04x t=%.9f\n',
                offset, value, mask, dsp.state['PC'].value, machine.time:as_double()))
        end)
    local port_control_writers = {}
    handles[#handles + 1] = dsp.spaces['data']:install_write_tap(0x06f3, 0x06f3,
        'nse6_native_port_control', function(offset, value, mask)
            local pc = dsp.state['PC'].value
            port_control_writers[pc] = (port_control_writers[pc] or 0) + 1
            if port_control_writers[pc] > 4 then return end
            machine:logerror(string.format(
                'nse6_native_port_control: pc=%04x value=%04x t=%.9f\n',
                pc, value, machine.time:as_double()))
        end)
    for _, site in ipairs({0x3bcd, 0x9232, 0x9129, 0x9116, 0x90e7, 0x92c4}) do
        local hits = 0
        handles[#handles + 1] = dsp.spaces['program']:install_read_tap(site, site,
            'nse6_native_port_transaction_' .. site, function()
                if dsp.state['PC'].value ~= site + 1 or hits >= 4 then return end
                hits = hits + 1
                local sp = dsp.state['SP'].value
                machine:logerror(string.format(
                    'nse6_native_port_transaction: site=%04x sp=%04x stack=%04x control=%04x a=%010x t=%.9f\n',
                    site, sp, dsp.spaces['data']:read_u16(sp),
                    dsp.spaces['data']:read_u16(0x06f3),
                    dsp.state['A'].value, machine.time:as_double()))
            end)
    end
    for _, direction in ipairs({'read', 'write'}) do
        local sites = {}
        local install = direction == 'read' and 'install_read_tap' or 'install_write_tap'
        handles[#handles + 1] = dsp.spaces['io'][install](dsp.spaces['io'], 0x36, 0x36,
            'nse6_native_port36_' .. direction, function(offset, value, mask)
                local pc = dsp.state['PC'].value
                sites[pc] = (sites[pc] or 0) + 1
                if sites[pc] > 4 then return end
                machine:logerror(string.format(
                    'nse6_native_port36: direction=%s value=%04x mask=%04x pc=%04x ar2=%04x control=%04x t=%.9f\n',
                    direction, value, mask, pc, dsp.state['AR2'].value,
                    dsp.spaces['data']:read_u16(0x06f3), machine.time:as_double()))
            end)
    end
    local completion_sites = {}
    handles[#handles + 1] = dsp.spaces['io']:install_write_tap(1, 1,
        'nse6_native_completion_bits', function(offset, value, mask)
            local pc = dsp.state['PC'].value
            local key = string.format('%04x:%04x', pc, value)
            completion_sites[key] = (completion_sites[key] or 0) + 1
            if completion_sites[key] > 4 then return end
            machine:logerror(string.format(
                'nse6_native_completion_bits: value=%04x pc=%04x t=%.9f\n',
                value, pc, machine.time:as_double()))
        end)
    local wakeup_writers = {}
    handles[#handles + 1] = dsp.spaces['data']:install_write_tap(0x08f8, 0x08f8,
        'nse6_native_tx_wakeup_flag', function(offset, value, mask)
            local pc = dsp.state['PC'].value
            wakeup_writers[pc] = (wakeup_writers[pc] or 0) + 1
            if wakeup_writers[pc] > 8 then return end
            machine:logerror(string.format(
                'nse6_native_tx_wakeup_flag: value=%04x mask=%04x pc=%04x t=%.9f\n',
                value, mask, pc, machine.time:as_double()))
        end)
    local service_irq_writes = 0
    handles[#handles + 1] = dsp.spaces['data']:install_write_tap(0x0029, 0x0029,
        'nse6_native_service_irq', function(offset, value, mask)
            service_irq_writes = service_irq_writes + 1
            if service_irq_writes > 128 then return end
            machine:logerror(string.format(
                'nse6_native_service_irq: value=%04x mask=%04x pc=%04x tx_producer=%04x tx_consumer=%04x t=%.9f\n',
                value, mask, dsp.state['PC'].value,
                dsp.spaces['data']:read_u16(0x0852),
                dsp.spaces['data']:read_u16(0x0853), machine.time:as_double()))
        end)
    local flag_readers = {}
    handles[#handles + 1] = dsp.spaces['data']:install_read_tap(0x1f11, 0x1f11,
        'nse6_native_control_flag_readers', function(offset, value, mask)
            if observing_native then return end
            local pc = dsp.state['PC'].value
            flag_readers[pc] = (flag_readers[pc] or 0) + 1
            if flag_readers[pc] > 2 then return end
            machine:logerror(string.format(
                'nse6_native_control_flag_reader: pc=%04x value=%04x t=%.9f\n',
                pc, value, machine.time:as_double()))
        end)
    for _, site in ipairs({0x49fc, 0x4a15, 0x4a16, 0x4a25,
            0x4ac7, 0x4b1e, 0x4b1f, 0x4b3d, 0x4b3e, 0x4b72, 0x4b73, 0x4bac}) do
        local hits = 0
        handles[#handles + 1] = dsp.spaces['program']:install_read_tap(site, site,
            'nse6_native_control_handler_' .. site, function()
                if dsp.state['PC'].value ~= site + 1 or hits >= 4 then return end
                hits = hits + 1
                observing_native = true
                machine:logerror(string.format(
                    'nse6_native_control_handler: site=%04x flags=%04x result=%04x scratch=%04x identity0=%04x identity1=%04x t=%.9f\n',
                    site, dsp.spaces['data']:read_u16(0x1f11),
                    dsp.spaces['data']:read_u16(0x06f9),
                    dsp.spaces['data']:read_u16(0x1200),
                    dsp.spaces['data']:read_u16(0x1f0c),
                    dsp.spaces['data']:read_u16(0x1f0d), machine.time:as_double()))
                observing_native = false
            end)
    end
    for _, site in ipairs({0x3982, 0x3987}) do
        local hits = 0
        handles[#handles + 1] = dsp.spaces['program']:install_read_tap(site, site,
            'nse6_native_special_' .. site, function()
                if dsp.state['PC'].value ~= site + 1 or hits >= 32 then return end
                hits = hits + 1
                machine:logerror(string.format(
                    'nse6_native_special_dispatch: site=%04x a=%04x b=%04x control=%04x t=%.9f\n',
                    site, dsp.state['A'].value & 0xffff, dsp.state['B'].value & 0xffff,
                    dsp.spaces['data']:read_u16(0x06e1), machine.time:as_double()))
            end)
    end
    local host_dispatches = 0
    handles[#handles + 1] = dsp.spaces['program']:install_read_tap(0x3995, 0x3995,
        'nse6_native_host_dispatch', function()
            if dsp.state['PC'].value ~= 0x3996 or host_dispatches >= 128 then return end
            host_dispatches = host_dispatches + 1
            machine:logerror(string.format(
                'nse6_native_host_dispatch: target=%04x table70=%04x ar0=%04x ar1=%04x t=%.9f\n',
                dsp.state['A'].value & 0xffff, dsp.spaces['data']:read_u16(0xb07f),
                dsp.state['AR0'].value, dsp.state['AR1'].value,
                machine.time:as_double()))
        end)
    local upload_writes = 0
    handles[#handles + 1] = dsp.spaces['program']:install_write_tap(0x2946, 0x295d,
        'nse6_native_context_upload', function(offset, value, mask)
            upload_writes = upload_writes + 1
            if upload_writes > 32 then return end
            machine:logerror(string.format(
                'nse6_native_context_upload: address=%04x value=%04x mask=%04x pc=%04x pmst=%04x t=%.9f\n',
                offset, value, mask, dsp.state['PC'].value, dsp.state['PMST'].value,
                machine.time:as_double()))
        end)
    local overlay_writes = 0
    handles[#handles + 1] = dsp.spaces['data']:install_write_tap(0x1ca4, 0x1ca5,
        'nse6_native_overlay_entry', function(offset, value, mask)
            overlay_writes = overlay_writes + 1
            if overlay_writes > 32 then return end
            machine:logerror(string.format(
                'nse6_native_overlay_write: address=%04x value=%04x mask=%04x pc=%04x sp=%04x pmst=%04x t=%.9f\n',
                offset, value, mask, dsp.state['PC'].value, dsp.state['SP'].value,
                dsp.state['PMST'].value, machine.time:as_double()))
        end)
    local context_observed = false
    handles[#handles + 1] = dsp.spaces['program']:install_read_tap(0x2677, 0x2677,
        'nse6_native_context_target', function()
            if context_observed or dsp.state['PC'].value ~= 0x2678 then return end
            context_observed = true
            local target = dsp.spaces['program']:read_u16(0x2679)
            local program_words, data_words = {}, {}
            for index = 0, 23 do
                program_words[#program_words + 1] = string.format('%04x', dsp.spaces['program']:read_u16(target + index))
                data_words[#data_words + 1] = string.format('%04x', dsp.spaces['data']:read_u16(target + index))
            end
            machine:logerror(string.format(
                'nse6_native_context_target: address=%04x pmst=%04x program=%s data=%s t=%.9f\n',
                target, dsp.state['PMST'].value, table.concat(program_words, ','),
                table.concat(data_words, ','), machine.time:as_double()))
        end)
    local stack_floor = 0x2000
    local stack_samples = 0
    handles[#handles + 1] = dsp.spaces['data']:install_write_tap(0x0800, 0x1fff,
        'nse6_native_stack_depth', function(offset, value, mask)
            local sp = dsp.state['SP'].value
            if offset ~= sp or sp >= stack_floor or stack_samples >= 32 then return end
            -- Sample each crossed 128-word boundary, not every stack write.
            stack_floor = sp & ~0x7f
            stack_samples = stack_samples + 1
            machine:logerror(string.format(
                'nse6_native_stack_depth: address=%04x value=%04x pc=%04x sp=%04x st1=%04x imr=%04x ifr=%04x t=%.9f\n',
                offset, value, dsp.state['PC'].value, sp, dsp.state['ST1'].value,
                dsp.state['IMR'].value, dsp.state['IFR'].value, machine.time:as_double()))
        end)
    local ring_writes = 0
    local tx_consumptions = 0
    handles[#handles + 1] = dsp.spaces['data']:install_write_tap(0x0853, 0x0853,
        'nse6_native_tx_consumption', function(offset, value, mask)
            local previous = dsp.spaces['data']:read_u16(0x0853)
            if previous == value or previous >= 0x52 or tx_consumptions >= 128 then return end
            tx_consumptions = tx_consumptions + 1
            local header = dsp.spaces['data']:read_u16(0x0800 + previous)
            local payload = {}
            if (header & 0xff) == 0x70 and (header >> 8) <= 0x80 then
                for index = 1, (header >> 8) // 2 do
                    payload[#payload + 1] = string.format('%04x',
                        dsp.spaces['data']:read_u16(0x0800 + ((previous + index) % 0x52)))
                end
                machine:logerror(string.format(
                    'nse6_native_control_packet: header=%04x words=%s t=%.9f\n',
                    header, table.concat(payload, ','), machine.time:as_double()))
            end
            if (header & 0xff) == 0x51 and (header >> 8) <= 0x80 then
                local upload = {words = {}, consumed = machine.time:as_double()}
                for index = 1, (header >> 8) // 2 do
                    local word = dsp.spaces['data']:read_u16(0x0800 + ((previous + index) % 0x52))
                    if index == 1 then upload.address = word
                    else upload.words[#upload.words + 1] = word end
                    payload[#payload + 1] = string.format('%04x',
                        word)
                end
                pending_memory_uploads[#pending_memory_uploads + 1] = upload
                machine:logerror(string.format(
                    'nse6_native_memory_upload: words=%s t=%.9f\n',
                    table.concat(payload, ','), machine.time:as_double()))
            end
            machine:logerror(string.format(
                'nse6_native_tx_consumption: previous=%04x next=%04x header=%04x pc=%04x t=%.9f\n',
                previous, value, header,
                dsp.state['PC'].value, machine.time:as_double()))
        end)
    handles[#handles + 1] = dsp.spaces['data']:install_write_tap(0x08e4, 0x08e5,
        'nse6_native_rx_indices', function(offset, value, mask)
            ring_writes = ring_writes + 1
            if ring_writes > 32 then return end
            machine:logerror(string.format(
                'nse6_native_rx_index_write: address=%04x value=%04x mask=%04x pc=%04x sp=%04x previous_word=%04x a=%010x b=%010x ar0=%04x ar1=%04x t=%.9f\n',
                offset, value, mask, dsp.state['PC'].value, dsp.state['SP'].value,
                dsp.spaces['program']:read_u16((dsp.state['PC'].value - 1) & 0xffff), dsp.state['A'].value,
                dsp.state['B'].value, dsp.state['AR0'].value, dsp.state['AR1'].value,
                machine.time:as_double()))
        end)
end
local counts = {}
local result_readers = {}
local report_events = {}
local report_raw_messages = {}
local d6_timer_requests = 0
local mask_writes = 0
local cache_writes = 0
local eeprom_blocks = 0
local lcd_data_count, lcd_nonzero_count, lcd_command_count = 0, 0, 0
handles[#handles + 1] = memory:install_write_tap(0x20028, 0x2002b,
    'nse6_lcd_data', function(offset, value, mask)
        if (mask & 0xff) == 0 then return end
        lcd_data_count = lcd_data_count + 1
        if (value & 0xff) ~= 0 then lcd_nonzero_count = lcd_nonzero_count + 1 end
    end)
handles[#handles + 1] = memory:install_write_tap(0x2002c, 0x2002f,
    'nse6_lcd_command', function(offset, value, mask)
        if (mask & 0xff000000) == 0 then return end
        lcd_command_count = lcd_command_count + 1
        if lcd_command_count > 48 then return end
        machine:logerror(string.format(
            'nse6_lcd_command: value=%02x pc=%08x t=%.9f\n',
            (value >> 24) & 0xff, cpu.state['PC'].value,
            machine.time:as_double()))
    end)
local row_writes = 0
handles[#handles + 1] = memory:install_write_tap(0x20000, 0x20003,
    'nse6_reset_control', function(offset, value, mask)
        if (mask & 0x00ff00ff) == 0 then return end
        machine:logerror(string.format(
            'nse6_reset_control_write: value=%08x mask=%08x pc=%08x lr=%08x t=%.9f\n',
            value, mask, cpu.state['PC'].value, cpu.state['R14'].value,
            machine.time:as_double()))
    end)
local window_writes = 0
local status_writes = 0
local object_selector_writes = 0
handles[#handles + 1] = memory:install_write_tap(0x10d618, 0x10d61b,
    'nse6_status_object_selector', function(offset, value, mask)
        local now = machine.time:as_double()
        if (mask & 0x0000ff00) == 0 or now < 6.05 or now > 6.1
                or object_selector_writes >= 16 then return end
        object_selector_writes = object_selector_writes + 1
        machine:logerror(string.format(
            'nse6_status_object_selector_write: value=%02x pc=%08x lr=%08x source=%08x characteristics=%02x t=%.9f\n',
            (value >> 8) & 0xff, cpu.state['PC'].value,
            cpu.state['R14'].value, cpu.state['R4'].value,
            memory:read_u8(cpu.state['R4'].value + 0x10), now))
    end)
handles[#handles + 1] = memory:install_write_tap(0x11f534, 0x11f537,
    'nse6_reset_status_source', function(offset, value, mask)
        if (mask & 0x00ff0000) == 0 or status_writes >= 32 then return end
        status_writes = status_writes + 1
        machine:logerror(string.format(
            'nse6_reset_status_source: value=%02x pc=%08x lr=%08x t=%.9f\n',
            (value >> 16) & 0xff, cpu.state['PC'].value,
            cpu.state['R14'].value, machine.time:as_double()))
    end)
handles[#handles + 1] = memory:install_write_tap(0x12166c, 0x12166f,
    'nse6_dsp_window_count', function(offset, value, mask)
        if (mask & 0xffff) == 0 or window_writes >= 64 then return end
        window_writes = window_writes + 1
        machine:logerror(string.format(
            'nse6_dsp_window_count: value=%04x mask=%08x pc=%08x lr=%08x t=%.9f\n',
            value & 0xffff, mask, cpu.state['PC'].value, cpu.state['R14'].value,
            machine.time:as_double()))
    end)
local irq_writes = 0
handles[#handles + 1] = memory:install_write_tap(0x20008, 0x2000b,
    'nse6_input_irq_ack', function(offset, value, mask)
        -- IRQ acknowledgement is byte 0x09; exclude the busy FIQ registers.
        if machine.time:as_double() < 7 or (mask & 0x00ff0000) == 0 then return end
        irq_writes = irq_writes + 1
        if irq_writes > 48 then return end
        machine:logerror(string.format(
            'nse6_irq_register_write: value=%08x mask=%08x pc=%08x lr=%08x t=%.9f\n',
            value, mask, cpu.state['PC'].value, cpu.state['R14'].value,
            machine.time:as_double()))
    end)
handles[#handles + 1] = memory:install_write_tap(0x2002c, 0x2002f,
    'nse6_row_direction', function(offset, value, mask)
        if (mask & 0xff) == 0 or machine.time:as_double() < 7 then return end
        row_writes = row_writes + 1
        if row_writes > 48 then return end
        machine:logerror(string.format(
            'nse6_row_direction_write: value=%02x pc=%08x lr=%08x t=%.9f\n',
            value & 0xff, cpu.state['PC'].value, cpu.state['R14'].value,
            machine.time:as_double()))
    end)
handles[#handles + 1] = memory:install_write_tap(0x12158c, 0x12158f,
    'nse6_source_cache', function(offset, value, mask)
        cache_writes = cache_writes + 1
        if cache_writes > 16 then return end
        machine:logerror(string.format(
            'nse6_source_cache_write: value=%08x mask=%08x pc=%08x source=%08x t=%.9f\n',
            value, mask, cpu.state['PC'].value, cpu.state['R4'].value,
            machine.time:as_double()))
    end)
handles[#handles + 1] = memory:install_write_tap(0x20030, 0x20033,
    'nse6_keypad_mask', function(offset, value, mask)
        if (mask & 0xff) == 0 or mask_writes >= 32 then return end
        mask_writes = mask_writes + 1
        machine:logerror(string.format(
            'nse6_column_mask_write: value=%02x pc=%08x lr=%08x t=%.9f\n',
            value & 0xff, cpu.state['PC'].value, cpu.state['R14'].value,
            machine.time:as_double()))
    end)
for _, address in ipairs({0x13ff74, 0x13fde1, 0x1216bc, 0x11eb59,
        0x13fde6, 0x13fdeb}) do
    local writes = 0
    local late_writes = 0
    local last_liveness_value = nil
    handles[#handles + 1] = memory:install_write_tap(address & ~3,
        (address & ~3) + 3, 'nse6_failure_' .. address,
        function(offset, value, mask)
            local lane = 0xff << ((3 - (address & 3)) * 8)
            if (mask & lane) == 0 then return end
            if address == 0x13fde6 or address == 0x13fdeb then
                local byte = (value >> ((3 - (address & 3)) * 8)) & 0xff
                if byte == last_liveness_value then return end
                last_liveness_value = byte
            end
            writes = writes + 1
            if machine.time:as_double() >= 7 then
                late_writes = late_writes + 1
                if late_writes > 16 then return end
            elseif writes > 32 then return end
            machine:logerror(string.format(
                '%s: address=%08x pc=%08x lr=%08x byte=%02x t=%.9f\n',
                address == 0x1216bc and 'nse6_wake_flag_write'
                    or address == 0x11eb59 and 'nse6_report_event_write'
                    or (address == 0x13fde6 or address == 0x13fdeb) and 'nse6_liveness_write'
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
        0x2b6026, 0x2b5fda,
        0x2b6196, 0x2b61bc, 0x2b6200, 0x2d333c, 0x2dcef0,
        0x2e1194, 0x2de164, 0x2e049a, 0x2ca910, 0x243a24, 0x243ba4, 0x243c22,
        0x27a76e, 0x27b604, 0x2d3460, 0x2c68c8, 0x2c6908,
        0x2e2748, 0x2e25f6, 0x211318, 0x20b3c4, 0x2b0adc, 0x240c1e,
        0x240992, 0x240b94, 0x2dfe9e, 0x28d5ca,
        0x2d3398, 0x2d33a0, 0x2d33a8, 0x2d33b0, 0x2d33b8,
        0x2d33c0, 0x2d33c8, 0x2d33cc, 0x288a7a,
        0x21fb1e, 0x21fb94, 0x21fb98, 0x2dca88, 0x294c90,
        0x222532, 0x22254e, 0x21e012, 0x293da2, 0x293ccc, 0x293d0e,
        0x2dd07e, 0x2d6f28, 0x28ced8, 0x2c90c2, 0x2dd18a, 0x2dd344,
        0x2184b6, 0x218a42, 0x2185f4, 0x2a94f4, 0x262bf4, 0x262056,
        0x202cb0, 0x206584, 0x206598, 0x2007f8, 0x218c66,
        0x2896fc, 0x218fb4, 0x218fbc, 0x21a74c, 0x21a868, 0x21a8dc,
        0x21cd2e, 0x21b312, 0x210fca, 0x210f90, 0x210fac, 0x210fbc,
        0x2110d6, 0x2110f0, 0x21111e, 0x2112e6, 0x2112a6, 0x212470,
        0x2d354c, 0x2d36ee, 0x2b5f62,
        0x2b5e78, 0x2b5e96, 0x2b5ecc, 0x2b66fc, 0x27b1c8, 0x29b9ea}) do
    handles[#handles + 1] = memory:install_read_tap(address & ~3,
        (address & ~3) + 3, 'nse6_stage_' .. address, function(offset, value, mask)
            if cpu.state['PC'].value ~= address then return end
            counts[address] = (counts[address] or 0) + 1
            if address == 0x29b9ea and counts[address] <= 8 then
                machine:logerror(string.format(
                    'nse6_native_control_constructor: r0=%08x r1=%08x lr=%08x t=%.9f\n',
                    cpu.state['R0'].value, cpu.state['R1'].value,
                    cpu.state['R14'].value, machine.time:as_double()))
            end
            if address == 0x27b1c8 and cpu.state['R0'].value == 3 then
                local packet = cpu.state['R1'].value
                if memory:read_u16(packet) == 2 and memory:read_u8(packet + 3) == 0x4c then
                    machine:logerror(string.format(
                        'nse6_native_control_post: object=%08x length=%02x payload=%04x,%04x lr=%08x t=%.9f\n',
                        packet, memory:read_u8(packet + 2), memory:read_u16(packet + 4),
                        memory:read_u16(packet + 6), cpu.state['R14'].value,
                        machine.time:as_double()))
                end
            end
            if address == 0x2d354c and counts[address] <= 128 then
                local packet = memory:read_u32(cpu.state['R13'].value + 0xb4)
                machine:logerror(string.format(
                    'nse6_tx_mailbox_object: object=%08x prefix=%04x type=%02x t=%.9f\n',
                    packet, memory:read_u16(packet), memory:read_u8(packet + 3),
                    machine.time:as_double()))
            end
            if address == 0x2d36ee and counts[address] <= 128 then
                local packet = cpu.state['R6'].value
                machine:logerror(string.format(
                    'nse6_tx_space_check: object=%08x type=%02x length=%02x available=%u t=%.9f\n',
                    packet, memory:read_u8(packet + 3), memory:read_u8(packet + 2),
                    cpu.state['R0'].value, machine.time:as_double()))
            end
            if address == 0x2b5f62 and counts[address] <= 128 then
                machine:logerror(string.format(
                    'nse6_tx_ring_request: source=%08x type=%02x length=%u lr=%08x t=%.9f\n',
                    cpu.state['R1'].value, memory:read_u8(cpu.state['R1'].value),
                    cpu.state['R0'].value, cpu.state['R14'].value,
                    machine.time:as_double()))
            end
            if address == 0x2112a6 and counts[address] <= 16 then
                local packet = cpu.state['R11'].value
                machine:logerror(string.format(
                    'nse6_search_packet_posted: object=%08x prefix=%04x length=%02x type=%02x control=%02x t=%.9f\n',
                    packet, memory:read_u16(packet), memory:read_u8(packet + 2),
                    memory:read_u8(packet + 3), memory:read_u8(packet + 5),
                    machine.time:as_double()))
            end
            if address == 0x2110d6 and counts[address] <= 16 then
                machine:logerror(string.format(
                    'nse6_search_builder_selector: local30=%08x context=%08x t=%.9f\n',
                    memory:read_u32(cpu.state['R13'].value + 0x30),
                    cpu.state['R4'].value, machine.time:as_double()))
            end
            if address == 0x210fac and counts[address] <= 16 then
                local context = cpu.state['R4'].value
                machine:logerror(string.format(
                    'nse6_completion_selector: context=%08x byte4=%02x t=%.9f\n',
                    context, memory:read_u8(context + 4), machine.time:as_double()))
            end
            if address == 0x210fbc and counts[address] <= 16 then
                local object = cpu.state['R4'].value
                machine:logerror(string.format(
                    'nse6_completion_static_object: object=%08x status=%04x t=%.9f\n',
                    object, memory:read_u16(object), machine.time:as_double()))
            end
            if address == 0x21b312 and counts[address] <= 16 then
                machine:logerror(string.format(
                    'nse6_controller_fallback: status=%04x context=%08x t=%.9f\n',
                    cpu.state['R11'].value & 0xffff, cpu.state['R4'].value,
                    machine.time:as_double()))
            end
            if address == 0x218fb4 and counts[address] <= 64 then
                machine:logerror(string.format(
                    'nse6_controller_wait: lr=%08x context=%08x mode_address=%08x mode=%04x t=%.9f\n',
                    cpu.state['R14'].value, cpu.state['R4'].value,
                    cpu.state['R7'].value, memory:read_u16(cpu.state['R7'].value),
                    machine.time:as_double()))
            end
            if address == 0x218fbc and counts[address] <= 64 then
                local message = cpu.state['R0'].value
                machine:logerror(string.format(
                    'nse6_search_controller_message: object=%08x status=%04x t=%.9f\n',
                    message, memory:read_u16(message), machine.time:as_double()))
            end
            if (address == 0x21a74c or address == 0x21a868 or address == 0x21cd2e)
                    and counts[address] <= 64 then
                machine:logerror(string.format(
                    'nse6_search_controller_return: site=%08x status=%04x t=%.9f\n',
                    address, cpu.state['R0'].value & 0xffff, machine.time:as_double()))
            end
            if address == 0x218c66 and counts[address] <= 64 then
                machine:logerror(string.format(
                    'nse6_search_lifecycle_construct: status=%04x lr=%08x t=%.9f\n',
                    cpu.state['R0'].value & 0xffff, cpu.state['R14'].value,
                    machine.time:as_double()))
            end
            if address == 0x202cb0 and counts[address] <= 64 then
                machine:logerror(string.format(
                    'nse6_search_lifecycle_input: status=%04x t=%.9f\n',
                    memory:read_u32(cpu.state['R13'].value) & 0xffff,
                    machine.time:as_double()))
            end
            if address == 0x2007f8 and counts[address] <= 64 then
                machine:logerror(string.format(
                    'nse6_lifecycle_publication: status=%04x arg=%08x lr=%08x t=%.9f\n',
                    cpu.state['R0'].value & 0xffff, cpu.state['R1'].value,
                    cpu.state['R14'].value, machine.time:as_double()))
            end
            if address == 0x262056 and counts[address] <= 64 then
                machine:logerror(string.format(
                    'nse6_search_predecessor_input: status=%04x t=%.9f\n',
                    cpu.state['R5'].value & 0xffff, machine.time:as_double()))
            end
            if address == 0x2a94f4 and counts[address] <= 64 then
                machine:logerror(string.format(
                    'nse6_search_status_construct: status=%04x lr=%08x t=%.9f\n',
                    cpu.state['R0'].value & 0xffff, cpu.state['R14'].value,
                    machine.time:as_double()))
            end
            if address == 0x2185f4 and counts[address] <= 64 then
                local message = cpu.state['R0'].value
                machine:logerror(string.format(
                    'nse6_search_dispatch_message: object=%08x status=%04x byte2=%02x t=%.9f\n',
                    message, memory:read_u16(message), memory:read_u8(message + 2),
                    machine.time:as_double()))
            end
            if address == 0x20b3c4 and counts[address] <= 16 then
                local object = cpu.state['R7'].value
                machine:logerror(string.format(
                    'nse6_reset_status_object: object=%08x header=%04x selector=%02x field18=%08x field1c=%08x t=%.9f\n',
                    object, memory:read_u16(object), memory:read_u8(object + 0x12),
                    memory:read_u32(object + 0x18), memory:read_u32(object + 0x1c),
                    machine.time:as_double()))
            end
            if address == 0x211318 and counts[address] <= 16 then
                local context = cpu.state['R4'].value
                machine:logerror(string.format(
                    'nse6_operational_window_entry: context=%08x count=%04x flags=%02x mode=%02x status=%02x lr=%08x t=%.9f\n',
                    context, memory:read_u16(context + 8), memory:read_u8(context + 0x1b),
                    memory:read_u8(cpu.state['R5'].value + 0x8d), memory:read_u8(0x11f535),
                    cpu.state['R14'].value, machine.time:as_double()))
            end
            if address == 0x2e25f6 and counts[address] <= 32 then
                machine:logerror(string.format(
                    'nse6_dsp_dispatch_entry: packet=%08x source=%08x counter_base=%08x t=%.9f\n',
                    cpu.state['R4'].value, cpu.state['R6'].value,
                    cpu.state['R5'].value, machine.time:as_double()))
            end
            if address == 0x2e2748 and counts[address] <= 16 then
                machine:logerror(string.format(
                    'nse6_dsp_window_expiry: status=%04x packets=%04x t=%.9f\n',
                    cpu.state['R4'].value, memory:read_u16(cpu.state['R5'].value + 2),
                    machine.time:as_double()))
            end
            if address == 0x2c68c8 and counts[address] <= 64 then
                machine:logerror(string.format(
                    'nse6_reset_selector_entry: input=%08x lr=%08x t=%.9f\n',
                    cpu.state['R0'].value, cpu.state['R14'].value, machine.time:as_double()))
            end
            if address == 0x2c6908 and counts[address] <= 64 then
                local sp = cpu.state['R13'].value
                machine:logerror(string.format(
                    'nse6_reset_selector_status: input=%08x nv607=%02x decoded=%02x reason=%02x t=%.9f\n',
                    cpu.state['R5'].value, memory:read_u8(sp), memory:read_u8(sp + 1),
                    cpu.state['R4'].value, machine.time:as_double()))
            end
            if address == 0x2d3460 and counts[address] <= 16 then
                machine:logerror(string.format(
                    'nse6_reset_timer_entry: lr=%08x t=%.9f\n',
                    cpu.state['R14'].value, machine.time:as_double()))
            end
            if (address == 0x243a24 or address == 0x243c22) and counts[address] <= 64 then
                machine:logerror(string.format(
                    'nse6_reset_status: pc=%08x argument=%04x counter=%02x activity=%02x lr=%08x t=%.9f\n',
                    address, cpu.state['R0'].value, memory:read_u8(0x13fde6),
                    memory:read_u8(0x13fdeb), cpu.state['R14'].value, machine.time:as_double()))
            end
            if address == 0x27a76e and cpu.state['R0'].value == 0x16 then
                d6_timer_requests = d6_timer_requests + 1
                if d6_timer_requests <= 32 then
                    machine:logerror(string.format(
                        'nse6_d6_timer_request: argument=%08x lr=%08x t=%.9f\n',
                        cpu.state['R1'].value, cpu.state['R14'].value, machine.time:as_double()))
                end
            end
            if address == 0x27b604 then
                local entry = cpu.state['R0'].value
                local timer_id = memory:read_u8(entry + 9)
                local status = memory:read_u32(0x2fa344 + timer_id * 8)
                if status == 0xd6 then
                    machine:logerror(string.format(
                        'nse6_d6_timer_receive: entry=%08x timer_id=%02x task_record=%08x lr=%08x t=%.9f\n',
                        entry, timer_id, cpu.state['R11'].value,
                        cpu.state['R14'].value, machine.time:as_double()))
                end
            end
            if address == 0x2b5fda and counts[address] <= 16 then
                local packet = cpu.state['R4'].value
                machine:logerror(string.format(
                    'nse6_dsp_rx_header: packet=%08x header=%04x read_index=%04x write_index=%04x lr=%08x t=%.9f\n',
                    packet, memory:read_u16(packet), memory:read_u16(0x101ca),
                    memory:read_u16(0x101c8), cpu.state['R14'].value, machine.time:as_double()))
            end
            if (address == 0x293ccc or address == 0x293d0e)
                    and counts[address] <= 4 then
                machine:logerror(string.format(
                    'nse6_source_sample: source=%u raw=%04x gain_bits=%08x offset_bits=%08x t=%.9f\n',
                    address == 0x293ccc and 7 or 8, cpu.state['R0'].value & 0xffff,
                    memory:read_u32(0x13fe4c), memory:read_u32(0x13fe50),
                    machine.time:as_double()))
            end
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
            if address == 0x2dd07e then
                eeprom_blocks = eeprom_blocks + 1
            if eeprom_blocks <= 4096 then
                machine:logerror(string.format(
                    'nse6_eeprom_block: address=%08x buffer=%08x length=%08x lr=%08x t=%.9f\n',
                    cpu.state['R0'].value, cpu.state['R1'].value,
                    cpu.state['R2'].value, cpu.state['R14'].value,
                    machine.time:as_double()))
            end
            end
            if address == 0x2d6f28 and machine.time:as_double() >= 6
                    and machine.time:as_double() <= 7 then
                machine:logerror(string.format(
                    'nse6_nv_record: buffer=%08x selector=%08x index=%08x lr=%08x t=%.9f\n',
                    cpu.state['R0'].value, cpu.state['R1'].value,
                    cpu.state['R2'].value, cpu.state['R14'].value,
                    machine.time:as_double()))
            end
            if address == 0x28ced8 or address == 0x2c90c2 then
                machine:logerror(string.format(
                    'nse6_identity_formatter: entry=%08x buffer=%08x flags=%08x lr=%08x t=%.9f\n',
                    address, cpu.state['R0'].value, cpu.state['R1'].value,
                    cpu.state['R14'].value, machine.time:as_double()))
            end
            if address == 0x2dd18a then
                local bytes, input_bytes, stored_bytes = {}, {}, {}
                local sp = cpu.state['R13'].value
                for index = 0, 15 do
                    bytes[#bytes + 1] = string.format('%02x', memory:read_u8(sp + index))
                end
                for index = 0, 7 do
                    input_bytes[#input_bytes + 1] = string.format('%02x',
                        memory:read_u8(cpu.state['R5'].value + index))
                    stored_bytes[#stored_bytes + 1] = string.format('%02x',
                        memory:read_u8(0x1215b0 + index))
                end
                machine:logerror(string.format(
                    'nse6_code_identity: bytes=%s entered=%s stored=%s input=%08x output=%08x t=%.9f\n',
                    table.concat(bytes), table.concat(input_bytes), table.concat(stored_bytes), cpu.state['R5'].value,
                    cpu.state['R4'].value, machine.time:as_double()))
            end
            if address == 0x2dd344 then
                machine:logerror(string.format(
                    'nse6_code_retry: matched=%08x attempts=%02x t=%.9f\n',
                    cpu.state['R0'].value, memory:read_u8(0x1215b4),
                    machine.time:as_double()))
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
    machine:logerror(string.format(
        'nse6_watchdog_snapshot: ccont=%02x mad2=%02x t=%.9f\n',
        ccont_watchdog:read(0), mad2_registers:read(3), now))
    if dsp then
        machine:logerror(string.format(
            'nse6_native_clock_snapshot: frames=%u slots=%u frame_enabled=%u t=%.9f\n',
            native_frames:read(0), native_slots:read(0),
            (ccont_registers:read(6) & 2) ~= 0 and 1 or 0, now))
        for _, upload in ipairs(pending_memory_uploads) do
            local matched = 0
            _G.rom4_observation_read = true
            for index, word in ipairs(upload.words) do
                if dsp.spaces['data']:read_u16(upload.address + index - 1) == word then
                    matched = matched + 1
                end
            end
            _G.rom4_observation_read = false
            machine:logerror(string.format(
                'nse6_native_memory_readback: address=%04x matched=%u words=%u consumed=%.9f t=%.9f\n',
                upload.address, matched, #upload.words, upload.consumed, now))
        end
        pending_memory_uploads = {}
        machine:logerror(string.format(
            'nse6_native_snapshot: pc=%04x sp=%04x imr=%04x ifr=%04x ccont_clock=%02x t=%.9f\n',
            dsp.state['PC'].value, dsp.state['SP'].value, dsp.state['IMR'].value, dsp.state['IFR'].value,
            ccont_registers:read(6), now))
        machine:logerror(string.format(
            'nse6_native_transport_snapshot: tx_producer=%04x tx_consumer=%04x command=%04x acknowledge=%04x rx_producer=%04x rx_consumer=%04x work=%04x mode_aa=%04x mode_ac=%04x t=%.9f\n',
            memory:read_u16(0x100a4), memory:read_u16(0x100a6),
            memory:read_u16(0x100a8), memory:read_u16(0x100aa),
            memory:read_u16(0x101c8), memory:read_u16(0x101ca),
            dsp.spaces['data']:read_u16(0x0866),
            dsp.spaces['data']:read_u16(0x00aa),
            dsp.spaces['data']:read_u16(0x00ac), now))
    end
    if now >= 28 and not late_captured then
        late_captured = true
        machine:logerror(string.format(
            'nse6_lcd_counts: data=%d nonzero=%d commands=%d t=%.9f\n',
            lcd_data_count, lcd_nonzero_count, lcd_command_count, now))
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
if dsp then
    local directory = assert(debug.getinfo(1, 'S').source:match('^@(.*/)'))
    dofile(directory .. 'c54x_rom4_mode_observe.lua')
end

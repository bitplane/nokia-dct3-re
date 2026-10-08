-- Own NSM-3 matrix already decoded from ROM; physical input only.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8210_observe_only = true
dofile(assert(source:match('^(.*[/])')) .. 'noki8210_menu_input.lua')
local machine = manager.machine
if os.getenv('NOKIA_DCT3_8210_PIN_ENTRY') == '1' then
    local cpu = machine.devices[':maincpu']
    local memory = cpu.spaces['program']
    _G.nsm3_readiness_constructor = memory:install_read_tap(0x2aede0, 0x2aede3,
        'nsm3_readiness_constructor', function(offset, value, mask)
            if cpu.state['PC'].value ~= 0x2aede0 then return end
            local input = cpu.state['R0'].value
            if input ~= 0x03ec and input ~= 0x03ed then return end
            machine:logerror(string.format('8210_readiness_constructor: input=%04x caller=%08x t=%.6f\n',
                input, cpu.state['R14'].value, machine.time:as_double()))
        end)
    _G.nsm3_readiness_sender = memory:install_read_tap(0x28845c, 0x28845f, 'nsm3_readiness_sender',
        function(offset, value, mask)
            if cpu.state['PC'].value ~= 0x28845c or cpu.state['R0'].value ~= 12 then return end
            local object = cpu.state['R1'].value
            if object < 0x100000 or object >= 0x17fffc then return end
            local input = memory:read_u16(object)
            if input ~= 0x03ec and input ~= 0x03ed then return end
            machine:logerror(string.format('8210_readiness_sender: input=%04x class=%02x caller=%08x t=%.6f\n',
                input, memory:read_u8(object + 3), cpu.state['R14'].value, machine.time:as_double()))
        end)
    _G.nsm3_cell_messages = memory:install_read_tap(0x2d5d80, 0x2d5d83, 'nsm3_cell_messages',
        function(offset, value, mask)
            if cpu.state['PC'].value ~= 0x2d5d80 then return end
            local caller = cpu.state['R14'].value
            if caller < 0x21b000 or caller >= 0x220000 then return end
            local object = cpu.state['R0'].value
            local context = cpu.state['R4'].value
            if object < 0x100000 or object >= 0x17fff4 or
                    context < 0x100000 or context >= 0x17fffc then return end
            local bytes = {}
            for index = 0, 11 do bytes[#bytes + 1] = string.format('%02x', memory:read_u8(object + index)) end
            machine:logerror(string.format('8210_cell_message: caller=%08x state=%04x data=%s t=%.6f\n',
                caller, memory:read_u16(context + 2), table.concat(bytes), machine.time:as_double()))
        end)
    _G.nsm3_cell_links = {}
    for _, address in ipairs({0x137238, 0x137240}) do
        _G.nsm3_cell_links[#_G.nsm3_cell_links + 1] = memory:install_write_tap(
            address, address + 3, 'nsm3_cell_link_' .. address,
            function(offset, value, mask)
                machine:logerror(string.format('8210_cell_link_write: address=%08x data=%08x mask=%08x pc=%08x caller=%08x t=%.6f\n',
                    offset, value, mask, cpu.state['PC'].value,
                    cpu.state['R14'].value, machine.time:as_double()))
            end)
    end
    _G.nsm3_cell_flags = {}
    local flags_pointer
    _G.nsm3_cell_flags_root = memory:install_write_tap(0x13722c, 0x13722f,
        'nsm3_cell_flags_root', function(offset, value, mask)
            local pointer = (memory:read_u32(0x13722c) & (~mask & 0xffffffff)) | (value & mask)
            if pointer == flags_pointer or pointer < 0x100000 or pointer >= 0x17fffc then return end
            flags_pointer = pointer
            _G.nsm3_cell_flags[#_G.nsm3_cell_flags + 1] = memory:install_write_tap(
                pointer, pointer + 3, 'nsm3_cell_flags_' .. pointer,
                function(address, data, written)
                    if memory:read_u32(0x13722c) ~= pointer then return end
                    machine:logerror(string.format('8210_cell_flags_write: address=%08x data=%08x mask=%08x pc=%08x caller=%08x t=%.6f\n',
                        address, data, written, cpu.state['PC'].value,
                        cpu.state['R14'].value, machine.time:as_double()))
                end)
        end)
    _G.nsm3_cell_decision = memory:install_read_tap(0x2a1380, 0x2a1383, 'nsm3_cell_decision',
        function(offset, value, mask)
            if cpu.state['PC'].value ~= 0x2a1380 then return end
            local state = memory:read_u32(0x2a16b8)
            machine:logerror(string.format('8210_cell_decision: caller=%08x argument=%02x state=%04x t=%.6f\n',
                cpu.state['R14'].value, cpu.state['R0'].value,
                memory:read_u16(state + 2), machine.time:as_double()))
        end)
    _G.nsm3_cell_predicates = {}
    for _, branch in ipairs({0x2a1934, 0x2a194c}) do
        _G.nsm3_cell_predicates[#_G.nsm3_cell_predicates + 1] = memory:install_read_tap(
            branch, branch + 3, 'nsm3_cell_predicate_' .. branch,
            function(offset, value, mask)
                if cpu.state['PC'].value ~= branch then return end
                local context = cpu.state['R4'].value
                if context < 0x100000 or context >= 0x17fff0 then return end
                local record = memory:read_u32(context + 4)
                if record < 0x100000 or record >= 0x17fffc then return end
                local flags = memory:read_u32(memory:read_u32(0x2a18f8))
                local left = memory:read_u32(memory:read_u32(0x2a1c14))
                local right = memory:read_u32(memory:read_u32(0x2a1c18))
                machine:logerror(string.format('8210_cell_predicates: branch=%08x argument=%02x flags=%08x record=%02x%02x%02x%02x left=%08x right=%08x t=%.6f\n',
                    branch, cpu.state['R6'].value, memory:read_u32(flags),
                    memory:read_u8(record), memory:read_u8(record + 1),
                    memory:read_u8(record + 2), memory:read_u8(record + 3),
                    left, right, machine.time:as_double()))
            end)
    end
    _G.nsm3_cell_trace = memory:install_read_tap(0x2d5dcc, 0x2d5dcf, 'nsm3_cell_trace',
        function(offset, value, mask)
            if cpu.state['PC'].value ~= 0x2d5dcc then return end
            local caller = cpu.state['R14'].value
            if caller < 0x21b000 or caller >= 0x220000 then return end
            local address = cpu.state['R0'].value
            if address < 0x200000 or address >= 0x400000 then return end
            local bytes = {}
            for index = 0, 159 do
                local byte = memory:read_u8(address + index)
                if byte == 0 then break end
                if byte < 32 or byte > 126 then byte = 32 end
                bytes[#bytes + 1] = string.char(byte)
            end
            machine:logerror(string.format('8210_cell_trace: caller=%08x text=%s t=%.6f\n',
                caller, table.concat(bytes), machine.time:as_double()))
        end)
    _G.nsm3_pin_route = memory:install_read_tap(0x2df484, 0x2df487, 'nsm3_pin_route',
        function(offset, value, mask)
            if cpu.state['PC'].value ~= 0x2df484 then return end
            local enabled = memory:read_u32(0x2df4f4)
            machine:logerror(string.format('8210_pin_measurement_route: enabled=%02x message=%08x t=%.6f\n',
                memory:read_u8(enabled), cpu.state['R0'].value, machine.time:as_double()))
        end)
    _G.nsm3_pin_completion = memory:install_read_tap(0x2a2250, 0x2a2253, 'nsm3_pin_completion',
        function(offset, value, mask)
            if cpu.state['PC'].value ~= 0x2a2250 then return end
            machine:logerror(string.format('8210_pin_measurement_completion: message=%08x t=%.6f\n',
                cpu.state['R0'].value, machine.time:as_double()))
            -- Own completion selects its parser using context +4, not band alone.
            local context = memory:read_u32(memory:read_u32(0x2a2294))
            if context >= 0x100000 and context < 0x17fff8 then
                machine:logerror(string.format('8210_pin_measurement_context: address=%08x selector=%02x alternate=%08x t=%.6f\n',
                    context, memory:read_u8(context), memory:read_u32(context + 4),
                    machine.time:as_double()))
            end
        end)
end
local sequence = {
    {2, 'Keypad 1'}, {3, 'Keypad 2'}, {4, 'Keypad 3'},
    {2, 'Keypad 4'}, {3, 'Keypad 5'}, {1, 'Menu'},
}
local input = coroutine.create(function()
    if os.getenv('NOKIA_DCT3_8210_PIN_ENTRY') == '1' then
        if not emu.wait(8) then return end
        machine.screens[':screen']:snapshot('8210_pin_prompt.png')
        for _, item in ipairs({{2, 'Keypad 1'}, {3, 'Keypad 2'},
                               {4, 'Keypad 3'}, {2, 'Keypad 4'}, {1, 'Menu'}}) do
            local key = assert(machine.ioport.ports[':COL.' .. item[1]].fields[item[2]])
            machine:logerror(string.format('8210_pin_physical: key=%s t=%.6f\n',
                item[2], machine.time:as_double()))
            key:set_value(1)
            if not emu.wait(0.15) then key:set_value(0); return end
            key:set_value(0)
            if not emu.wait(0.85) then return end
        end
        if not emu.wait(3) then return end
    elseif not emu.wait(12) then return end
    for _, item in ipairs(sequence) do
        local key = assert(machine.ioport.ports[':COL.' .. item[1]].fields[item[2]])
        machine:logerror(string.format('8210_security_physical: key=%s t=%.6f\n',
            item[2], machine.time:as_double()))
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return end
        key:set_value(0)
        if not emu.wait(0.35) then return end
    end
    if not emu.wait(5) then return end
    machine.screens[':screen']:snapshot('8210_after_security.png')
    if _G.noki8210_security_only then return end
    local menu = assert(machine.ioport.ports[':COL.1'].fields['Menu'])
    menu:set_value(1)
    if not emu.wait(0.15) then menu:set_value(0); return end
    menu:set_value(0)
    if not emu.wait(2) then return end
    machine.screens[':screen']:snapshot('8210_security_then_menu.png')
end)
_G.noki8210_security_input = input
assert(coroutine.resume(input))

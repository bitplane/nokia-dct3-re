-- Own NSM-3 matrix already decoded from ROM; physical input only.
local source = debug.getinfo(1, 'S').source:sub(2)
_G.noki8210_observe_only = true
dofile(assert(source:match('^(.*[/])')) .. 'noki8210_menu_input.lua')
local machine = manager.machine
if os.getenv('NOKIA_DCT3_8210_PIN_ENTRY') == '1' then
    local cpu = machine.devices[':maincpu']
    local memory = cpu.spaces['program']
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

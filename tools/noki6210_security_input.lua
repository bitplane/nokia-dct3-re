-- Physical PIN entry against a PIN-enabled laboratory SIM, no phone NV edits.
local source = debug.getinfo(1, 'S').source:sub(2)
local pin_only = os.getenv('NOKIA_DCT3_6210_PIN_ENTRY') == '1'
if not pin_only then
    dofile(assert(source:match('^(.*[/])')) .. 'noki6210_staged_observe.lua')
end
local machine = manager.machine
local cpu = machine.devices[':maincpu']
local memory = cpu.spaces['program']
_G.npe3_pin_measurement_route = memory:install_read_tap(0x45835c, 0x45835f, 'npe3_pin_route',
    function(offset, value, mask)
        if cpu.state['PC'].value ~= 0x45835c then return end
        local enabled = memory:read_u32(0x4583cc)
        machine:logerror(string.format('6210_pin_measurement_route: enabled=%02x message=%08x t=%.6f\n',
            memory:read_u8(enabled), cpu.state['R0'].value, machine.time:as_double()))
    end)
_G.npe3_pin_measurement_post = memory:install_read_tap(0x3c22c0, 0x3c22c3, 'npe3_pin_post',
    function(offset, value, mask)
        if cpu.state['PC'].value ~= 0x3c22c0 then return end
        local message = cpu.state['R1'].value
        if message < 0x100000 or message >= 0x180000 or
                memory:read_u16(message) ~= 0x1802 or memory:read_u8(message+3) ~= 0x8b then return end
        machine:logerror(string.format('6210_pin_measurement_post: target=%02x message=%08x t=%.6f\n',
            cpu.state['R0'].value, message, machine.time:as_double()))
    end)
_G.npe3_pin_measurement_completion = memory:install_read_tap(0x3c9750, 0x3c9753, 'npe3_pin_completion',
    function(offset, value, mask)
        if cpu.state['PC'].value ~= 0x3c9750 then return end
        machine:logerror(string.format('6210_pin_measurement_completion: message=%08x t=%.6f\n',
            cpu.state['R0'].value, machine.time:as_double()))
    end)
local input = coroutine.create(function()
    if not emu.wait(8) then return end
    machine.screens[':screen']:snapshot('6210_pin_prompt.png')
    for _, step in ipairs({{2, 'Keypad 1'}, {3, 'Keypad 2'},
                          {4, 'Keypad 3'}, {2, 'Keypad 4'},
                          {1, 'Left Softkey / Menu'}}) do
        local key = assert(machine.ioport.ports[':COL.' .. step[1]].fields[step[2]])
        machine:logerror('6210_security_physical: action=' ..
            (step[1] == 1 and 'confirm' or step[2]) .. '\n')
        key:set_value(1)
        if not emu.wait(0.15) then key:set_value(0); return end
        key:set_value(0)
        if not emu.wait(0.85) then return end
    end
    if pin_only then return end
    if not emu.wait(20) then return end
    local menu = assert(machine.ioport.ports[':COL.1'].fields['Left Softkey / Menu'])
    machine:logerror('6210_security_physical: action=menu\n')
    menu:set_value(1)
    if not emu.wait(0.15) then menu:set_value(0); return end
    menu:set_value(0)
    if not emu.wait(2) then return end
    machine.screens[':screen']:snapshot('6210_security_then_menu.png')
end)
_G.noki6210_security_input = input
assert(coroutine.resume(input))

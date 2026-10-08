-- Only physical key cells are driven; firmware observations are read-only.
local machine = manager.machine
local cpu = machine.devices[':maincpu']
local memory = cpu.spaces['program']
_G.slow_pin_route = memory:install_read_tap(0x4649ac, 0x4649af, 'slow_pin_route',
    function(offset, value, mask)
        if cpu.state['PC'].value ~= 0x4649ac then return end
        machine:logerror(string.format('6250_pin_rssi_route: enable=%02x message=%08x t=%.6f\n',
            memory:read_u8(0x1721e0), cpu.state['R0'].value, machine.time:as_double()))
    end)
_G.slow_pin_post = memory:install_read_tap(0x3c348c, 0x3c348f, 'slow_pin_post',
    function(offset, value, mask)
        if cpu.state['PC'].value ~= 0x3c348c then return end
        local message = cpu.state['R1'].value
        if message < 0x100000 or message >= 0x180000 or
                memory:read_u16(message) ~= 0x1802 or memory:read_u8(message+3) ~= 0x8b then return end
        machine:logerror(string.format('6250_pin_radio_post: target=%02x class=8b caller=%08x message=%08x t=%.6f\n',
            cpu.state['R0'].value, cpu.state['R14'].value, message, machine.time:as_double()))
    end)
_G.slow_pin_completion = memory:install_read_tap(0x3cbe14, 0x3cbe17, 'slow_pin_completion',
    function(offset, value, mask)
        if cpu.state['PC'].value ~= 0x3cbe14 then return end
        machine:logerror(string.format('6250_pin_rssi_completion: caller=%08x message=%08x t=%.6f\n',
            cpu.state['R14'].value, cpu.state['R0'].value, machine.time:as_double()))
    end)
local function cell(column, row)
    for _, field in pairs(machine.ioport.ports[':COL.' .. column].fields) do
        if field.mask == (1 << row) then return field end
    end
    error('missing physical PIN key cell')
end
local input = coroutine.create(function()
    assert(emu.wait(8))
    machine.screens[':screen']:snapshot('6250_pin_prompt.png')
    for _, item in ipairs({{2, 1}, {3, 1}, {4, 1}, {2, 2}, {1, 1}}) do
        local key = cell(item[1], item[2])
        machine:logerror(string.format('6250_pin_physical: column=%d row=%d t=%.6f\n',
            item[1], item[2], machine.time:as_double()))
        key:set_value(1)
        assert(emu.wait(0.15))
        key:set_value(0)
        assert(emu.wait(0.85))
    end
    assert(emu.wait(19))
    machine.screens[':screen']:snapshot('6250_pin_idle.png')
end)
_G.noki6250_slow_pin_input = input
assert(coroutine.resume(input))

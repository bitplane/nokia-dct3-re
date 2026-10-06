-- MMIO self-conformance, not a MU4 handshake or firmware boot acceptance.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki5510_input_observe.lua')
local machine = manager.machine
local space = machine.devices[':maincpu'].spaces.program
local completed = false
emu.register_frame_done(function()
    if completed or machine.time:as_double() < 14 then return end
    completed = true
    assert(machine.system.name == 'nmp5hle')
    local direction = space:read_u8(0x2006a)
    local data = space:read_u8(0x2002a)
    local mask = space:read_u8(0x2006b)
    local pending = space:read_u8(0x2002b)
    assert(direction == 0x24, 'own mixed-port initialization missing')
    -- Firmware cannot run between these synchronous writes and restoration.
    space:write_u8(0x2006b, 0)
    space:write_u8(0x2002a, 0)
    assert((space:read_u8(0x2002a) & 0x24) == 0, 'output-low readback')
    space:write_u8(0x2002a, 0x24)
    assert((space:read_u8(0x2002a) & 0x24) == 0x24, 'output-high readback')
    assert(space:read_u8(0x2002b) == pending, 'output drive generated input IRQ')
    space:write_u8(0x2002a, data)
    space:write_u8(0x2006b, mask)
    assert(space:read_u8(0x2006a) == direction, 'direction changed')
    machine:logerror('5510_gpio_conformance: PASS output_mask=24 status_preserved=1\n')
end)

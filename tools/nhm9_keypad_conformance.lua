-- MMIO controller conformance, not firmware UI acceptance.
local machine = manager.machine
assert(machine.system.name == 'noki3350')
local space = machine.devices[':maincpu'].spaces.program
local keys = {0x3e,0x1a,0x0a,0x18,0x19, 0x3e,0x0f,1,2,0x0e,
    0x3e,0x3e,6,5,4, 0x3e,0x3e,9,8,7, 0x3e,3,0x0b,0x17,0x0c}
local phase, saved = 0, nil
local function field(tag, mask)
    for _, value in pairs(machine.ioport.ports[':' .. tag].fields) do
        if value.mask == mask then return value end
    end
    error('missing physical input ' .. tag .. '/' .. mask)
end
emu.register_frame_done(function()
    if phase == 3 or machine.time:as_double() < 0.5 then return end
    if phase == 1 then
        assert((space:read_u8(0x2002a) & 31) == 0x1d, 'Power column mismatch')
        field('PWR', 1):set_value(0)
        phase = 2
        return
    elseif phase == 2 then
        assert((space:read_u8(0x2002a) & 31) == 31, 'Power release mismatch')
        -- The MCU is in the measured ownership wait, which does not rewrite
        -- these registers during this two-frame conformance window.
        for address, value in pairs(saved) do space:write_u8(address, value) end
        machine:logerror('nhm9_keypad: PASS matrix_keys=18 scans=90 power_mask=02 ui_delivery=unvalidated\n')
        phase = 3
        return
    end
    saved = {}
    for _, address in ipairs({0x200a8, 0x20028, 0x2006b}) do
        saved[address] = space:read_u8(address)
    end
    space:write_u8(0x2006b, 31)
    space:read_u8(0x2002a)
    local count = 0
    for row = 0, 4 do
        for column = 0, 4 do
            if keys[row * 5 + column + 1] ~= 0x3e then
                local key = field('COL.' .. column, 1 << row)
                key:set_value(1)
                for scan = 0, 4 do
                    space:write_u8(0x200a8, 1 << scan)
                    space:write_u8(0x20028, (~(1 << scan)) & 255)
                    local expected = scan == row and (31 & (~(1 << column))) or 31
                    assert((space:read_u8(0x2002a) & 31) == expected,
                        string.format('matrix row=%d column=%d scan=%d', row, column, scan))
                end
                key:set_value(0)
                count = count + 1
            end
        end
    end
    assert(count == 18)
    space:write_u8(0x200a8, 0xe0)
    space:write_u8(0x20028, 31)
    field('PWR', 1):set_value(1)
    phase = 1
end, 'NHM-9 keypad controller conformance')

-- Controller conformance: MMIO row drive plus physical host inputs, not UI acceptance.
local machine = manager.machine
local space = machine.devices[":maincpu"].spaces.program
local table_keys = {0x5a,0x0e,0x19,0x1a,0x0c,0x5a,0x0f,0x0a,0x0b,0x06,
    0x5a,0x01,0x12,0x04,0x07,0x5a,0x02,0x05,0x08,0x09,0x5a,0x03,0x5a,0x5a,0x5a}
local phase, saved, completed = 0, nil, false
local function field(tag, mask)
    for _, f in pairs(machine.ioport.ports[":" .. tag].fields) do
        if f.mask == mask then return f end
    end
    error("missing physical input " .. tag .. "/" .. mask)
end
emu.register_frame_done(function()
    if phase ~= 0 then
        local actual = space:read_u8(0x2002a) & 31
        if phase == 1 then
            assert(actual == 0x1d, "Power must pull special column bit 1 low")
            field("PWR", 1):set_value(0)
            phase = 2
        else
            assert(actual == 31, "Power did not release")
            space:write_u8(0x200a8, saved.direction)
            space:write_u8(0x20028, saved.row)
            space:write_u8(0x2006b, saved.mask)
            machine:logerror("nse5_keypad: PASS matrix_keys=17 scans=85 power_mask=02\n")
            phase = 0
        end
        return
    end
    if completed or machine.time:as_double() < 0.5 then return end
    completed = true
    assert(machine.system.name == "noki7110")
    saved = {direction=space:read_u8(0x200a8), row=space:read_u8(0x20028), mask=space:read_u8(0x2006b)}
    space:write_u8(0x2006b, 31)
    space:read_u8(0x2002a) -- Consume the cold-start indication.
    local count = 0
    for row = 0, 4 do
        for column = 1, 4 do
            if table_keys[row * 5 + column + 1] ~= 0x5a then
                local key = field("COL." .. column, 1 << row)
                key:set_value(1)
                for scan = 0, 4 do
                    space:write_u8(0x200a8, 1 << scan)
                    space:write_u8(0x20028, (~(1 << scan)) & 255)
                    local expected = scan == row and (31 & (~(1 << column))) or 31
                    assert((space:read_u8(0x2002a) & 31) == expected,
                        string.format("matrix row=%d column=%d scan=%d", row, column, scan))
                end
                key:set_value(0)
                count = count + 1
            end
        end
    end
    assert(count == 17)
    space:write_u8(0x200a8, 0xe0)
    space:write_u8(0x20028, 31)
    field("PWR", 1):set_value(1)
    phase = 1
end, "NSE-5 keypad controller conformance")

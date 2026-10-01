-- Controller conformance: MMIO row drive plus physical host inputs, not UI acceptance.
local machine = manager.machine
local space = machine.devices[":maincpu"].spaces.program
local table_keys = {0x5a,0x0e,0x19,0x1a,0x0c,0x5a,0x0f,0x0a,0x0b,0x06,
    0x5a,0x01,0x12,0x04,0x07,0x5a,0x02,0x05,0x08,0x09,0x5a,0x03,0x5a,0x5a,0x5a}
local phase, saved, completed = 0, nil, false
local roller_phase, roller_saved = 0, nil
local function field(tag, mask)
    for _, f in pairs(machine.ioport.ports[":" .. tag].fields) do
        if f.mask == mask then return f end
    end
    error("missing physical input " .. tag .. "/" .. mask)
end
emu.register_frame_done(function()
    if roller_phase ~= 0 then
        local masks = {2, 1, 32}
        local patterns = {{1,1,1,0,1,0}, {1,0,1,1,0,1}, {0,1,0,1,1,1}}
        local observed = {}
        for drive = 1, 3 do
            for pin = 1, 3 do
                space:write_u8(0x20030 + pin, drive == pin and 0 or masks[pin])
                space:write_u8(0x200b0 + pin, drive == pin and 0 or masks[pin])
            end
            for pin = 1, 3 do
                if pin ~= drive then
                    local level = (space:read_u8(0x200f0 + pin) & masks[pin]) ~= 0 and 1 or 0
                    observed[#observed + 1] = level
                end
            end
        end
        for i = 1, 6 do
            assert(observed[i] == patterns[roller_phase][i], "roller low-drive probe mismatch")
        end
        for previous = 1, 3 do
            for pin = 1, 3 do
                space:write_u8(0x20030 + pin, pin == previous and 0 or masks[pin])
                space:write_u8(0x200b0 + pin, pin == previous and 0 or masks[pin])
            end
            for pin = 1, 3 do
                local expected
                if previous == roller_phase then expected = pin ~= previous
                else expected = pin == roller_phase end
                assert(((space:read_u8(0x200f0 + pin) & masks[pin]) ~= 0) == expected,
                    "roller restored-drive phase mismatch")
            end
        end
        if roller_phase == 3 then
            for address, value in pairs(roller_saved) do space:write_u8(address, value) end
            field("ROLLER", 3):set_value(0)
            roller_phase = 0
            machine:logerror("nse5_roller: PASS positions=3 probes=18 restored_pairs=9 irq_delivery=unvalidated\n")
        else
            roller_phase = roller_phase + 1
            field("ROLLER", 3):set_value(roller_phase - 1)
        end
        return
    end
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
            roller_saved = {}
            for pin = 1, 3 do
                for _, base in ipairs({0x20030, 0x200b0}) do
                    roller_saved[base + pin] = space:read_u8(base + pin)
                end
            end
            field("ROLLER", 3):set_value(0)
            roller_phase = 1
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

-- Controller conformance only: MMIO row drive plus physical host inputs.
-- This does not claim that the parked firmware scans or handles these keys.
local machine = manager.machine
local space = machine.devices[":maincpu"].spaces["program"]
local done = false
local power_phase = 0
local saved = nil
local function field(tag, mask)
    for _, f in pairs(machine.ioport.ports[":" .. tag].fields) do
        if f.mask == mask then return f end
    end
    error("missing input " .. tag .. "/" .. mask)
end
emu.register_frame_done(function()
    if power_phase ~= 0 then
        local actual = space:read_u8(0x2002a) & 0x1f
        if power_phase == 1 then
            assert(actual == 0x0f, string.format("wrong Power special-scan bit: %02x", actual))
            field("PWR", 1):set_value(0)
            power_phase = 2
        else
            assert(actual == 0x1f, "Power did not release")
            space:write_u8(0x200a8, saved.direction)
            space:write_u8(0x20028, saved.row)
            space:write_u8(0x2006b, saved.mask)
            machine:logerror("npe3_keypad: PASS matrix_keys=20 scans=100 power_mask=10\n")
            power_phase = 0
        end
        return
    end
    if done or machine.time:as_double() < 0.5 then return end
    done = true
    assert(machine.system.name == "noki6210")
    local row = space:read_u8(0x20028)
    local direction = space:read_u8(0x200a8)
    local mask = space:read_u8(0x2006b)
    saved = {row=row, direction=direction, mask=mask}
    space:write_u8(0x2006b, 0x1f)
    -- Consume the cold-start indication before testing held physical keys.
    space:read_u8(0x2002a)
    local count = 0
    for drive = 1, 4 do
        for sense = 0, 4 do
            local f = field("COL." .. sense, 1 << drive)
            f:set_value(1)
            for scan = 0, 4 do
                space:write_u8(0x200a8, 1 << scan)
                space:write_u8(0x20028, (~(1 << scan)) & 0xff)
                local actual = space:read_u8(0x2002a) & 0x1f
                local expected = scan == drive and (0x1f & (~(1 << sense))) or 0x1f
                assert(actual == expected, string.format(
                    "matrix drive=%d sense=%d scan=%d actual=%02x expected=%02x",
                    drive, sense, scan, actual, expected))
            end
            f:set_value(0)
            count = count + 1
        end
    end
    space:write_u8(0x200a8, 0xe0)
    space:write_u8(0x20028, 0x1f)
    local power = field("PWR", 1)
    power:set_value(1)
    assert(count == 20)
    -- The direct ioport callback samples Power on the next input frame.
    power_phase = 1
end, "NPE-3 keypad controller conformance")

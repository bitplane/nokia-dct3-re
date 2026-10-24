-- Physical Phone-memory save; the SIM already holds the distinct A/12345 control.
_G.nsm1_followup_contacts = {}
local directory = assert(debug.getinfo(1, 'S').source:match('^@(.*/)'))
dofile(directory .. 'nsm1_security_input.lua')
local machine = manager.machine
local memory = machine.devices[':maincpu'].spaces.program
local function snapshot(phase)
    machine.screens[':screen']:snapshot('nsm1_phone_local_' .. phase .. '.png')
end
local function key(column, mask, code)
    local field
    for _, candidate in pairs(machine.ioport.ports[':COL.' .. column].fields) do
        if candidate.mask == mask then field = candidate end
    end
    assert(field)
    local row = 0
    while (1 << row) ~= mask do row = row + 1 end
    assert(memory:read_u8(0x2e0b18 + row * 5 + column) == code)
    field:set_value(1)
    machine:logerror(string.format('nsm1_phone_local_key: key=%02x down=1\n', code))
    assert(emu.wait(0.2))
    field:set_value(0)
    machine:logerror(string.format('nsm1_phone_local_key: key=%02x down=0\n', code))
    assert(emu.wait(0.6))
end
local input = coroutine.create(function()
    assert(emu.wait(15))
    machine:logerror('nsm1_phone_local: mode=write begin=1\n')
    key(1, 16, 0x1a)
    for index = 1, 4 do key(1, 8, 0x18) end
    key(1, 2, 0x19)
    snapshot('options')
    key(1, 2, 0x19)
    snapshot('choices')
    key(1, 8, 0x18)
    snapshot('choice_phone')
    key(1, 2, 0x19)
    assert(emu.wait(2))
    snapshot('selected')
    key(0, 8, 0x0f)
    assert(emu.wait(1))
    key(1, 16, 0x1a)
    key(1, 8, 0x18)
    key(1, 2, 0x19)
    key(4, 2, 3)
    assert(emu.wait(1))
    snapshot('name')
    key(1, 2, 0x19)
    for _, cell in ipairs({{4,4,6}, {2,8,7}, {3,8,8}, {4,8,9}}) do key(table.unpack(cell)) end
    snapshot('number')
    key(1, 2, 0x19)
    assert(emu.wait(2))
    -- The save notice has expired; this is the new selected D row, not a notice oracle.
    snapshot('saved')
    machine:logerror('nsm1_phone_local: mode=write complete=1\n')
end)
_G.nsm1_phone_local_input = input
assert(coroutine.resume(input))

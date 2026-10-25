-- Own physical authentication and passive Toolkit wire capture.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
_G.nsm1_correct_pin = true
dofile(directory .. 'nsm1_pin_input.lua')
-- A separately qualified factory-reset bank no longer requests the phone code.
-- Keep the original locked-bank input sequence as the default.
if not _G.nsm1_toolkit_skip_phone_authentication then
    _G.nsm1_phone_code_start = 15
    _G.nsm1_followup_contacts = {}
    dofile(directory .. 'nsm1_security_input.lua')
end
local machine = manager.machine
local memory = machine.devices[':maincpu'].spaces.program
local counts = {TX=0, RX=0}
local wire_limit = _G.nsm1_toolkit_wire_limit or 8192
local function wire(offset, data, mask, direction, address)
    for lane = 0, 3 do
        local shift = (3-lane)*8
        if offset+lane == address and mask & (255 << shift) ~= 0 then
            counts[direction] = counts[direction]+1
            assert(counts[direction] <= wire_limit, 'Toolkit wire capture limit exceeded')
            machine:logerror(string.format(
                'nsm1_sim_wire: direction=%s data=%02x t=%.9f observer_writes=0\n',
                direction, (data >> shift) & 255, machine.time:as_double()))
        end
    end
end
_G.nsm1_toolkit_tx = memory:install_write_tap(0x20034,0x20037,'nsm1_toolkit_tx',
    function(offset,data,mask) wire(offset,data,mask,'TX',0x20036) end)
_G.nsm1_toolkit_rx = memory:install_read_tap(0x20034,0x20037,'nsm1_toolkit_rx',
    function(offset,data,mask) wire(offset,data,mask,'RX',0x20037) end)
if _G.nsm1_toolkit_observe_only then return end
local actions = coroutine.create(function()
    assert(emu.wait(65))
    machine.screens[':screen']:snapshot('nsm1_toolkit_before.png')
    assert(emu.wait(3))
    assert(memory:read_u8(0x2e0b1e) == 0x19, 'own Menu contact differs')
    local field
    for _, candidate in pairs(machine.ioport.ports[':COL.1'].fields) do
        if candidate.mask == 2 then field = candidate end
    end
    assert(field, 'own physical Menu contact unavailable')
    field:set_value(1)
    machine:logerror('nsm1_toolkit_physical: action=dismiss down=1 t=68.000000000\n')
    assert(emu.wait(.2))
    field:set_value(0)
    machine:logerror('nsm1_toolkit_physical: action=dismiss down=0 t=68.200000000\n')
    assert(emu.wait(2))
    machine.screens[':screen']:snapshot('nsm1_toolkit_after.png')
end)
assert(coroutine.resume(actions))

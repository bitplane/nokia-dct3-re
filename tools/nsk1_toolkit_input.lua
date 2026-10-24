-- Physical phone authentication and passive external Toolkit wire capture.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
_G.nsk1_security_code_only = true
dofile(directory .. 'nsk1_security_fixture.lua')
local machine = manager.machine
local memory = machine.devices[':maincpu'].spaces.program
local dsp = assert(machine.devices[':dsp_c54x:cpu'])
local function snapshot(label, index)
    machine.screens[':screen']:snapshot(label)
    machine:logerror(string.format('nsk1_toolkit_native: sample=%d pc=%06x stopped=%d idle=%d observer_writes=0\n',
        index, dsp.state.PC.value, dsp.state.ILLEGAL.value, dsp.state.IDLE.value))
end
local counts = {TX=0, RX=0}
local function wire(offset, data, mask, direction, address)
    for lane = 0, 3 do
        local shift = (3-lane)*8
        if offset+lane == address and mask & (255 << shift) ~= 0 then
            counts[direction] = counts[direction]+1
            assert(counts[direction] <= 8192, 'Toolkit wire capture limit exceeded')
            machine:logerror(string.format(
                'nsk1_sim_wire: direction=%s data=%02x t=%.9f observer_writes=0\n',
                direction, (data >> shift) & 255, machine.time:as_double()))
        end
    end
end
_G.nsk1_toolkit_tx = memory:install_write_tap(0x20034,0x20037,'nsk1_toolkit_tx',
    function(offset,data,mask) wire(offset,data,mask,'TX',0x20036) end)
_G.nsk1_toolkit_rx = memory:install_read_tap(0x20034,0x20037,'nsk1_toolkit_rx',
    function(offset,data,mask) wire(offset,data,mask,'RX',0x20037) end)
local actions = coroutine.create(function()
    assert(emu.wait(65))
    snapshot('nsk1_toolkit_before.png', 1)
    assert(emu.wait(3))
    local field
    for _, candidate in pairs(machine.ioport.ports[':COL.1'].fields) do
        if candidate.mask == 4 then field = candidate end
    end
    assert(field, 'own physical Menu contact unavailable')
    field:set_value(1)
    machine:logerror('nsk1_toolkit_physical: action=dismiss down=1 t=68.000000000\n')
    assert(emu.wait(.2))
    field:set_value(0)
    machine:logerror('nsk1_toolkit_physical: action=dismiss down=0 t=68.200000000\n')
    assert(emu.wait(2))
    snapshot('nsk1_toolkit_after.png', 2)
end)
assert(coroutine.resume(actions))
